#include <linux/bug.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <mach/hardware.h>

#include <linux/irqflags.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/regulator/consumer.h>

#include <mach/hardware.h>
#include <mach/regs_glb.h>
#include <mach/regs_ana_glb.h>
#include <mach/sci.h>
#include <mach/adi.h>
#include <mach/adc.h>
#include <mach/efuse.h>

#define REG_SYST_VALUE                  (SPRD_SYSCNT_BASE + 0x0004)

#define debug(format, arg...) pr_info("dcdc: " "@@@" format, ## arg)
#define info(format, arg...) pr_info("dcdc: " "@@@" format, ## arg)

int sprd_get_adc_cal_type(void);
uint16_t sprd_get_adc_to_vol(uint16_t data);
extern int (*dcdc_get_small_voltage) (u32);
extern int (*dcdc_set_small_voltage) (u32, int);

static uint32_t bat_numerators, bat_denominators;
static int is_ddr2;

#define CALIBRATE_TO	(60 * 1)	/* one minute */
#define MEASURE_TIMES	(128)

int dcdc_adc_get(int adc_chan)
{
	int i;
	u32 val[MEASURE_TIMES], sum = 0, adc_vol;
	u32 chan_numerators, chan_denominators;

	sci_adc_get_vol_ratio(adc_chan, true, &chan_numerators,
			      &chan_denominators);

	for (i = 0; i < ARRAY_SIZE(val); i++) {
		sum += val[i] = sci_adc_get_value(adc_chan, true);
	}
	sum /= ARRAY_SIZE(val);	/* get average value */
	/* info("adc chan %d, value %d\n", adc_chan, sum); */
	adc_vol = DIV_ROUND_CLOSEST(sprd_get_adc_to_vol(sum) *
				    (bat_numerators * chan_denominators),
				    (bat_denominators * chan_numerators));
	return adc_vol;
}

static u32 __dcdc_get_cal_ctl(u32 vol_ctl)
{
	u32 cal_ctl = 0;
	switch (vol_ctl) {
	case ANA_REG_GLB_DCDC_CTRL0:
		cal_ctl = ANA_REG_GLB_DCDC_CTRL_CAL;
		break;
	case ANA_REG_GLB_DCDCARM_CTRL0:
		cal_ctl = ANA_REG_GLB_DCDCARM_CTRL_CAL;
		break;
	case ANA_REG_GLB_DCDCMEM_CTRL0:
		cal_ctl = ANA_REG_GLB_DCDCMEM_CTRL_CAL;
		break;
	case ANA_REG_GLB_DCDCLDO_CTRL0:
		cal_ctl = ANA_REG_GLB_DCDCLDO_CTRL_CAL;
		break;
	default:
		break;
	}
	return cal_ctl;
}

static int __dcdc_get_small_voltage(u32 vol_ctl)
{
	int cal_vol = 0;
	u32 cal_ctl = __dcdc_get_cal_ctl(vol_ctl);

	/* dcdc calibration control bits (default 00000),
	 * small adjust voltage: 100/32mv ~= 3.125mv
	 */
	if (cal_ctl) {
		cal_vol = sci_adi_read(cal_ctl) & BITS_DCDC_CAL(-1);
		return DIV_ROUND_CLOSEST(cal_vol * 100, 32);
	}
	return 0;
}

static int __dcdc_set_small_voltage(u32 vol_ctl, int cal_vol)
{
	int i = DIV_ROUND_CLOSEST(cal_vol * 32, 100) % 32;
	u32 cal_ctl = __dcdc_get_cal_ctl(vol_ctl);

	if (cal_ctl) {
		sci_adi_raw_write(cal_ctl,
				  BITS_DCDC_CAL(i) |
				  BITS_DCDC_CAL_RST(BITS_DCDC_CAL(-1) - i));
	}
	return 0;
}

