#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <mach/globalregs.h>

#include "sprd_busmonitor_core.h"

struct sprd_bm_reg {
	u32 intc;
	u32 cfg;
	u32 addr_min;
	u32 addr_max;
	u32 addr_mask;
	u32 data_min;
	u32 data_max;
	u32 data_mask;
	u32 match_addr;
	u32 match_data;
	union {
		u32 cnt;
		/* for AXI busmonitor */
		struct {
			u32 match_data_h32;
			u32 data_min_h32;
			u32 data_max_h32;
			u32 data_mask_h32;
		};
	};
};

static int sprd_bm_open(const struct sprd_bm_chip *chip)
{
	/* AHB register */
	if (chip->bm_type == AHB_BM) {
		switch (chip->bm_id) {
		case 0:
			sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL, AHB_CTL0_BM0_EN, AHB_CTL0);
			break;
		case 1:
			sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL, AHB_CTL0_BM1_EN, AHB_CTL0);
			break;
		case 2:
			sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL, AHB_CTL0_BM2_EN, AHB_CTL0);
			break;
		case 3:
			sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL, AHB_CTL0_BM3_EN, AHB_CTL0);
			break;
		case 4:
			sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL, AHB_CTL0_BM4_EN, AHB_CTL0);
			break;
		default:
			break;
		}
	}

	if (chip->bm_type == AXI_BM) {
		switch (chip->bm_id) {
		case 0:
			sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL, AHB_CTL0_AXIBUSMON0_EN, AHB_CTL0);
			break;
		case 1:
			sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL, AHB_CTL0_AXIBUSMON1_EN, AHB_CTL0);
			break;
		case 2:
			sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL, AHB_CTL0_AXIBUSMON2_EN, AHB_CTL0);
			break;
		default:
			break;
		}
	}
	return 0;
}

static int sprd_bm_close(const struct sprd_bm_chip *chip)
{
	/* AHB register */
	if (chip->bm_type == AHB_BM) {
		switch (chip->bm_id) {
		case 0:
			sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL, AHB_CTL0_BM0_EN, AHB_CTL0);
			break;
		case 1:
			sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL, AHB_CTL0_BM1_EN, AHB_CTL0);
			break;
		case 2:
			sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL, AHB_CTL0_BM2_EN, AHB_CTL0);
			break;
		case 3:
			sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL, AHB_CTL0_BM3_EN, AHB_CTL0);
			break;
		case 4:
			sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL, AHB_CTL0_BM4_EN, AHB_CTL0);
			break;
		default:
			break;
		}
	}

	if (chip->bm_type == AXI_BM) {
		switch (chip->bm_id) {
		case 0:
			sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL, AHB_CTL0_AXIBUSMON0_EN, AHB_CTL0);
			break;
		case 1:
			sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL, AHB_CTL0_AXIBUSMON1_EN, AHB_CTL0);
			break;
		case 2:
			sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL, AHB_CTL0_AXIBUSMON2_EN, AHB_CTL0);
			break;
		default:
			break;
		}
	}


	return 0;
}

static void sprd_bm_chn_sel(const struct sprd_bm_chip *chip, u32 chn_id)
{
	u32 val = 0;

	switch (chip->bm_id) {
	case 0:
		val = 0x1 << 4;
		break;
	case 1:
		val = 0x1 << 5;
		break;
	/* bus mon 2, 3, 4 and axi mon 1, 2, 3 have channel 0 only */
	default:
		break;
	}

	if (chn_id == 0)
		sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL, val, AHB_CTL3);
	if (chn_id == 1)
		sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL, val, AHB_CTL3);
}

static void sprd_bm_point_enable(const struct sprd_bm_chip *chip)
{
	volatile struct sprd_bm_reg *reg;

	reg = (struct sprd_bm_reg * )chip->reg_base;

	reg->intc |= 0x1 << 29;
	reg->intc |= 0x1 << 28 | 0x1;
}

static void sprd_bm_point_disable(const struct sprd_bm_chip *chip)
{
	volatile struct sprd_bm_reg *reg;

	reg = (struct sprd_bm_reg * )chip->reg_base;

		reg->intc &= ~(0x1 << 28 | 0x1);
		reg->intc |= 0x1 << 29;
}

