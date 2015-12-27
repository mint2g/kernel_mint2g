/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/io.h>

#include <mach/hardware.h>
#include <mach/irqs.h>
#include <mach/adi.h>
#include <mach/gpio.h>
#include <mach/globalregs.h>

#ifdef CONFIG_ARCH_SC7710
/*
 * SC8810 GPIO bank and number summary:
 *
 * 	Bank		log-s		 To		NR		phy_s	to		Type
 *	1		0   ~			223		224		0~		223		D-GPIO
 *	2		224 ~		255		32		0~		31		ANA GPIO
 *	3		256 ~		271		16		0~		15		ANA EIC
 *	4		272~		287		16		0~		15		D-EIC
 *	5		288~		?		?		0~		?		D-EIC2
 */



#define	D_GPIO_START				0
#define	D_GPIO_NR					224

#define	A_GPIO_START				224
#define	A_GPIO_NR					32

#define	A_EIC_START				256
#define	A_EIC_NR					16

#define	D_EIC_START				272
#define	D_EIC_NR					16

//#define	D_EIC2_START		288
//#define	D_EIC2_NR			?

#define	GPIO_RESERVED0_START	203
#define	GPIO_RESERVED0_NR		5

#else
/*
 * SC8810 GPIO bank and number summary:
 *
 * Bank	  From	  To	NR	Type
 * 1	  0   ~	  15	16	EIC
 * 2	  16  ~	  159	144	GPIO
 * 3	  160 ~	  175	16	ANA EIC
 * 4	  176 ~	  207	32	ANA GPIO
 */

#define	D_GPIO_START		16
#define	D_GPIO_NR		144
#define	A_GPIO_START		176
#define	A_GPIO_NR		32

#define	D_EIC_START		0
#define	D_EIC_NR		16
#define	A_EIC_START		160
#define	A_EIC_NR		16
#endif


/* Digital GPIO/EIC base address */
#define CTL_GPIO_BASE		(SPRD_GPIO_BASE)
#define CTL_EIC_BASE		(SPRD_EIC_BASE)
#ifdef CONFIG_ARCH_SC7710
#define CTL_EIC2_BASE		(SPRD_EIC2_BASE)
#endif
/* Analog GPIO/EIC base address */
#define ANA_CTL_GPIO_BASE	(SPRD_MISC_BASE + 0x0480)
#define ANA_CTL_EIC_BASE	(SPRD_MISC_BASE + 0x0700)

/* 16 GPIO share a group of registers */
#define	GPIO_GROUP_NR		(16)
#define GPIO_GROUP_MASK		(0xFFFF)

#define	GPIO_GROUP_OFFSET	(0x80)
#define	ANA_GPIO_GROUP_OFFSET	(0x40)

/* registers definitions for GPIO controller */
#define REG_GPIO_DATA		(0x0000)
#define REG_GPIO_DMSK		(0x0004)
#define REG_GPIO_DIR		(0x0008) /* only for gpio */
#define REG_GPIO_IS		(0x000c) /* only for gpio */
#define REG_GPIO_IBE		(0x0010) /* only for gpio */
#define REG_GPIO_IEV		(0x0014)
#define REG_GPIO_IE		(0x0018)
#define REG_GPIO_RIS		(0x001c)
#define REG_GPIO_MIS		(0x0020)
#define REG_GPIO_IC		(0x0024)
#define REG_GPIO_INEN		(0x0028) /* only for gpio */

/* 8 EIC share a group of registers */
#define	EIC_GROUP_NR		(8)
#define EIC_GROUP_MASK		(0xFF)

/* registers definitions for EIC controller */
#define REG_EIC_DATA		REG_GPIO_DATA
#define REG_EIC_DMSK		REG_GPIO_DMSK
#define REG_EIC_IEV		REG_GPIO_IEV
#define REG_EIC_IE		REG_GPIO_IE
#define REG_EIC_RIS		REG_GPIO_RIS
#define REG_EIC_MIS		REG_GPIO_MIS
#define REG_EIC_IC		REG_GPIO_IC
#define REG_EIC_TRIG		(0x0028) /* only for eic */
#define REG_EIC_0CTRL		(0x0040)
#define REG_EIC_1CTRL		(0x0044)
#define REG_EIC_2CTRL		(0x0048)
#define REG_EIC_3CTRL		(0x004c)
#define REG_EIC_4CTRL		(0x0050)
#define REG_EIC_5CTRL		(0x0054)
#define REG_EIC_6CTRL		(0x0058)
#define REG_EIC_7CTRL		(0x005c)
#define REG_EIC_DUMMYCTRL	(0x0000)