int dcdc_calibrate(int adc_chan, int def_vol, int to_vol)
{
	int ret;
	int adc_vol, ctl_vol, cal_vol = 0;
	struct regulator *dcdc = 0;
	const char *id = NULL;

	switch (adc_chan) {
	case ADC_CHANNEL_DCDCCORE:
		cal_vol = 1100;
		id = "vddcore";
		break;
	case ADC_CHANNEL_DCDCARM:
		cal_vol = 1200;
		id = "vddarm";
		break;
	case ADC_CHANNEL_DCDCMEM:
		if (is_ddr2)
			cal_vol = 1200;
		else;		/*FIXME: */
		id = "vddmem";
		break;
	case ADC_CHANNEL_DCDCLDO:
		cal_vol = 2200;
		id = "dcdcldo";
		break;
	default:
		break;
	}
	if (NULL == id)
		goto exit;

	dcdc = regulator_get(0, id);
	if (IS_ERR(dcdc))
		goto exit;

	if (!def_vol) {
		ctl_vol = regulator_get_voltage(dcdc);
		if (IS_ERR_VALUE(ctl_vol)) {
			info("no valid %s vol ctrl bits\n", id);
			def_vol = cal_vol;
		} else		/* dcdc maybe had been adjusted in uboot-spl */
			def_vol = ctl_vol / 1000;
	}

	if (!def_vol)
		goto exit;

	adc_vol = dcdc_adc_get(adc_chan);

	info("%s default %dmv, from %dmv to %dmv\n", __FUNCTION__, def_vol,
	     adc_vol, to_vol);

	cal_vol = abs(adc_vol - to_vol);
	if (cal_vol > 200 /* mv */ )
		goto exit;
	else if (cal_vol < to_vol / 100) {
		info("%s %s is ok\n", __FUNCTION__, id);
		regulator_put(dcdc);
		return 0;
	}

	ctl_vol = DIV_ROUND_CLOSEST(def_vol * to_vol, adc_vol);

	ret = regulator_set_voltage(dcdc, ctl_vol * 1000, ctl_vol * 1000);
	if (IS_ERR_VALUE(ret))
		goto exit;

	regulator_put(dcdc);
	return ctl_vol;

exit:
	info("%s failure\n", __FUNCTION__);
	regulator_put(dcdc);
	return -1;
}

int mpll_calibrate(int cpu_freq)
{
	u32 val = 0;
	unsigned long flags;
	//BUG_ON(cpu_freq != 1200);     /* only upgrade 1.2G */
	cpu_freq /= 4;
	flags = hw_local_irq_save();
	val = sci_glb_raw_read(REG_GLB_M_PLL_CTL0);
	if ((val & MASK_MPLL_N) == cpu_freq)
		goto exit;
	val = (val & ~MASK_MPLL_N) | cpu_freq;
	sci_glb_set(REG_GLB_GEN1, BIT_MPLL_CTL_WE);	/* mpll unlock */
	sci_glb_write(REG_GLB_M_PLL_CTL0, val, MASK_MPLL_N);
	sci_glb_clr(REG_GLB_GEN1, BIT_MPLL_CTL_WE);
exit:
	hw_local_irq_restore(flags);
	debug("%s 0x%08x\n", __FUNCTION__, val);
	return 0;
}

struct dcdc_delayed_work {
	struct delayed_work work;
	u32 uptime;
	int cal_typ;
};

static struct dcdc_delayed_work dcdc_work = {
	.work.work.func = NULL,
	.uptime = 0,
	.cal_typ = 0,
};

static u32 sci_syst_read(void)
{
	u32 t = __raw_readl(REG_SYST_VALUE);
	while (t != __raw_readl(REG_SYST_VALUE))
		t = __raw_readl(REG_SYST_VALUE);
	return t;
}

