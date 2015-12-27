/*
 * linux/arch/arm/mach-ne1/core.c
 *
 * Copyright (C) NEC Electronics Corporation 2007, 2008
 *
 * This file is based on arch/arm/mach-realview/core.c
 *
 * Copyright (C) 1999 - 2003 ARM Limited
 * Copyright (C) 2000 Deep Blue Solutions Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/sysdev.h>
#include <linux/interrupt.h>
#include <linux/serial_8250.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>
#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/leds.h>

#include <asm/mach/arch.h>
#include <asm/mach/flash.h>
#include <asm/mach/irq.h>
#include <asm/mach/time.h>
#include <asm/mach/map.h>

//#include <asm/mach/mmc.h>

#include <asm/hardware/gic.h>

#include <mach/ne1_timer.h>
#include <mach/ne1tb.h>
#include <mach/hardware.h>
#include <mach/platform.h>

#include <linux/smsc911x.h>

#include "core.h"
#include "clock.h"

#ifdef    CONFIG_NKERNEL

#include <nk/nkern.h>

#else

#define NE1xB_REFCOUNTER		(IO_ADDRESS(NE1xB_ETH_BASE + 0x9c))

/*
 * This is the NE1-xB sched_clock implementation using the 
 * LAN9118 FREE_RUN (Free-Run 25MHz Counter).  Resolution of
 * 40 ns, and a maximum value of about 172 s.
 */
unsigned long long sched_clock(void)
{
	unsigned long long v;

	v = ((unsigned long long)readl(NE1xB_REFCOUNTER)) * 40;

	return v;
}

#endif /* CONFIG_NKERNEL */

/* ---- NE1-TB UART ---- */
static struct plat_serial8250_port ne1xb_serial_ports[] = {
	[0] = {
		.membase	= (char *)IO_ADDRESS(NE1_BASE_UART_0),
		.mapbase	= (unsigned long)NE1_BASE_UART_0,
		.irq		= IRQ_UART_0,
		.flags		= UPF_BOOT_AUTOCONF,
		.iotype		= UPIO_MEM,
		.regshift	= 2,
		.uartclk	= 66666000*2,
	},
	[1] = {
		.membase	= (char *)IO_ADDRESS(NE1_BASE_UART_1),
		.mapbase	= (unsigned long)NE1_BASE_UART_1,
		.irq		= IRQ_UART_1,
		.flags		= UPF_BOOT_AUTOCONF,
		.iotype		= UPIO_MEM,
		.regshift	= 2,
		.uartclk	= 66666000*2,
	},
	[2] = {
		.membase	= (char *)IO_ADDRESS(NE1_BASE_UART_2),
		.mapbase	= (unsigned long)NE1_BASE_UART_2,
		.irq		= IRQ_UART_2,
		.flags		= UPF_BOOT_AUTOCONF,
		.iotype		= UPIO_MEM,
		.regshift	= 2,
		.uartclk	= 66666000*2,
	},
	[3] = {
		.flags		= 0,
	},
};

static struct platform_device ne1xb_serial_device = {
	.name           = "serial8250",
	.id             = 0,
	.dev            = {
		.platform_data  = ne1xb_serial_ports,
	},
	.num_resources  = 0,
	.resource       = NULL,
};


/* ---- NE1-TB NOR Flash ROM ---- */
static struct resource ne1xb_flash_resource[] = {
	[0] = {
		.start		= NE1TB_FLASH_BASE,
		.end		= NE1TB_FLASH_BASE + NE1TB_FLASH_SIZE - 1,
		.flags		= IORESOURCE_MEM,
	},
};

static struct mtd_partition ne1xb_flash_partitions[] = {
	[0] = {
		.name		= "Bootloader",
		.size		= 0x00200000,
		.offset		= 0,
		.mask_flags	= MTD_WRITEABLE,	/* force read-only */
	},
	[1] = {
		.name		= "kernel",
		.size		= 0x00400000,
		.offset		= MTDPART_OFS_APPEND,
		.mask_flags	= MTD_WRITEABLE	/* force read-only */
	},
	[2] = {
		.name		= "root",
		.size		= 0x03200000,
		.offset		= MTDPART_OFS_APPEND,
		.mask_flags	= MTD_WRITEABLE	/* force read-only */
	},
	[3] = {
		.name		= "home",
		.size		= (NE1TB_FLASH_SIZE - 0x00200000 - 0x00400000 - 0x03200000),
		.offset		= MTDPART_OFS_APPEND,
		.mask_flags	= 0,
	},
};

