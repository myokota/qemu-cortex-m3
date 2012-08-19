/*
 * NXP LPC1768 Board support
 *
 * Copyright (c) 2011,2012 Masashi YOKOTA <yktmss@gmail.com>
 *
 * This code is licensed under the GPL v2.
 */

#include "sysbus.h"
#include "arm-misc.h"
#include "devices.h"
#include "boards.h"
#include "qemu-timer.h"
#include "exec-memory.h"

static void lpc1768_common_init(const char *kernel_filename, const char *cpu_model)
{
    MemoryRegion *sysmem = get_system_memory();
    MemoryRegion *ex_sram;
    qemu_irq *pic;
    int sram_size;
    int flash_size;

    flash_size = 512 * 1024;
    sram_size  = 32 * 1024;
    pic = armv7m_init(sysmem, flash_size, sram_size, 0x10000000,
                      kernel_filename, cpu_model);

    sram_size = 16 * 1024 * 2; /* 2 blocks of 16 KiB */
    ex_sram = g_new(MemoryRegion, 1);
    memory_region_init_ram(ex_sram, "armv7m.AHB-SRAM", sram_size);
    memory_region_add_subregion(sysmem, 0x2007c000, ex_sram);

    sysbus_create_simple("lpc1768,uart", 0x4000C000, pic[5]); // 21 - (16) = 5
    sysbus_create_simple("lpc1768,sysc", 0x400FC000, NULL);
}

/* FIXME: Figure out how to generate these from lpc1768_boards.  */
static void lpc1768_generic_init(ram_addr_t ram_size,
                     const char *boot_device,
                     const char *kernel_filename, const char *kernel_cmdline,
                     const char *initrd_filename, const char *cpu_model)
{
    lpc1768_common_init(kernel_filename, cpu_model);
}

static QEMUMachine lpc1768_generic_machine = {
    .name = "lpc1768_generic",
    .desc = "LPC1768 Generic board",
    .init = lpc1768_generic_init,
};

static void lpc1768_machine_init(void)
{
    qemu_register_machine(&lpc1768_generic_machine);
}

machine_init(lpc1768_machine_init);