/* bits definitions for register REG_EIC_DUMMYCTRL */
#define BIT_FORCE_CLK_DBNC	BIT(15)
#define BIT_EIC_DBNC_EN		BIT(14)
#define SHIFT_EIC_DBNC_CNT	(0)
#define MASK_EIC_DBNC_CNT	(0xFFF)
#define BITS_EIC_DBNC_CNT(_x_)	((_x) & 0xFFF)

struct sci_gpio_chip {
	struct gpio_chip	chip;

	uint32_t		base_addr;
	uint32_t		group_offset;

	uint32_t		(*read_reg)(uint32_t addr);
	void			(*write_reg)(uint32_t value, uint32_t addr);
	void			(*set_bits)(uint32_t bits, uint32_t addr);
	void			(*clr_bits)(uint32_t bits, uint32_t addr);
};

#define	to_sci_gpio(c)		container_of(c, struct sci_gpio_chip, chip)

/* D-Die regs ops */
static uint32_t d_read_reg(uint32_t addr)
{
	return __raw_readl(addr);
}
static void d_write_reg(uint32_t value, uint32_t addr)
{
	__raw_writel(value, addr);
}
static void d_set_bits(uint32_t bits, uint32_t addr)
{
	__raw_writel(__raw_readl(addr) | bits, addr);
}
static void d_clr_bits(uint32_t bits, uint32_t addr)
{
	__raw_writel(__raw_readl(addr) & ~bits, addr);
}

/* A-Die regs ops */
static uint32_t a_read_reg(uint32_t addr)
{
	return sci_adi_read(addr);
}
static void a_write_reg(uint32_t value, uint32_t addr)
{
	sci_adi_raw_write(addr, value);
}
static void a_set_bits(uint32_t bits, uint32_t addr)
{
	sci_adi_set(addr, bits);
}
static void a_clr_bits(uint32_t bits, uint32_t addr)
{
	sci_adi_clr(addr, bits);
}

static int sci_gpio_read(struct gpio_chip *chip, uint32_t offset, uint32_t reg)
{
	struct sci_gpio_chip *sci_gpio = to_sci_gpio(chip);
	int group = offset / GPIO_GROUP_NR;
	int bitof = offset & (GPIO_GROUP_NR - 1);
	int addr = sci_gpio->base_addr + sci_gpio->group_offset * group + reg;
	int value = sci_gpio->read_reg(addr) & GPIO_GROUP_MASK;

	return (value >> bitof) & 0x1;
}

static void sci_gpio_write(struct gpio_chip *chip, uint32_t offset, uint32_t reg, int value)
{
	struct sci_gpio_chip *sci_gpio = to_sci_gpio(chip);
	int group = offset / GPIO_GROUP_NR;
	int bitof = offset & (GPIO_GROUP_NR - 1);
	int addr = sci_gpio->base_addr + sci_gpio->group_offset * group + reg;

	if (value) {
		sci_gpio->set_bits(1 << bitof, addr);
	} else {
		sci_gpio->clr_bits(1 << bitof, addr);
	}
}

/* GPIO/EIC libgpio interfaces */
static int sci_gpio_request(struct gpio_chip *chip, unsigned offset)
{
	pr_debug("%s %d+%d\n", __FUNCTION__, chip->base, offset);
	sci_gpio_write(chip, offset, REG_GPIO_DMSK, 1);
	return 0;
}

static void sci_gpio_free(struct gpio_chip *chip, unsigned offset)
{
	pr_debug("%s %d+%d\n", __FUNCTION__, chip->base, offset);
	sci_gpio_write(chip, offset, REG_GPIO_DMSK, 0);
}