static struct flash_platform_data ne1xb_flash_data = {
	.map_name	= "cfi_probe",
	.width		= 2,
	.parts		= ne1xb_flash_partitions,
	.nr_parts	= ARRAY_SIZE(ne1xb_flash_partitions),
};

static struct platform_device ne1xb_flash_device = {
	.name		= "ne1xb-flash",
	.id		= 0,
	.dev		= {
		.platform_data	= &ne1xb_flash_data,
	},
	.num_resources	= ARRAY_SIZE(ne1xb_flash_resource),
	.resource	= &ne1xb_flash_resource[0],
};

/* ---- NE1-TB NAND Flash ROM ---- */
static struct mtd_partition ne1xb_nand_partition[] = {
	[0] = {
		.name   = "nand boot",
		.size   = 0x00060000,	/* 128K x 3 */
		.offset = 0,
	},
	[1] = {
		.name	= "nand filesystem",
		.size	= NE1TB_NAND_SIZE - 0x00060000,
		.offset	= MTDPART_OFS_APPEND,
	},
};
static struct platform_nand_chip ne1xb_nand_data = {
	.nr_chips      = 1,
	.chip_delay    = 15,
	.options       = 0,
	.partitions    = ne1xb_nand_partition,
	.nr_partitions = ARRAY_SIZE(ne1xb_nand_partition),
};
static struct resource ne1xb_nand_resource[] = {
	[0] = {
		.start = NE1_BASE_NAND_REG,
		.end   = NE1_BASE_NAND_REG + SZ_4K -1,
		.flags = IORESOURCE_MEM,
	},
};
static struct platform_device ne1xb_nand_device = {
	.name = "ne1xb-nand",
	.id   = -1,
	.dev  = {
		.platform_data = &ne1xb_nand_data,
		},
	.num_resources = ARRAY_SIZE(ne1xb_nand_resource),
	.resource = ne1xb_nand_resource,
};

/* ---- NE1-TB LAN9118 NIC ---- */
static struct resource ne1xb_smsc911x_resources[] = {
	[0] = {
		.start		= NE1xB_ETH_BASE,
		.end		= NE1xB_ETH_BASE + SZ_64K - 1,
		.flags		= IORESOURCE_MEM,
	},
	[1] = {
		.start		= IRQ_ETH,
		.end		= IRQ_ETH,
		.flags		= IORESOURCE_IRQ,
	},
};

static struct smsc911x_platform_config smsc911x_config = {
	.flags		   = SMSC911X_USE_32BIT,
	.irq_polarity	   = SMSC911X_IRQ_POLARITY_ACTIVE_LOW,
	.irq_type	   = SMSC911X_IRQ_TYPE_PUSH_PULL,
	.phy_interface	   = PHY_INTERFACE_MODE_MII,
};

static struct platform_device ne1xb_smsc911x_device = {
	.name		   = "smsc911x",
	.id		   = 0,
	.num_resources	   = ARRAY_SIZE(ne1xb_smsc911x_resources),
	.resource	   = ne1xb_smsc911x_resources,
	.dev.platform_data = &smsc911x_config,
};


/* ---- NE1 DMA ---- */
static struct platform_device ne1xb_dma_device = {
	.name = "dma",
	.id = -1,
};

/* ---- NE1 CSI ---- */
static struct platform_device ne1xb_csi0_device = {
	.name = "csi",
	.id = 0,
};
static struct platform_device ne1xb_csi1_device = {
	.name = "csi",
	.id = 1,
};

/* ---- NE1-TB MMC ---- */
static struct resource ne1xb_mmc_resources[] = {
	{
		.start = IRQ_MMC,
		.end   = IRQ_MMC,
		.flags = IORESOURCE_IRQ,
	},
};
static struct platform_device ne1xb_mmc_device = {
	.name = "ne1_mmc",
	.id = -1,
	.num_resources = ARRAY_SIZE(ne1xb_mmc_resources),
	.resource = ne1xb_mmc_resources,
};

