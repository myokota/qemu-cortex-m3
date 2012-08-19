/*
 * NXP LPC1768 UART support
 *
 * Copyright (c) 2011 Masashi YOKOTA <yktmss@gmail.com>
 *
 * This code is licensed under the GPL.
 */

#include "sysbus.h"
#include "devices.h"
#include "qemu-char.h"

//#define DEBUG_SERIAL
#ifdef DEBUG_SERIAL
static int dbg_cnt = 0;
#define DPRINTF(fmt, ...) \
do { fprintf(stderr, "%s[%010d]:" fmt , __func__, ++dbg_cnt, ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) do {} while(0)
#endif

#define UART_RBR_DLL            (0x00)   
#define UART_THR_DLL            (0x00)   
#define UART_IER_DLM            (0x04)   
#define UART_IIR                (0x08)   
#define UART_FCR                (0x08)   
#define UART_LCR                (0x0C)   
#define UART_MCR                (0x10)   
#define UART_LSR                (0x14)   
#define UART_MSR                (0x18)   
#define UART_SCR                (0x1C)   

#define UART_IER_RBR            (0x01)       
#define UART_IER_THRE           (0x02)       
#define UART_IIR_RDA            (2 << 1)
#define UART_IIR_THRE           (1 << 1)

#define UART_LCR_DLAB            (0x80)       
#define UART_LCR_NP_8_1          (0x03)       
#define UART_FCR_FIFO_DISABLE    (0x00)

#define UART_LSR_RX_DATA_READY   (0x01)
#define UART_LSR_TX_EMPTY        (0x20)
#define UART_MCR_INT_ENABLE      (0x08)

typedef struct {
    SysBusDevice busdev;
    CharDriverState *chr;
    MemoryRegion mmio;
    uint8_t fifo[32];
    uint32_t fifo_index;

    uint32_t dll;
    uint32_t dlm;
    uint32_t fcr;
    uint32_t lcr;
    uint32_t lsr;
    uint32_t ier;
    uint32_t iir;

    uint32_t irq_level;
    qemu_irq irq;
} Lpc1768UartState;

static void lpc1768_uart_update(Lpc1768UartState *s)
{
    uint32_t level = 0;

    if (s->ier & UART_IER_RBR) {
        level = !!(s->iir & UART_IIR_RDA);
    }

    if (s->ier & UART_IER_THRE) {
        level = !!(s->iir & UART_IIR_THRE);
    }

    DPRINTF("");
    if (level)
        qemu_irq_pulse(s->irq);
}

static uint64_t lpc1768_uart_read(void *opaque, target_phys_addr_t offset,
				  unsigned size)
{
    Lpc1768UartState *s = (Lpc1768UartState *)opaque;
    uint32_t retval = 0;

    switch (offset) {
    case UART_RBR_DLL:
        if (s->lcr & UART_LCR_DLAB) {
            ;
        } else {
            s->lsr &= ~UART_LSR_RX_DATA_READY;
            s->iir &= ~UART_IIR_RDA;
            retval = (uint32_t)s->fifo[0];
        }
        break;
    case UART_IER_DLM:
        if (s->lcr & UART_LCR_DLAB) {
            retval = s->dlm;
        } else {
            retval = s->ier;
        }
        break;
    case UART_IIR:
        retval = s->iir;
        s->iir = 1;
        break;
    case UART_LSR:
        retval = s->lsr | UART_LSR_TX_EMPTY;
        break;
    default:
        cpu_abort(cpu_single_env, "%s: Bad offset 0x%x\n", __func__, 
			(int)offset);
    }

    DPRINTF("0x%08x ---> 0x%08x \n",  offset, retval);
    return retval;
}

static void lpc1768_uart_write(void *opaque, target_phys_addr_t offset,
                               uint64_t value, unsigned size)
{
    Lpc1768UartState *s = (Lpc1768UartState *)opaque;

    DPRINTF("0x%08x <--- 0x%08x\n", offset, value);
    switch (offset) {
    case UART_THR_DLL:
        if (s->lcr & UART_LCR_DLAB) {
            s->dll = value;
        } else {
            uint8_t ch = value & 0xff;
            qemu_chr_fe_write(s->chr, &ch, 1);
            s->iir |= UART_IIR_THRE;
            s->iir &= ~1;
            s->iir &= ~UART_IIR_THRE;
            lpc1768_uart_update(s);
        }
        break;
    case UART_IER_DLM:
        if (s->lcr & UART_LCR_DLAB) {
            s->dlm = value;
        } else {
            s->ier = value;
        }
        break;
    case UART_FCR:
        s->fcr = value;
        break;
    case UART_LCR:
        s->lcr = value;
        break;
    default:
        cpu_abort(cpu_single_env, "%s: Bad offset 0x%x\n", __func__, 
			(int)offset);
    }
}

static const MemoryRegionOps lpc1768_uart_mem_ops = {
    .read = lpc1768_uart_read,
    .write = lpc1768_uart_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void lpc1768_uart_rx(void *opaque, const uint8_t *buf, int size)
{
    Lpc1768UartState *s = opaque;

    s->fifo[0] = *buf;
    s->lsr |= UART_LSR_RX_DATA_READY;
    s->iir |= UART_IIR_RDA;
    s->iir &= ~1;
    lpc1768_uart_update(s);
}

static int lpc1768_uart_can_rx(void *opaque)
{
    Lpc1768UartState *s = opaque;

    if (s->lsr % UART_LSR_RX_DATA_READY)
        return 0;
    else
        return 1; /* bytes */
}

static void lpc1768_uart_event(void *opaque, int event)
{
}

static void lpc1768_uart_reset(DeviceState *d)
{
    Lpc1768UartState *s = container_of(d, Lpc1768UartState, busdev.qdev);
    /* defaults */
    s->dll = 0x01;
    s->dlm = 0x00;
    s->ier = 0x00;
    s->iir = 0x01;
    s->fcr = 0x00;
    s->lcr = 0x00;
    s->lsr = 0x60;
    memset(s->fifo, 0, sizeof(s->fifo));
    s->fifo_index = 0;
}

static int lpc1768_uart_init(SysBusDevice *dev)
{
    Lpc1768UartState *s = FROM_SYSBUS(typeof(*s), dev);

    sysbus_init_irq(dev, &s->irq);
    memory_region_init_io(&s->mmio, &lpc1768_uart_mem_ops, s,
		    	  "lpc1768_uart-mmio", 0x1000);
    sysbus_init_mmio(dev, &s->mmio);

    s->chr = qemu_char_get_next_serial();
    if (s->chr) {
        qemu_chr_add_handlers(s->chr, lpc1768_uart_can_rx, 
                              lpc1768_uart_rx, lpc1768_uart_event, s);
    }

    return 0;
}

static void lpc1768_uart_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = lpc1768_uart_init;
    dc->reset = lpc1768_uart_reset;
}

static TypeInfo lpc1768_uart_info = {
    .name  = "lpc1768,uart",
    .parent  = TYPE_SYS_BUS_DEVICE,
    .instance_size  = sizeof(Lpc1768UartState),
    .class_init  = &lpc1768_uart_class_init,
};

static void lpc1768_register_type(void)
{
    type_register_static(&lpc1768_uart_info);
}

type_init(lpc1768_register_type)