static int sci_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	pr_debug("%s %d+%d\n", __FUNCTION__, chip->base, offset);
	sci_gpio_write(chip, offset, REG_GPIO_DIR, 0);
	sci_gpio_write(chip, offset, REG_GPIO_INEN, 1);
	return 0;
}

static int sci_eic_direction_input(struct gpio_chip *chip, unsigned offset)
{
	/* do nothing */
	pr_debug("%s %d+%d\n", __FUNCTION__, chip->base, offset);
	return 0;
}

static int sci_gpio_direction_output(struct gpio_chip *chip, unsigned offset,
				     int value)
{
	pr_debug("%s %d+%d %d\n", __FUNCTION__, chip->base, offset, value);
	sci_gpio_write(chip, offset, REG_GPIO_DIR, 1);
	sci_gpio_write(chip, offset, REG_GPIO_INEN, 0);
	sci_gpio_write(chip, offset, REG_GPIO_DATA, value);
	return 0;
}

static int sci_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	pr_debug("%s %d+%d\n", __FUNCTION__, chip->base, offset);
	return sci_gpio_read(chip, offset, REG_GPIO_DATA);
}

static void sci_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	pr_debug("%s %d+%d %d\n", __FUNCTION__, chip->base, offset, value);
	sci_gpio_write(chip, offset, REG_GPIO_DATA, value);
}

static int sci_gpio_set_debounce(struct gpio_chip *chip, unsigned offset,
				unsigned debounce)
{
	/* not supported */
	pr_err("%s %d+%d\n", __FUNCTION__, chip->base, offset);
	return -EINVAL;
}

static int sci_eic_set_debounce(struct gpio_chip *chip, unsigned offset,
				unsigned debounce)
{
	/* TODO */
	pr_info("%s %d+%d\n", __FUNCTION__, chip->base, offset);
	return 0;
}

static int sci_gpio_to_irq(struct gpio_chip *chip, unsigned offset)
{
	return chip->base + offset + GPIO_IRQ_START;
}

static int sci_irq_to_gpio(struct gpio_chip *chip, unsigned irq)
{
	return irq - GPIO_IRQ_START - chip->base;
}

static struct sci_gpio_chip d_sci_gpio = {
	.chip.label		= "sprd-d-gpio",
	.chip.request		= sci_gpio_request,
	.chip.free		= sci_gpio_free,
	.chip.direction_input	= sci_gpio_direction_input,
	.chip.get		= sci_gpio_get,
	.chip.direction_output	= sci_gpio_direction_output,
	.chip.set		= sci_gpio_set,
	.chip.set_debounce	= sci_gpio_set_debounce,
	.chip.to_irq		= sci_gpio_to_irq,
	.chip.base		= D_GPIO_START,
	.chip.ngpio		= D_GPIO_NR,

	.base_addr		= CTL_GPIO_BASE,
	.group_offset		= GPIO_GROUP_OFFSET,
	.read_reg		= d_read_reg,
	.write_reg		= d_write_reg,
	.set_bits		= d_set_bits,
	.clr_bits		= d_clr_bits,
};

static struct sci_gpio_chip a_sci_gpio = {
	.chip.label		= "sprd-a-gpio",
	.chip.request		= sci_gpio_request,
	.chip.free		= sci_gpio_free,
	.chip.direction_input	= sci_gpio_direction_input,
	.chip.get		= sci_gpio_get,
	.chip.direction_output	= sci_gpio_direction_output,
	.chip.set		= sci_gpio_set,
	.chip.set_debounce	= sci_gpio_set_debounce,
	.chip.to_irq		= sci_gpio_to_irq,
	.chip.base		= A_GPIO_START,
	.chip.ngpio		= A_GPIO_NR,

	.base_addr		= ANA_CTL_GPIO_BASE,
	.group_offset		= ANA_GPIO_GROUP_OFFSET,
	.read_reg		= a_read_reg,
	.write_reg		= a_write_reg,
	.set_bits		= a_set_bits,
	.clr_bits		= a_clr_bits,
};

/*
 * EIC has the same register layout with GPIO when it's used as GPI.
 * So most implementation of GPIO can be shared by EIC.
 */