/* ---- NE1-TB RTC ---- */
static struct resource ne1xb_rtc_resources[] = {
	{
		.start = IRQ_RTC,
		.end   = IRQ_RTC,
		.flags = IORESOURCE_IRQ,
	},
};
static struct platform_device ne1xb_rtc_device = {
	.name = "ne1xb_rtc",
	.id = -1,
	.num_resources = ARRAY_SIZE(ne1xb_rtc_resources),
	.resource = ne1xb_rtc_resources,
};

#ifdef CONFIG_NE1_USB
static u64 ne1_dmamask = ~(u32)0;
static void usb_release(struct device *dev)
{
}

/* ---- NE1-TB USB ---- */
static struct resource ne1xb_ohci_resources[] = {
	[0] = {
		.start		= NE1_BASE_USBH,
		.end		= NE1_BASE_USBH + SZ_4K - 1,
		.flags		= IORESOURCE_MEM,
	},
	[1] = {
		.start		= IRQ_OHCI,
		.end		= IRQ_OHCI,
		.flags		= IORESOURCE_IRQ,
	},
};
struct platform_device ne1xb_ohci_device = {
	.name = "ne1xb_ohci",
	.id = 0,
	.dev = {
		.release = usb_release,
		.dma_mask = &ne1_dmamask,
		.coherent_dma_mask = 0xffffffff,
	},
	.num_resources	= ARRAY_SIZE(ne1xb_ohci_resources),
	.resource	= ne1xb_ohci_resources,
};

static struct resource ne1xb_ehci_resources[] = {
	[0] = {
		.start		= NE1_BASE_USBH + 0x1000,
		.end		= NE1_BASE_USBH + 0x1000 + SZ_4K - 1,
		.flags		= IORESOURCE_MEM,
	},
	[1] = {
		.start		= IRQ_EHCI,
		.end		= IRQ_EHCI,
		.flags		= IORESOURCE_IRQ,
	},
};
struct platform_device ne1xb_ehci_device = {
	.name = "ne1xb_ehci",
	.id = -1,
	.dev = {
		.release = usb_release,
		.dma_mask = &ne1_dmamask,
		.coherent_dma_mask = 0xffffffff,
	},
	.num_resources	= ARRAY_SIZE(ne1xb_ehci_resources),
	.resource	= ne1xb_ehci_resources,
};
#endif


struct platform_device *platform_devs[] __initdata = {
	&ne1xb_dma_device,
	&ne1xb_csi0_device,
	&ne1xb_csi1_device,
	&ne1xb_mmc_device,
#ifdef CONFIG_NE1_USB
	&ne1xb_ohci_device,
	&ne1xb_ehci_device,
#endif

	&ne1xb_serial_device,
	&ne1xb_smsc911x_device,
	&ne1xb_flash_device,
	&ne1xb_nand_device,

	&ne1xb_rtc_device,
};

void ne1_core_init(void)
{
	platform_add_devices(platform_devs, ARRAY_SIZE(platform_devs));
}

#ifdef CONFIG_LEDS
#define VA_LEDS_BASE (IO_ADDRESS(NE1TB_BASE_FPGA) + FPGA_FLED)

void ne1tb_leds_event(led_event_t ledevt)
{
	unsigned long flags;
	u32 val;

	local_irq_save(flags);
	val = readw(VA_LEDS_BASE);

	switch (ledevt) {
	case led_idle_start:
		val = val & ~FLED_LED0;
		break;

	case led_idle_end:
		val = val | FLED_LED0;
		break;

	case led_timer:
		val = val ^ FLED_LED1;
		break;

	case led_halted:
		val = 0;
		break;

	default:
		break;
	}

	writew(val, VA_LEDS_BASE);
	local_irq_restore(flags);
}
#endif	/* CONFIG_LEDS */

#ifndef CONFIG_NKERNEL
#ifndef CONFIG_LOCAL_TIMER_TICK
/*
 * How long is the timer interval?
 */
