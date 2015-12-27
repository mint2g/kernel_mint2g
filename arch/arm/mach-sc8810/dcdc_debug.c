#include <linux/bug.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <mach/hardware.h>
#include <mach/hardware.h>
#include <mach/adi.h>
#include <mach/adc.h>

/* FIXME */
#define	  SPRD_ANA_BASE 	   (SPRD_MISC_BASE + 0x600)
#define   ANA_REG_BASE         SPRD_ANA_BASE	/*  0x82000600 */
#define   ANA_DCDC_CTRL        (ANA_REG_BASE + 0x38)
#define   ANA_DCDC_CTRL_CAL    (ANA_REG_BASE + 0x40)
#define   ANA_DCDCARM_CTRL     (ANA_REG_BASE + 0x44)
#define   ANA_DCDCARM_CTRL_CAL (ANA_REG_BASE + 0x48)
#define   MPLL_MN				(SPRD_GREG_BASE + 0x0024)

static const int dcdc_ctl_vol[] = {
	650, 700, 800, 900, 1000, 1100, 1200, 1300, 1400,
};

int mpll_calibrate(int cpu_freq);
int dcdc_calibrate(int adc_chan, int def_vol, int to_vol);

static u32 dcdc_to_vol = 0, dcdcarm_to_vol = 0;
static int debugfs_dcdc_get(void *data, u64 * val)
{
	int def_vol = 1100;
	int i, cal_vol = sci_adi_read(ANA_DCDC_CTRL_CAL) & 0x1f;
	i = sci_adi_read(ANA_DCDC_CTRL) & 0x07;
	if (0 != i /* + cal_vol */ )
		def_vol = dcdc_ctl_vol[i];
	def_vol += cal_vol * 100 / 32;
	*(u32 *) data = *val = def_vol;
	return 0;
}

static int debugfs_dcdcarm_get(void *data, u64 * val)
{
	int def_vol = 1200;
	int i, cal_vol = sci_adi_read(ANA_DCDCARM_CTRL_CAL) & 0x1f;
	i = sci_adi_read(ANA_DCDCARM_CTRL) & 0x07;
	if (0 != i /* + cal_vol */ )
		def_vol = dcdc_ctl_vol[i];
	def_vol += cal_vol * 100 / 32;
	*(u32 *) data = *val = def_vol;
	return 0;
}

static int debugfs_dcdc_set(void *data, u64 val)
{
	int ret, to_vol;
	to_vol = *(u32 *) data = val;
	debugfs_dcdc_get(data, &val);
	ret = dcdc_calibrate(ADC_CHANNEL_DCDC, dcdc_to_vol, to_vol);
	if (ret > 0)
		dcdc_calibrate(ADC_CHANNEL_DCDC, ret, to_vol);
	return 0;
}

static int debugfs_dcdcarm_set(void *data, u64 val)
{
	int ret, to_vol;
	to_vol = *(u32 *) data = val;
	debugfs_dcdcarm_get(data, &val);
	ret = dcdc_calibrate(ADC_CHANNEL_DCDCARM, dcdcarm_to_vol, to_vol);
	if (ret > 0)
		dcdc_calibrate(ADC_CHANNEL_DCDCARM, ret, to_vol);
	return 0;
}

static u32 mpll_freq = 0;
static int debugfs_mpll_get(void *data, u64 * val)
{
	if (0 == mpll_freq) {
		*(u32 *) data = (__raw_readl(MPLL_MN) & 0x7ff) * 4;	/* default refin */
	}
	*val = *(u32 *) data;
	return 0;
}

static int debugfs_mpll_set(void *data, u64 val)
{
	*(u32 *) data = val;
	mpll_calibrate(mpll_freq);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_dcdc,
			debugfs_dcdc_get, debugfs_dcdc_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fops_dcdcarm,
			debugfs_dcdcarm_get, debugfs_dcdcarm_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fops_mpll,
			debugfs_mpll_get, debugfs_mpll_set, "%llu\n");

static int __init dcdc_debugfs_init(void)
{
	static struct dentry *debug_root = NULL;
	debug_root = debugfs_create_dir("vol", NULL);
	if (IS_ERR_OR_NULL(debug_root)) {
		printk("%s return %p\n", __FUNCTION__, debug_root);
		return (int)debug_root;
	}
	debugfs_create_file("dcdc", S_IRUGO | S_IWUSR | S_IWGRP,
			    debug_root, &dcdc_to_vol, &fops_dcdc);
	debugfs_create_file("dcdcarm", S_IRUGO | S_IWUSR | S_IWGRP,
			    debug_root, &dcdcarm_to_vol, &fops_dcdcarm);
	debugfs_create_file("mpll", S_IRUGO | S_IWUSR | S_IWGRP,
			    debug_root, &mpll_freq, &fops_mpll);
	return 0;
}

module_init(dcdc_debugfs_init);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Robot <zhulin.lian@spreadtrum.com>");
MODULE_DESCRIPTION("dcdc debugfs");