static struct sci_gpio_chip d_sci_eic = {
	.chip.label		= "sprd-d-eic",
	.chip.request		= sci_gpio_request,
	.chip.free		= sci_gpio_free,
	.chip.direction_input	= sci_eic_direction_input,
	.chip.get		= sci_gpio_get,
	.chip.direction_output	= NULL,
	.chip.set		= NULL,
	.chip.set_debounce	= sci_eic_set_debounce,
	.chip.to_irq		= sci_gpio_to_irq,
	.chip.base		= D_EIC_START,
	.chip.ngpio		= D_EIC_NR,

	.base_addr		= CTL_EIC_BASE,
	.group_offset		= GPIO_GROUP_OFFSET,
	.read_reg		= d_read_reg,
	.write_reg		= d_write_reg,
	.set_bits		= d_set_bits,
	.clr_bits		= d_clr_bits,
};

static struct sci_gpio_chip a_sci_eic = {
	.chip.label		= "sprd-a-eic",
	.chip.request		= sci_gpio_request,
	.chip.free		= sci_gpio_free,
	.chip.direction_input	= sci_eic_direction_input,
	.chip.get		= sci_gpio_get,
	.chip.direction_output	= NULL,
	.chip.set		= NULL,
	.chip.set_debounce	= sci_eic_set_debounce,
	.chip.to_irq		= sci_gpio_to_irq,
	.chip.base		= A_EIC_START,
	.chip.ngpio		= A_EIC_NR,

	.base_addr		= ANA_CTL_EIC_BASE,
	.group_offset		= ANA_GPIO_GROUP_OFFSET,
	.read_reg		= a_read_reg,
	.write_reg		= a_write_reg,
	.set_bits		= a_set_bits,
	.clr_bits		= a_clr_bits,
};

/* GPIO/EIC irq interfaces */
static void sci_gpio_irq_ack(struct irq_data *data)
{
	struct gpio_chip *chip = (struct gpio_chip *)data->chip_data;
	int offset = sci_irq_to_gpio(chip, data->irq);
	pr_debug("%s %d+%d\n", __FUNCTION__, chip->base, offset);
	sci_gpio_write(chip, offset, REG_GPIO_IC, 1);
}

static void sci_gpio_irq_mask(struct irq_data *data)
{
	struct gpio_chip *chip = (struct gpio_chip *)data->chip_data;
	int offset = sci_irq_to_gpio(chip, data->irq);
	pr_debug("%s %d+%d\n", __FUNCTION__, chip->base, offset);
	sci_gpio_write(chip, offset, REG_GPIO_IE, 0);
}

static void sci_gpio_irq_unmask(struct irq_data *data)
{
	struct gpio_chip *chip = (struct gpio_chip *)data->chip_data;
	int offset = sci_irq_to_gpio(chip, data->irq);
	pr_debug("%s %d+%d\n", __FUNCTION__, chip->base, offset);
	sci_gpio_write(chip, offset, REG_GPIO_IE, 1);
}

static void sci_eic_irq_unmask(struct irq_data *data)
{
	struct gpio_chip *chip = (struct gpio_chip *)data->chip_data;
	int offset = sci_irq_to_gpio(chip, data->irq);
	pr_debug("%s %d+%d\n", __FUNCTION__, chip->base, offset);
	sci_gpio_write(chip, offset, REG_EIC_IE, 1);

	/* TODO: the interval of two EIC trigger needs be longer than 2ms */
	sci_gpio_write(chip, offset, REG_EIC_TRIG, 1);
}