#define	TIMER_PCLK	(66666000)			/* 66.666 MHz */
#define TIMER_RELOAD	((TIMER_PCLK / 100) - 1)	/* ticks per 10 msec */
#define TIMER_PSCALE	(0x00000001)
#define TICKS2USECS(x)	((1000 * (x)) / ((TIMER_RELOAD + 1) / 10))

/*
 * Returns number of ms since last clock interrupt.
 * Note that interrupts will have been disabled by do_gettimeoffset()
 */
static unsigned long ne1_gettimeoffset(void)
{
	unsigned long ticks1, ticks2, status;

	/*
	 * Get the current number of ticks.  Note that there is a race
	 * condition between us reading the timer and checking for
	 * an interrupt.  We get around this by ensuring that the
	 * counter has not reloaded between our two reads.
	 */
	ticks1 = readl(IO_ADDRESS(NE1_BASE_TIMER_0 + TMR_TMRCNT));
	do {
		ticks2 = ticks1;
		status = readl(IO_ADDRESS(NE1_BASE_TIMER_0 + TMR_GTINT));
		ticks1 = readl(IO_ADDRESS(NE1_BASE_TIMER_0 + TMR_TMRCNT));
	} while (ticks1 < ticks2);

	/*
	 * Interrupt pending?  If so, we've reloaded once already.
	 */
	if (status & GTINT_TCI)
		ticks1 += TIMER_RELOAD;

	/*
	 * Convert the ticks to usecs
	 */
	return TICKS2USECS(ticks1);
}

/*
 * IRQ handler for the timer
 */
static irqreturn_t ne1_timer_interrupt(int irq, void *dev_id)
{
	write_seqlock(&xtime_lock);

	// ...clear the interrupt
	writel(GTINT_TCI, IO_ADDRESS(NE1_BASE_TIMER_0 + TMR_GTINT));

	timer_tick();

#if defined(CONFIG_SMP) && !defined(CONFIG_LOCAL_TIMERS)
	smp_send_timer();
	update_process_times(user_mode(get_irq_regs()));
#endif

	write_sequnlock(&xtime_lock);

	return IRQ_HANDLED;
}

static struct irqaction ne1_timer_irq = {
	.name		= "timer tick",
	.flags		= IRQF_DISABLED | IRQF_TIMER,
	.handler	= ne1_timer_interrupt,
};


/*
 * Set up timer interrupt, and return the current time in seconds.
 */
static void __init ne1_timer_init(void)
{
	/*
	 * Initialise to a known state (all timers off)
	 */
	writel(0, IO_ADDRESS(NE1_BASE_TIMER_0 + TMR_TMRCTRL));	/* CE=0, CAE=0 */
#ifdef CONFIG_NKERNEL
	/* TIMER_1 is used as a timestamp in nkbsp */
#else
	writel(0, IO_ADDRESS(NE1_BASE_TIMER_1 + TMR_TMRCTRL));
#endif
	writel(0, IO_ADDRESS(NE1_BASE_TIMER_2 + TMR_TMRCTRL));
	writel(0, IO_ADDRESS(NE1_BASE_TIMER_3 + TMR_TMRCTRL));
	writel(0, IO_ADDRESS(NE1_BASE_TIMER_4 + TMR_TMRCTRL));
	writel(0, IO_ADDRESS(NE1_BASE_TIMER_5 + TMR_TMRCTRL));
	writel(0, IO_ADDRESS(NE1_BASE_TIMER_0 + TMR_GTICTRL));	/* RIS_C=FAL_C=0 */
#ifdef CONFIG_NKERNEL
	/* TIMER_1 is used as a timestamp in nkbsp */
#else
	writel(0, IO_ADDRESS(NE1_BASE_TIMER_1 + TMR_GTICTRL));
#endif
	writel(0, IO_ADDRESS(NE1_BASE_TIMER_2 + TMR_GTICTRL));
	writel(0, IO_ADDRESS(NE1_BASE_TIMER_3 + TMR_GTICTRL));
	writel(0, IO_ADDRESS(NE1_BASE_TIMER_4 + TMR_GTICTRL));
	writel(0, IO_ADDRESS(NE1_BASE_TIMER_5 + TMR_GTICTRL));

	/* 
	 * Set pre-scaler.
	 *   Input PCLK is 66.666 MHz
	 *   PSCAL=0b000001(1/1), PSCALE2=0x00
	 *   Timer precision is 15 ns
	 */
	writel(TIMER_PSCALE, IO_ADDRESS(NE1_BASE_TIMER_0 + TMR_PSCALE));

	/*
	 * Set reset register, clear all pending interrupts,
	 * enable timer interrupt (TCE=1), and start timer.
	 */
	writel(TIMER_RELOAD, IO_ADDRESS(NE1_BASE_TIMER_0 + TMR_TMRRST));
	writel(0xffffffff, IO_ADDRESS(NE1_BASE_TIMER_0 + TMR_GTINT));
	writel(GTINTEN_TCE, IO_ADDRESS(NE1_BASE_TIMER_0 + TMR_GTINTEN));

	/* 
	 * Make irqs happen for the system timer, and start
	 */
	setup_irq(IRQ_TIMER_0, &ne1_timer_irq);

	writel((TMRCTRL_CE | TMRCTRL_CAE), IO_ADDRESS(NE1_BASE_TIMER_0 + TMR_TMRCTRL));
}