static void do_dcdc_work(struct work_struct *work)
{
	int ret, cnt = CALIBRATE_TO;
	int dcdc_to_vol = 1100;	/* vddcore */
	int dcdcarm_to_vol = 1200;	/* vddarm */
	int dcdcldo_to_vol = 2200;	/* dcdcldo */
	int dcdcmem_to_vol = 1200;	/* dcdcmem */
	int cpu_freq = 1000;	/* Mega */
	u32 val = 0;

	/* debug("%s %d\n", __FUNCTION__, sprd_get_adc_cal_type()); */
	if (dcdc_work.cal_typ == sprd_get_adc_cal_type())
		goto exit;	/* no change, set next delayed work */

	val = sci_efuse_get(5);
	debug("%s efuse flag 0x%08x, mpll %08x\n", __FUNCTION__, val,
	      __raw_readl(REG_GLB_M_PLL_CTL0));

	if (val & BIT(16) /*1.2G flag */ ) {
		dcdc_to_vol = 1200;
		dcdcarm_to_vol = 1250;
		cpu_freq = 1200;
	}

	if (is_ddr2) {
		dcdcmem_to_vol = 1200;
	}

	dcdc_work.cal_typ = sprd_get_adc_cal_type();
	debug("%s %d %d\n", __FUNCTION__, dcdc_work.cal_typ, cnt);

	ret = dcdc_calibrate(ADC_CHANNEL_DCDCCORE, 0, dcdc_to_vol);
	if (ret > 0)
		dcdc_calibrate(ADC_CHANNEL_DCDCCORE, ret, dcdc_to_vol);

	ret = dcdc_calibrate(ADC_CHANNEL_DCDCARM, 0, dcdcarm_to_vol);
	if (ret > 0)
		dcdc_calibrate(ADC_CHANNEL_DCDCARM, ret, dcdcarm_to_vol);

	ret = dcdc_calibrate(ADC_CHANNEL_DCDCLDO, 0, dcdcldo_to_vol);
	if (ret > 0)
		dcdc_calibrate(ADC_CHANNEL_DCDCLDO, ret, dcdcldo_to_vol);

	ret = dcdc_calibrate(ADC_CHANNEL_DCDCMEM, 0, dcdcmem_to_vol);
	if (ret > 0)
		dcdc_calibrate(ADC_CHANNEL_DCDCMEM, ret, dcdcmem_to_vol);

exit:
	if (sci_syst_read() - dcdc_work.uptime < CALIBRATE_TO * 1000) {
		schedule_delayed_work(&dcdc_work.work, msecs_to_jiffies(1000));
	} else {
		info("%s end\n", __FUNCTION__);
	}

	if (cpu_freq == 1200) {
		msleep(100);
		mpll_calibrate(cpu_freq);
	}
	return;
}

void dcdc_calibrate_callback(void *data)
{
	if (!dcdc_work.work.work.func) {
		INIT_DELAYED_WORK(&dcdc_work.work, do_dcdc_work);
		dcdc_work.uptime = sci_syst_read();
	}
	schedule_delayed_work(&dcdc_work.work, msecs_to_jiffies(10));
}

static int __init dcdc_init(void)
{
	u32 chan_numerators, chan_denominators;
	sci_adc_get_vol_ratio(ADC_CHANNEL_VBAT, 0, &bat_numerators,
			      &bat_denominators);
	sci_adc_get_vol_ratio(ADC_CHANNEL_DCDCCORE, true, &chan_numerators,
			      &chan_denominators);
	is_ddr2 = 0 == (sci_adi_read(ANA_REG_GLB_ANA_STATUS) & BIT(6));
	info("vbat chan sampling ratio %u/%u, and dcdc %u/%u, %s\n",
	     bat_numerators, bat_denominators,
	     chan_numerators, chan_denominators, is_ddr2 ? "ddr2" : "ddr");

	/* setup small adjust */
	dcdc_get_small_voltage = __dcdc_get_small_voltage;
	dcdc_set_small_voltage = __dcdc_set_small_voltage;
	dcdc_calibrate_callback(0);
	return 0;
}

EXPORT_SYMBOL(dcdc_calibrate);
EXPORT_SYMBOL(mpll_calibrate);
late_initcall(dcdc_init);