static int sci_gpio_irq_set_type(struct irq_data *data, unsigned int flow_type)
{
	struct gpio_chip *chip = (struct gpio_chip *)data->chip_data;
	int offset = sci_irq_to_gpio(chip, data->irq);
	pr_debug("%s %d+%d %d\n", __FUNCTION__, chip->base, offset, flow_type);
	switch (flow_type) {
	case IRQ_TYPE_EDGE_RISING:
		sci_gpio_write(chip, offset, REG_GPIO_IS, 0);
		sci_gpio_write(chip, offset, REG_GPIO_IBE, 0);
		sci_gpio_write(chip, offset, REG_GPIO_IEV, 1);
		__irq_set_handler_locked(data->irq, handle_edge_irq);
		break;
	case IRQ_TYPE_EDGE_FALLING:
		sci_gpio_write(chip, offset, REG_GPIO_IS, 0);
		sci_gpio_write(chip, offset, REG_GPIO_IBE, 0);
		sci_gpio_write(chip, offset, REG_GPIO_IEV, 0);
		__irq_set_handler_locked(data->irq, handle_edge_irq);
		break;
	case IRQ_TYPE_EDGE_BOTH:
		sci_gpio_write(chip, offset, REG_GPIO_IS, 0);
		sci_gpio_write(chip, offset, REG_GPIO_IBE, 1);
		__irq_set_handler_locked(data->irq, handle_edge_irq);
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		sci_gpio_write(chip, offset, REG_GPIO_IS, 1);
		sci_gpio_write(chip, offset, REG_GPIO_IBE, 0);
		sci_gpio_write(chip, offset, REG_GPIO_IEV, 1);
		__irq_set_handler_locked(data->irq, handle_level_irq);
		break;
	case IRQ_TYPE_LEVEL_LOW:
		sci_gpio_write(chip, offset, REG_GPIO_IS, 1);
		sci_gpio_write(chip, offset, REG_GPIO_IBE, 0);
		sci_gpio_write(chip, offset, REG_GPIO_IEV, 0);
		__irq_set_handler_locked(data->irq, handle_level_irq);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sci_eic_irq_set_type(struct irq_data *data, unsigned int flow_type)
{
	struct gpio_chip *chip = (struct gpio_chip *)data->chip_data;
	int offset = sci_irq_to_gpio(chip, data->irq);
	pr_debug("%s %d+%d %d\n", __FUNCTION__, chip->base, offset, flow_type);
	switch (flow_type) {
	case IRQ_TYPE_LEVEL_HIGH:
		sci_gpio_write(chip, offset, REG_EIC_IEV, 1);
		__irq_set_handler_locked(data->irq, handle_level_irq);
		break;
	case IRQ_TYPE_LEVEL_LOW:
		sci_gpio_write(chip, offset, REG_EIC_IEV, 0);
		__irq_set_handler_locked(data->irq, handle_level_irq);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sci_gpio_irq_set_wake(struct irq_data *data, unsigned int on)
{
	return on ? 0 : -EPERM;
}

static struct irq_chip d_gpio_irq_chip = {
	.name		= "irq-d-gpio",
	.irq_ack	= sci_gpio_irq_ack,
	.irq_mask	= sci_gpio_irq_mask,
	.irq_unmask	= sci_gpio_irq_unmask,
	.irq_set_type	= sci_gpio_irq_set_type,
	.irq_set_wake	= sci_gpio_irq_set_wake,
};

static struct irq_chip a_gpio_irq_chip = {
	.name		= "irq-a-gpio",
	.irq_ack	= sci_gpio_irq_ack,
	.irq_mask	= sci_gpio_irq_mask,
	.irq_unmask	= sci_gpio_irq_unmask,
	.irq_set_type	= sci_gpio_irq_set_type,
	.irq_set_wake	= sci_gpio_irq_set_wake,
};

/*
 * EIC has the same register layout with GPIO when it's used as GPI.
 * So most implementation of GPIO can be shared by EIC.
 */
static struct irq_chip d_eic_irq_chip = {
	.name		= "irq-d-eic",
	.irq_ack	= sci_gpio_irq_ack,
	.irq_mask	= sci_gpio_irq_mask,
	.irq_unmask	= sci_eic_irq_unmask,
	.irq_set_type	= sci_eic_irq_set_type,
};

static struct irq_chip a_eic_irq_chip = {
	.name		= "irq-a-eic",
	.irq_ack	= sci_gpio_irq_ack,
	.irq_mask	= sci_gpio_irq_mask,
	.irq_unmask	= sci_eic_irq_unmask,
	.irq_set_type	= sci_eic_irq_set_type,
};

/*
 * We set IRQ_EIC_INT as the last reserved IRQ NR, because D-Die EIC chip
 * share the IRQ number IRQ_GPIO_INT with D-Die GPIO. It's handled in the
 * muxed handler.
 */
#ifndef CONFIG_ARCH_SC7710
/*In 7710g chip, it has independent EIC interrupt bit in interrupt controller*/
#define	IRQ_EIC_INT	(NR_IRQS - 1)
#endif

/* gpio/eic cascaded irq handler */
static void gpio_muxed_handler(unsigned int irq, struct irq_desc *desc)
{
	struct gpio_chip *chip = irq_get_handler_data(irq);
	struct sci_gpio_chip *sci_gpio = to_sci_gpio(chip);
	int group, n, addr, value, count = 0;

	pr_debug("%s %d+%d %d\n", __FUNCTION__, chip->base, chip->ngpio, irq);
	for (group = 0; group * GPIO_GROUP_NR < chip->ngpio; group++) {
		addr = sci_gpio->base_addr + sci_gpio->group_offset * group + REG_GPIO_MIS;
		value = sci_gpio->read_reg(addr) & GPIO_GROUP_MASK;

		while (value) {
			n = __ffs(value);
			value &= ~(1 << n);
			n = chip->to_irq(chip, group * GPIO_GROUP_NR + n);
			pr_debug("%s generic_handle_n %d\n", __FUNCTION__, n);
			count++;
			generic_handle_irq(n);
		}
	}

#ifndef CONFIG_ARCH_SC7710
	/* handle the shared D-Die EIC */
	if (irq == IRQ_GPIO_INT && count == 0) {
		gpio_muxed_handler(IRQ_EIC_INT, irq_to_desc(IRQ_EIC_INT));
	}
#endif

#ifdef CONFIG_NKERNEL
	desc->irq_data.chip->irq_unmask(&desc->irq_data);
#endif
}

void __init gpio_irq_init(int irq, struct gpio_chip *gpiochip, struct irq_chip *irqchip)
{
	int n = gpiochip->to_irq(gpiochip, 0);
	int irqend = n + gpiochip->ngpio;

	/* setup the cascade irq handlers */
	irq_set_chained_handler(irq, gpio_muxed_handler);
	irq_set_handler_data(irq, gpiochip);

	for (; n < irqend; n++) {
		irq_set_chip_and_handler(n, irqchip, handle_level_irq);
		irq_set_chip_data(n, gpiochip);
		set_irq_flags(n, IRQF_VALID);
	}
}

#define ANA_CTL_GLB_BASE		( SPRD_MISC_BASE + 0x0600 )
#define ANA_REG_GLB_APB_CLK_EN          ( ANA_CTL_GLB_BASE + 0x0000 )
#define BIT_RTC_EIC_EB                  ( BIT(11) )
#define BIT_EIC_EB                      ( BIT(3) )

static int __init gpio_init(void)
{
	/* enable EIC */
	sci_adi_set(ANA_REG_GLB_APB_CLK_EN, BIT_EIC_EB | BIT_RTC_EIC_EB);
	sprd_greg_set_bits(REG_TYPE_GLOBAL, GEN0_EIC_EN | GEN0_EIC_RTC_EN, GR_GEN0);

	gpiochip_add(&d_sci_eic.chip);
	gpiochip_add(&d_sci_gpio.chip);
	gpiochip_add(&a_sci_eic.chip);
	gpiochip_add(&a_sci_gpio.chip);

#ifndef CONFIG_NKERNEL
#ifndef CONFIG_ARCH_SC7710
	irq_set_chip_and_handler(IRQ_EIC_INT, &dummy_irq_chip, handle_level_irq);
	set_irq_flags(IRQ_EIC_INT, IRQF_VALID);
#endif
#endif

	gpio_irq_init(IRQ_EIC_INT, &d_sci_eic.chip, &d_eic_irq_chip);
	gpio_irq_init(IRQ_GPIO_INT, &d_sci_gpio.chip, &d_gpio_irq_chip);
	gpio_irq_init(IRQ_ANA_EIC_INT, &a_sci_eic.chip, &a_eic_irq_chip);
	gpio_irq_init(IRQ_ANA_GPIO_INT, &a_sci_gpio.chip, &a_gpio_irq_chip);
	return 0;
}

arch_initcall(gpio_init);