struct sys_timer ne1_timer = {
	.init		= ne1_timer_init,
	.offset		= ne1_gettimeoffset,
};

#else

#include <asm/hardware/arm_twd.h>

/*
 * How long is the timer interval?
 */
#define PSCALER         (4)
#define	MPCORE_CLK	(399996000)			           /* 399.996 MHz */

#define TIMER_RELOAD	(((MPCORE_CLK / 100) / (PSCALER * 2)) - 1) /* ticks per 10 msec */
#define TICKS2USECS(x)	((1000 * (x)) / ((TIMER_RELOAD + 1) / 10))

#define CTRL_ENABLE_TIMER 0x00000001
#define CTRL_AUTO_RELOAD  0x00000002
#define CTRL_ENABLE_INT   0x00000004

#define INTSTAT_EVENT     0x00000001

/*
 * Returns number of ms since last clock interrupt.
 * Note that interrupts will have been disabled by do_gettimeoffset()
 */
static unsigned long ne1_gettimeoffset(void)
{
	unsigned long ticks1, ticks2, status;

	/*
	 * Get the current number of ticks.  Note that there is a race
	 * condition between us reading the timer and checking for
	 * an interrupt.  We get around this by ensuring that the
	 * counter has not reloaded between our two reads.
	 */
	ticks1 = readl(IO_ADDRESS(NE1_BASE_PTMR + TWD_TIMER_COUNTER));
	do {
		ticks2 = ticks1;
		status = readl(IO_ADDRESS(NE1_BASE_PTMR + TWD_TIMER_INTSTAT));
		ticks1 = readl(IO_ADDRESS(NE1_BASE_PTMR + TWD_TIMER_COUNTER));
	} while (ticks2 < ticks1);

	/*
	 * Interrupt pending?  If so, we've reloaded once already.
	 */
	if (status & INTSTAT_EVENT)
		ticks1 -= TIMER_RELOAD;

	/*
	 * Convert the ticks to usecs
	 */
	return TICKS2USECS(ticks1);
}

static irqreturn_t ne1_timer_interrupt(int irq, void *dev_id)
{
	write_seqlock(&xtime_lock);
	// ...clear the interrupt
	if (readl(IO_ADDRESS(NE1_BASE_PTMR + TWD_TIMER_INTSTAT))) {
	    writel(INTSTAT_EVENT, IO_ADDRESS(NE1_BASE_PTMR + TWD_TIMER_INTSTAT));
	    timer_tick();
	    write_sequnlock(&xtime_lock);
	    return IRQ_HANDLED;
	}
	write_sequnlock(&xtime_lock);
	return IRQ_NONE;
}

static struct irqaction ne1_timer_irq = {
	.name		= "timer tick",
	.flags		= IRQF_DISABLED | IRQF_TIMER,
	.handler	= ne1_timer_interrupt,
};

