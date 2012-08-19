/*
 * NXP LPC1768 System Control support
 *
 * Copyright (c) 2011 Masashi YOKOTA <yktmss@gmail.com>
 *
 * This code is licensed under the GPL.
 */

#include "sysbus.h"
#include "devices.h"
#include "arm-misc.h"

#define SYSC_FLASHCFG   (0x000)

#define SYSC_PLL0CON    (0x080)
#define SYSC_PLL0CFG    (0x084)    
#define SYSC_PLL0STAT   (0x088)
#define SYSC_PLL0FEED   (0x08C)

#define SYSC_PLL1CON    (0x0A0)
#define SYSC_PLL1CFG    (0x0A4)
#define SYSC_PLL1STAT   (0x0A8)
#define SYSC_PLL1FEED   (0x0AC)

#define SYSC_PCON
#define SYSC_PCONP

#define SYSC_CCLKCFG    (0x104)
#define SYSC_USBCLKCFG  (0x108)
#define SYSC_CLKSRCSEL  (0x10C)

#define SYSC_EXTINT
#define SYSC_RESERVED5
#define SYSC_EXTMODE
#define SYSC_EXTPOLAR

#define SYSC_RSID

#define SYSC_SCS
#define SYSC_IRCTRIM
#define SYSC_PCLKSEL0   (0x1A8)
#define SYSC_PCLKSEL1   (0x1AC)

#define SYSC_USBIntSt
#define SYSC_DMAREQSEL
#define SYSC_CLKOUTCFG  (0x1C8)
        
enum {
    SRCSEL_INTERNAL_CR,
    SRCSEL_MAIN_OSC,
    SRCSEL_RTC,
};

typedef struct {
    SysBusDevice busdev;
    MemoryRegion mmio;
    uint32_t main_clk_hz;
    uint32_t clk_config;
    uint32_t clk_select;
    uint32_t pll0_config;
    uint32_t pll0_control;
} Lpc1768SyscState;

static const char * get_clock_name(Lpc1768SyscState *s)
{
    switch (s->clk_select & 3) {
        case SRCSEL_INTERNAL_CR:
            return "Internal CR";
        case SRCSEL_MAIN_OSC:
            return "Main OSC";
        case SRCSEL_RTC:
            return "RTC";
    }
    return "(unknown clock source)";
}

static uint32_t get_pll0(Lpc1768SyscState *s)
{
    uint32_t n = ((s->pll0_config >> 16) & 0xffff) + 1;
    uint32_t m = (s->pll0_config & 0xffff) + 1;

    return (2 * m * s->main_clk_hz) / n;
}

static void update_system_clock(Lpc1768SyscState *s)
{
    if ((s->clk_select & 3) != SRCSEL_INTERNAL_CR) {
        printf("unsupported clock source :%s\n", 
               get_clock_name(s));
        printf("assuming that the system clock is at 100MHz\n");
        system_clock_scale = 100000000;
        return;
    }
    
    system_clock_scale = get_pll0(s) / ((s->clk_config & 0xff) + 1);
}

static uint64_t lpc1768_sysc_read(void *opaque, target_phys_addr_t offset,
		 		  unsigned size)
{
    Lpc1768SyscState *s = (Lpc1768SyscState *)opaque;
    uint32_t retval = 0;

    switch (offset) {
    case SYSC_CCLKCFG:
        retval = s->clk_config;
        break;
    case SYSC_CLKSRCSEL:
        retval = s->clk_select;
        break;
    case SYSC_PLL0CFG:
        retval = s->pll0_config;
        break;
    case SYSC_PLL0CON:
        retval = s->pll0_control;
        break;
    case SYSC_PLL0STAT:
        retval = (7<<24);
        break;
    }
    return retval;
}

static void lpc1768_sysc_write(void *opaque, target_phys_addr_t offset,
                               uint64_t value, unsigned size)
{
    Lpc1768SyscState *s = (Lpc1768SyscState *)opaque;
    switch (offset) {
    case SYSC_CCLKCFG:
        s->clk_config = value;
        break;
    case SYSC_CLKSRCSEL:
        s->clk_select = value;
        break;
    case SYSC_PLL0CFG:
        s->pll0_config = value;
        break;
    case SYSC_PLL0CON:
        s->pll0_control = value;
        break;
    default:
        return;
    }
    update_system_clock(s);
}

static const MemoryRegionOps lpc1768_sysc_mem_ops = {
    .read = lpc1768_sysc_read,
    .write = lpc1768_sysc_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static int lpc1768_sysc_init(SysBusDevice *dev)
{
    Lpc1768SyscState *s = FROM_SYSBUS(Lpc1768SyscState, dev);

    memory_region_init_io(&s->mmio, &lpc1768_sysc_mem_ops, s,
		    	  "lpc1768_sysc-mmio", 0x1000);
    sysbus_init_mmio(dev, &s->mmio);

    return 0;
}

static void lpc1768_sysc_reset(DeviceState *d)
{
}

static Property lpc1768_sysc_properties[] = {
    DEFINE_PROP_UINT32("main_clk_hz", Lpc1768SyscState, main_clk_hz, 4000000),
    DEFINE_PROP_END_OF_LIST(),
};
static void lpc1768_sysc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = lpc1768_sysc_init;
    dc->reset = lpc1768_sysc_reset;
    dc->props = lpc1768_sysc_properties;
}

static TypeInfo lpc1768_sysc_info = {
    .name  = "lpc1768,sysc",
    .parent  = TYPE_SYS_BUS_DEVICE,
    .instance_size  = sizeof(Lpc1768SyscState),
    .class_init  = &lpc1768_sysc_class_init,
};

static void lpc1768_register_type(void)
{
    type_register_static(&lpc1768_sysc_info);
}

type_init(lpc1768_register_type)