static int sprd_bm_point_config(const struct sprd_bm_chip *chip, const  struct sprd_bm_setting *bm_setting)
{
	volatile struct sprd_bm_reg *reg;

	reg = (struct sprd_bm_reg * )chip->reg_base;

	sprd_bm_chn_sel(chip, bm_setting->bm_chn);

	reg->cfg = bm_setting->bm_cfg;
	reg->addr_min = bm_setting->addr_min;
	reg->addr_max = bm_setting->addr_max;
	reg->data_min = bm_setting->data_min;
	reg->data_max = bm_setting->data_max;
	reg->data_mask = bm_setting->data_mask;
	if (chip->bm_type == AHB_BM) {
		reg->addr_mask = bm_setting->addr_mask;
	}

	if (chip->bm_type == AXI_BM) {
		reg->data_min_h32 = bm_setting->data_min_h32;
		reg->data_max_h32 = bm_setting->data_max_h32;
		reg->data_mask_h32 = bm_setting->data_mask_h32;
	}

	return 0;
}

static int sprd_bm_point_get_cnt(const struct sprd_bm_chip *chip)
{
	volatile struct sprd_bm_reg *reg;

	if (chip->bm_type == AHB_BM) {
		reg = (struct sprd_bm_reg * )chip->reg_base;
		return reg->cnt;
	}

	return 0;
}

static irqreturn_t sprd_bm_isr(int irq_num, void *dev)
{
	struct sprd_bm_chip *chip;
	volatile struct sprd_bm_reg *reg;

	chip = (struct sprd_bm_chip *)dev;
	reg = (struct sprd_bm_reg *)chip->reg_base;

	if (unlikely(0 == (reg->intc & 0x1 << 30)))
		return IRQ_NONE;

	chip->is_irq_occur = 1;
	chip->match_data.match_addr = reg->match_addr;
	chip->match_data.match_data = reg->match_data;

	if (chip->bm_type == AXI_BM)
		chip->match_data.match_data_h32 = reg->match_data_h32;

	if (chip->call_back)
		chip->call_back(chip);

	reg->intc |= 0x1 << 29;

	wake_up_interruptible(&chip->wait_queue);

	return IRQ_HANDLED;
}

static int __devinit sprd_bm_probe(struct platform_device *pdev)
{
	int ret;
	void *__iomem reg_base;
	u32 irq_num;
	struct resource *res;
	struct sprd_bm_chip *chip;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		return -ENODEV;
	}
	reg_base = (void *__iomem)res->start;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		return -ENODEV;
	}
	irq_num = (u32)res->start;

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		return -ENOMEM;
	}

	/* the device address has mapped already */
	chip->reg_base = reg_base;
	chip->irq_num = irq_num;
	chip->bm_id = pdev->id;
	chip->name = pdev->name;

	if (!strcmp(chip->name, "sprd_ahb_busmonitor"))
		chip->bm_type = AHB_BM;

	if (!strcmp(chip->name, "sprd_axi_busmonitor"))
		chip->bm_type = AXI_BM;

	chip->open = sprd_bm_open;
	chip->release = sprd_bm_close;
	chip->point_config = sprd_bm_point_config;
	chip->point_enable = sprd_bm_point_enable;
	chip->point_disable = sprd_bm_point_disable;
	chip->point_get_cnt = sprd_bm_point_get_cnt;

	ret = request_irq(chip->irq_num, sprd_bm_isr, IRQF_SHARED, pdev->name, chip);
	if (ret < 0) {
		goto err;
	}

	ret = sprd_bm_register( chip);
	if (ret < 0) {
		goto err;
	}

	platform_set_drvdata(pdev, chip);

	return 0;
err:
	kfree(chip);
	return ret;
}

static int __devexit sprd_bm_remove(struct platform_device *pdev)
{
	struct sprd_bm_chip *chip;

	chip = platform_get_drvdata(pdev);

	sprd_bm_unregister( chip);

	kfree(chip);

	return 0;
}

static const struct platform_device_id sprd_bm_ids[] = {
	[0] = {
		.name = "sprd_ahb_busmonitor",
	},
	[1] = {
		.name = "sprd_axi_busmonitor",
	},
	[2] = {
	},
};

static  struct platform_driver sprd_bm_driver = {
	.probe = sprd_bm_probe,
	.remove = sprd_bm_remove,
	.id_table = sprd_bm_ids,
	.driver = {
		.owner = THIS_MODULE,
		.name = "sprd_busmonitor",
	},
};

static int __init sprd_bm_init(void)
{
	return platform_driver_register(&sprd_bm_driver);
}

static void __exit sprd_bm_exit(void)
{
	platform_driver_unregister(&sprd_bm_driver);
}

module_init(sprd_bm_init);
module_exit(sprd_bm_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("jack.jiang<jack.jiang@apreadtrum.com>");
MODULE_DESCRIPTION("spreadtrum platform busmonitor driver");