static void __init ne1_timer_init(void)
{
	/*
	 * Initialise to a known state (all timers off)
	 */
	writel(0, IO_ADDRESS(NE1_BASE_TIMER_0 + TMR_TMRCTRL));	/* CE=0, CAE=0 */
#ifdef CONFIG_NKERNEL
	/* TIMER_1 is used as a timestamp in nkbsp */
#else
	writel(0, IO_ADDRESS(NE1_BASE_TIMER_1 + TMR_TMRCTRL));
#endif
	writel(0, IO_ADDRESS(NE1_BASE_TIMER_2 + TMR_TMRCTRL));
	writel(0, IO_ADDRESS(NE1_BASE_TIMER_3 + TMR_TMRCTRL));
	writel(0, IO_ADDRESS(NE1_BASE_TIMER_4 + TMR_TMRCTRL));
#ifdef CONFIG_NKERNEL
	/* TIMER_5 is used as the system tick in t-kernel*/
#else
	writel(0, IO_ADDRESS(NE1_BASE_TIMER_5 + TMR_TMRCTRL));
#endif
	writel(0, IO_ADDRESS(NE1_BASE_TIMER_0 + TMR_GTICTRL));	/* RIS_C=FAL_C=0 */
#ifdef CONFIG_NKERNEL
	/* TIMER_1 is used as a timestamp in nkbsp */
#else
	writel(0, IO_ADDRESS(NE1_BASE_TIMER_1 + TMR_GTICTRL));
#endif
	writel(0, IO_ADDRESS(NE1_BASE_TIMER_2 + TMR_GTICTRL));
	writel(0, IO_ADDRESS(NE1_BASE_TIMER_3 + TMR_GTICTRL));
	writel(0, IO_ADDRESS(NE1_BASE_TIMER_4 + TMR_GTICTRL));
#ifdef CONFIG_NKERNEL
	/* TIMER_5 is used as the system tick in t-kernel*/
#else
	writel(0, IO_ADDRESS(NE1_BASE_TIMER_5 + TMR_GTICTRL));
#endif

            /* Disable the local timer */
	writel(0x00000000,   IO_ADDRESS(NE1_BASE_PTMR + TWD_TIMER_CONTROL));
            /* Set the period */
	writel(TIMER_RELOAD, IO_ADDRESS(NE1_BASE_PTMR + TWD_TIMER_LOAD));
            /* Set the interrupt handler */
	setup_irq(IRQ_LOCALTIMER, &ne1_timer_irq);
            /* Acknowledge pending interrupts */
	writel(INTSTAT_EVENT, IO_ADDRESS(NE1_BASE_PTMR + TWD_TIMER_INTSTAT));
            /* Start the local timer */
	writel(((PSCALER -1) << 8) |
               CTRL_ENABLE_INT | CTRL_AUTO_RELOAD | CTRL_ENABLE_TIMER,
               IO_ADDRESS(NE1_BASE_PTMR + TWD_TIMER_CONTROL));
	printk("Local timer used as system tick\n");
}

struct sys_timer ne1_timer = {
	.init		= ne1_timer_init,
	.offset		= ne1_gettimeoffset,
};

#endif
#endif

#ifdef CONFIG_NKERNEL

#define	TIMER_PCLK	(66666000)			/* 66.666000 MHz */
#define	PSCALE_RATIO	4

#define	TIMER_CLK	(TIMER_PCLK / PSCALE_RATIO)     /* 16.666500 MHz*/

#define NE1TB_TICKS_PER_HZ		(TIMER_CLK / HZ)
#define NE1TB_TICK_MODULO		(TIMER_CLK % HZ)

#include <nk/nkern.h>
unsigned long nk_vtick_read_stamp(void)
{
	return 	readl(IO_ADDRESS(NE1_BASE_TIMER_1 + TMR_TMRCNT));
}

unsigned long nk_vtick_ticks_to_usecs(unsigned long ticks)
{
	return (ticks * (1000000 / HZ)) / (TIMER_CLK / HZ);
}

unsigned long nk_vtick_get_ticks_per_hz(void)
{
	return NE1TB_TICKS_PER_HZ;
}

unsigned long nk_vtick_get_modulo(void)
{
	return NE1TB_TICK_MODULO;
}

#endif
