/* drivers/input/touchscreen/mms_ts.c
 *
 * Copyright (C) 2010 Melfas, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define SEC_TSP
#ifdef SEC_TSP
#define SEC_TSP_FACTORY_TEST
#define TSP_FACTORY_TEST
#define TSP_BUF_SIZE 1024
#undef ENABLE_NOISE_TEST_MODE
#if defined(CONFIG_KOR_MODEL_SHV_E120S) \
	|| defined(CONFIG_KOR_MODEL_SHV_E120K) \
	|| defined(CONFIG_KOR_MODEL_SHV_E120L) \
	|| defined(CONFIG_KOR_MODEL_SHV_E160S) \
	|| defined(CONFIG_KOR_MODEL_SHV_E160K) \
	|| defined(CONFIG_KOR_MODEL_SHV_E160L)
#define TSP_BOOST
#else
#undef TSP_BOOST
#endif
#endif
#undef TA_DETECTION
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/input/mms134s_ts.h>
#include <linux/cpufreq.h>
#ifdef SEC_TSP
#include <linux/gpio.h>
#endif
#if 1
#include <linux/miscdevice.h>
#include <linux/ioctl.h>
#include <linux/string.h>
#include <linux/semaphore.h>
#include <linux/kthread.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/firmware.h>
#include <linux/input/mt.h>
#include <asm/io.h>
#include <mach/gpio.h>
#include <linux/regulator/machine.h>
#endif
#include <mach/regulator.h>

#define TS_MAX_Z_TOUCH			255
#define TS_MAX_W_TOUCH		100
#define TS_MAX_X_COORD		480
#define TS_MAX_Y_COORD		800
#ifdef SEC_TSP
#define P5_THRESHOLD			0x05
//#define TS_READ_REGS_LEN		5
#define TS_WRITE_REGS_LEN		16
#endif
#ifdef CONFIG_SEC_DVFS
#define TOUCH_BOOSTER			1
#define TOUCH_BOOSTER_OFF_TIME	100
#define TOUCH_BOOSTER_CHG_TIME	200
#else
#define TOUCH_BOOSTER			0
#endif
#define TS_READ_REGS_LEN		66
#define MELFAS_MAX_TOUCH		5
static int FW_VERSION;
#define DEBUG_PRINT			0			// need to delete after completed PV
#define PRESS_KEY					1
#define RELEASE_KEY					0
#define SET_DOWNLOAD_BY_GPIO	1
#define TS_READ_VERSION_ADDR		0x1B
#define DOWNLOAD_RETRY_CNT		1
#define MIP_CONTACT_ON_EVENT_THRES	0x05
#define MIP_MOVING_EVENT_THRES		0x06
#define MIP_ACTIVE_REPORT_RATE		0x07
#define MIP_POSITION_FILTER_LEVEL	0x08
#define TS_READ_START_ADDR			0x0F
#define TS_READ_START_ADDR2			0x10
#define MIP_TSP_REVISION				0xF0
#define MIP_HARDWARE_REVISION		0xF1
#define MIP_COMPATIBILITY_GROUP		0xF2
#define MIP_CORE_VERSION				0xF3
#define MIP_PRIVATECUSTOM_VERSION	0xF4
#define MIP_PUBLICCUSTOM_VERSION		0xF5
#define MIP_PRODUCT_CODE				0xF6
#ifdef SEC_TSP_FACTORY_TEST
#define TX_NUM		19
#define RX_NUM		12
#define NODE_NUM	228 /* 19x12 */
/* VSC(Vender Specific Command)  */
#define MMS_VSC_CMD			0xB0	/* vendor specific command */
#define MMS_VSC_MODE			0x1A	/* mode of vendor */
#define MMS_VSC_CMD_ENTER		0X01
#define MMS_VSC_CMD_CM_DELTA		0X02
#define MMS_VSC_CMD_CM_ABS		0X03
#define MMS_VSC_CMD_EXIT		0X05
#define MMS_VSC_CMD_INTENSITY		0X04
#define MMS_VSC_CMD_RAW			0X06
#define MMS_VSC_CMD_REFER		0X07
#define VSC_INTENSITY_TK		0x14
#define VSC_RAW_TK			0x16
#define VSC_THRESHOLD_TK		0x18
#define TSP_CMD_STR_LEN			32
#define TSP_CMD_RESULT_STR_LEN		512
#define TSP_CMD_PARAM_NUM		8
#endif /* SEC_TSP_FACTORY_TEST */
#define SET_TSP_CONFIG
#define TSP_PATTERN_TRACTKING
#undef TSP_PATTERN_TRACTKING
#if SET_DOWNLOAD_BY_GPIO
#include "MMS100S_ISC_Updater.h"
#endif
#define TS_READ_EXCITING_CH_ADDR	0x2E
#define TS_READ_SENSING_CH_ADDR	    0x2F
#define TS_WRITE_REGS_LEN		    16
#define RMI_ADDR_TESTMODE           0xA0
#define UNIVERSAL_CMD_RESULT_SIZE   0xAE
#define UNIVERSAL_CMD_RESULT        0xAF
//#define	TSP_PWR_LDO_GPIO	41

#define GPIO_I2C_INT 60
#define G1M_V34_CRC	0
#define G1F_V19_CRC	47
#define G1F_V20_CRC 88
#define GFF_V19_CRC 76
#define GFF_V19H_CRC 173
#define GFF_V19H2_CRC 64
#define GFF_V19H3_CRC 155
#define GFF_V20_CRC 206
unsigned long saved_rate;
//static bool lock_status;
static int tsp_enabled;
int touch_is_pressed;
static int tsp_testmode;
static int index;
//static int TA_enable;
static int gMenuKey_Intensity, gBackKey_Intensity;
extern unsigned int board_hw_revision;
extern unsigned char IC_type;

unsigned char TSP_PanelVersion, TSP_PhoneVersion;

#if defined(CONFIG_MACH_KYLETD) || defined(CONFIG_MACH_VASTOI)
#define MAX_RX_	12
#define MAX_TX_	19
static const uint16_t SCR_ABS_UPPER_SPEC[MAX_RX_][MAX_TX_] = {
	{3171,3304,3323,3339,3352,3362,3365,3369,3371,3371,3381,3383,3385,3387,3388,3389,3390,3390,3377},
	{3187,3308,3320,3339,3349,3360,3362,3367,3368,3370,3377,3379,3381,3383,3384,3385,3385,3386,3372},
	{3199,3315,3334,3351,3367,3377,3383,3388,3393,3397,3403,3406,3409,3410,3411,3411,3411,3411,3380},
	{3212,3325,3337,3355,3365,3377,3379,3383,3384,3387,3392,3394,3396,3398,3399,3400,3400,3400,3387},
	{3218,3324,3340,3355,3368,3377,3381,3383,3386,3388,3391,3394,3396,3398,3399,3400,3400,3400,3386},
	{3220,3326,3339,3356,3367,3377,3380,3383,3385,3388,3391,3394,3396,3398,3399,3399,3400,3400,3386},
	{3214,3320,3334,3350,3362,3370,3374,3377,3379,3382,3384,3387,3389,3391,3392,3393,3394,3394,3379},
	{3210,3320,3334,3350,3362,3370,3373,3376,3379,3382,3384,3387,3389,3391,3392,3392,3393,3393,3379},
	{3208,3320,3335,3350,3363,3370,3373,3376,3379,3381,3382,3386,3388,3390,3391,3392,3393,3393,3379},
	{3204,3321,3338,3358,3372,3381,3385,3391,3396,3402,3404,3408,3410,3412,3412,3412,3413,3412,3379},
	{3198,3314,3329,3345,3357,3365,3368,3370,3373,3376,3373,3378,3380,3382,3383,3384,3385,3385,3373},
	{3208,3324,3338,3354,3365,3373,3375,3378,3380,3383,3350,3376,3382,3384,3385,3384,3387,3388,3375},
};

static const uint16_t SCR_ABS_LOWER_SPEC[MAX_RX_][MAX_TX_] = {
	{1903,1983,1994,2003,2011,2017,2019,2021,2022,2023,2028,2030,2031,2032,2033,2033,2034,2034,2026},
	{1912,1985,1992,2003,2009,2016,2017,2020,2021,2022,2026,2027,2029,2030,2030,2031,2031,2032,2023},
	{1919,1989,2000,2011,2020,2026,2030,2033,2036,2038,2042,2044,2045,2046,2046,2047,2047,2046,2028},
	{1927,1995,2002,2013,2019,2026,2027,2030,2031,2032,2035,2036,2038,2039,2039,2040,2040,2040,2032},
	{1931,1995,2004,2013,2021,2026,2028,2030,2031,2033,2035,2036,2037,2039,2039,2040,2040,2040,2032},
	{1932,1995,2003,2013,2020,2026,2028,2030,2031,2033,2035,2036,2038,2039,2039,2040,2040,2040,2032},
	{1929,1992,2001,2010,2017,2022,2024,2026,2028,2029,2030,2032,2034,2035,2035,2036,2036,2036,2028},
	{1926,1992,2000,2010,2017,2022,2024,2026,2027,2029,2030,2032,2033,2034,2035,2035,2036,2036,2027},
	{1925,1992,2001,2010,2018,2022,2024,2026,2027,2029,2029,2031,2033,2034,2034,2035,2036,2036,2027},
	{1922,1993,2003,2015,2023,2028,2031,2035,2038,2041,2042,2045,2046,2047,2047,2047,2048,2047,2027},
	{1919,1988,1997,2007,2014,2019,2021,2022,2024,2026,2024,2027,2028,2029,2030,2030,2031,2031,2024},
	{1925,1994,2003,2012,2019,2024,2025,2027,2028,2030,2010,2026,2029,2030,2031,2031,2032,2033,2025},
};

#else	/* CONFIG_MACH_KYLE */
#define MAX_RX_	10
#define MAX_TX_	15
static const uint16_t SCR_ABS_UPPER_SPEC[MAX_RX_][MAX_TX_] = {
	{3575,  3495,  3470,  3459,  3444},
	{3438,  3426,  3424,  3419,  3419},
	{3419,  3415,  3411,  3410,  3370},
	{3548,  3488,  3459,  3444,  3424},
	{3413,  3400,  3391,  3389,  3383},
	{3379,  3378,  3375,  3373,  3379},
	{3554,  3500,  3475,  3464,  3448},
	{3441,  3431,  3430,  3424,  3423},
	{3423,  3420,  3418,  3416,  3374},
	{3550,  3500,  3475,  3465,  3449},
	{3444,  3433,  3431,  3428,  3424},
	{3424,  3423,  3420,  3419,  3376},
	{3548,  3501,  3476,  3466,  3450},
	{3444,  3433,  3433,  3429,  3425},
	{3425,  3424,  3421,  3420,  3378},
	{3549,  3499,  3475,  3463,  3449},
	{3441,  3431,  3431,  3428,  3424},
	{3424,  3423,  3420,  3419,  3376},
	{3555,  3499,  3476,  3461,  3450},
	{3440,  3434,  3433,  3428,  3425},
	{3425,  3424,  3423,  3420,  3378},
	{3556,  3499,  3476,  3463,  3450},
	{3441,  3434,  3434,  3430,  3425},
	{3425,  3425,  3424,  3423,  3380},
	{3549,  3486,  3459,  3440,  3424},
	{3410,  3401,  3399,  3391,  3389},
	{3384,  3380,  3380,  3379,  3385},
	{3555,  3488,  3470,  3453,  3444},
	{3433,  3429,  3450,  3429,  3423},
	{3423,  3421,  3421,  3420,  3380},
};
static const uint16_t SCR_ABS_LOWER_SPEC[MAX_RX_][MAX_TX_] = {
	{2145, 2097, 2082, 2075, 2066},
	{2063, 2056, 2054, 2051, 2051},
	{2051, 2049, 2047, 2046, 2022},
	{2129, 2093, 2075, 2066, 2054},
	{2048, 2040, 2035, 2033, 2030},
	{2027, 2027, 2025, 2024, 2027},
	{2132, 2100, 2085, 2078, 2069},
	{2065, 2059, 2058, 2054, 2054},
	{2054, 2052, 2051, 2050, 2024},
	{2130, 2100, 2085, 2079, 2069},
	{2066, 2060, 2059, 2057, 2054},
	{2054, 2054, 2052, 2051, 2026},
	{2129, 2101, 2086, 2080, 2070},
	{2066, 2060, 2060, 2057, 2055},
	{2055, 2054, 2053, 2052, 2027},
	{2129, 2099, 2085, 2078, 2069},
	{2065, 2059, 2059, 2057, 2054},
	{2054, 2054, 2052, 2051, 2026},
	{2133, 2099, 2086, 2077, 2070},
	{2064, 2060, 2060, 2057, 2055},
	{2055, 2054, 2054, 2052, 2027},
	{2134, 2099, 2086, 2078, 2070},
	{2065, 2060, 2060, 2058, 2055},
	{2055, 2055, 2054, 2054, 2028},
	{2129, 2092, 2075, 2064, 2054},
	{2046, 2041, 2039, 2035, 2033},
	{2030, 2028, 2028, 2027, 2031},
	{2133, 2093, 2082, 2072, 2066},
	{2060, 2057, 2070, 2057, 2054},
	{2054, 2053, 2053, 2052, 2028},
};
#endif

static int g_exciting_ch, g_sensing_ch;
static unsigned char is_inputmethod;
#ifdef TSP_BOOST
static unsigned char is_boost;
#endif
enum {
	None = 0,
	TOUCH_SCREEN,
	TOUCH_KEY
};
struct muti_touch_info {
	int strength;
	int width;
	int posX;
	int posY;
};
struct mms_ts_data {
	uint16_t addr;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct mms_tsi_platform_data *pdata;
	struct work_struct  work;
	struct mms_version *version;
	uint32_t flags;
	int (*power)(int on);
	int (*gpio)(void);
	const u8			*config_fw_version;
	int				irq;
#ifdef TA_DETECTION
	void (*register_cb)(void *);
	void (*read_ta_status)(void *);
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif

#if TOUCH_BOOSTER
	struct delayed_work work_dvfs_off;
	struct delayed_work work_dvfs_chg;
	bool	dvfs_lock_status;
	struct mutex dvfs_lock;
#endif

	struct mutex			lock;
	bool				enabled;
	bool			noise_mode;
	bool			ta_status;
	u8				fw_ic_ver;
#if defined(SEC_TSP_FACTORY_TEST)
	struct list_head			cmd_list_head;
	u8			cmd_state;
	char			cmd[TSP_CMD_STR_LEN];
	int			cmd_param[TSP_CMD_PARAM_NUM];
	char			cmd_result[TSP_CMD_RESULT_STR_LEN];
	struct mutex			cmd_lock;
	bool			cmd_is_running;
	unsigned int reference[NODE_NUM];
	unsigned int raw[NODE_NUM]; /* CM_ABS */
	unsigned int inspection[NODE_NUM];/* CM_DELTA */
	unsigned int intensity[NODE_NUM];
	bool ft_flag;
#endif /* SEC_TSP_FACTORY_TEST */
};
static struct mms_ts_data *ts = NULL;
#ifdef SEC_TSP
extern struct class *sec_class;
struct device *sec_touchscreen_dev;
struct device *sec_touchkey_dev;
static struct regulator *touch_regulator_1v8 = NULL;
static struct regulator *touch_regulator_3v0 =  NULL;
int menu_pressed;
int back_pressed;
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
static void mms_ts_early_suspend(struct early_suspend *h);
static void mms_ts_late_resume(struct early_suspend *h);
#endif
#if defined(SEC_TSP_FACTORY_TEST)
#define TSP_CMD(name, func) .cmd_name = name, .cmd_func = func
struct tsp_cmd {
	struct list_head	list;
	const char	*cmd_name;
	void	(*cmd_func)(void *device_data);
};
static void fw_update(void *device_data);
static void get_fw_ver_bin(void *device_data);
static void get_fw_ver_ic(void *device_data);
static void get_config_ver(void *device_data);
static void get_threshold(void *device_data);
static void module_off_master(void *device_data);
static void module_on_master(void *device_data);
static void get_chip_vendor(void *device_data);
static void get_chip_name(void *device_data);
static void get_reference(void *device_data);
static void get_cm_abs(void *device_data);
static void get_cm_delta(void *device_data);
static void get_intensity(void *device_data);
static void get_x_num(void *device_data);
static void get_y_num(void *device_data);
static void run_reference_read(void *device_data);
static void run_cm_abs_read(void *device_data);
static void run_cm_delta_read(void *device_data);
static void run_intensity_read(void *device_data);
static void not_support_cmd(void *device_data);
static int check_delta_value(struct mms_ts_data *ts);
extern eMFSRet_t check_firmware_version_func(void);
extern eMFSRet_t MFS_ISC_force_update(void);
struct tsp_cmd tsp_cmds[] = {
	{TSP_CMD("fw_update", fw_update),},
	{TSP_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{TSP_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{TSP_CMD("get_config_ver", get_config_ver),},
	{TSP_CMD("get_threshold", get_threshold),},
	{TSP_CMD("module_off_master", module_off_master),},
	{TSP_CMD("module_on_master", module_on_master),},
	{TSP_CMD("module_off_slave", not_support_cmd),},
	{TSP_CMD("module_on_slave", not_support_cmd),},
	{TSP_CMD("get_chip_vendor", get_chip_vendor),},
	{TSP_CMD("get_chip_name", get_chip_name),},
	{TSP_CMD("get_x_num", get_x_num),},
	{TSP_CMD("get_y_num", get_y_num),},
	{TSP_CMD("get_reference", get_reference),},
	{TSP_CMD("get_cm_abs", get_cm_abs),},
	{TSP_CMD("get_cm_delta", get_cm_delta),},
	{TSP_CMD("get_intensity", get_intensity),},
	{TSP_CMD("run_reference_read", run_reference_read),},
	{TSP_CMD("run_cm_abs_read", run_cm_abs_read),},
	{TSP_CMD("run_cm_delta_read", run_cm_delta_read),},
	{TSP_CMD("run_intensity_read", run_intensity_read),},
	{TSP_CMD("not_support_cmd", not_support_cmd),},
};
#endif

#if TOUCH_BOOSTER
static void change_dvfs_lock(struct work_struct *work)
{
	struct mms_ts_data *ts = container_of(work,
				struct mms_ts_data, work_dvfs_chg.work);
	int ret;
	mutex_lock(&ts->dvfs_lock);
	ret = set_freq_limit(DVFS_TOUCH_ID, 1008000);
	mutex_unlock(&ts->dvfs_lock);

	if (ret < 0)
		printk(KERN_ERR "%s: 1booster stop failed(%d)\n",\
					__func__, __LINE__);
	else
		printk(KERN_INFO "[TSP] %s", __func__);
}

static void set_dvfs_off(struct work_struct *work)
{
	struct mms_ts_data *ts = container_of(work,
				struct mms_ts_data, work_dvfs_off.work);
	mutex_lock(&ts->dvfs_lock);
	set_freq_limit(DVFS_TOUCH_ID, -1);
	ts->dvfs_lock_status = false;
	mutex_unlock(&ts->dvfs_lock);

}

static void set_dvfs_lock(struct mms_ts_data *ts, uint32_t on)
{
	int ret = 0;

	mutex_lock(&ts->dvfs_lock);
	if (on == 0) {
		if (ts->dvfs_lock_status) {
			schedule_delayed_work(&ts->work_dvfs_off,
				msecs_to_jiffies(TOUCH_BOOSTER_OFF_TIME));
		}
	} else if (on == 1) {
		cancel_delayed_work(&ts->work_dvfs_off);
		if (!ts->dvfs_lock_status) {
			ret = set_freq_limit(DVFS_TOUCH_ID, 1008000);
			if (ret < 0)
				printk(KERN_ERR "%s: cpu lock failed(%d)\n",\
							__func__, ret);

			ts->dvfs_lock_status = true;
		}
	} else if (on == 2) {
		cancel_delayed_work(&ts->work_dvfs_off);
		schedule_work(&ts->work_dvfs_off.work);
	}
	mutex_unlock(&ts->dvfs_lock);
}
#endif

#ifdef SEC_TSP
static int mms_ts_suspend(struct i2c_client *client, pm_message_t mesg);
static int mms_ts_resume(struct i2c_client *client);
static void release_all_fingers(struct mms_ts_data *ts);
static int mms_set_config(struct i2c_client *client, u8 reg, u8 value);
static int mms_i2c_write(struct i2c_client *client, char *buf, int length);
static void TSP_reboot(void);
#endif
int mms_fw_i2c_read(u16 addr, u8 *value, u16 length);
int mms_fw_i2c_write(char *buf, int length);
#if 0
static ssize_t	check_init_lowleveldata();
#endif
static struct muti_touch_info g_Mtouch_info[MELFAS_MAX_TOUCH];
#define VREG_ENABLE		1
#define VREG_DISABLE	0
#define TOUCH_ON  1
#define TOUCH_OFF 0
static void ts_power_enable(int en)
{
	int ret=0;
	printk(KERN_ERR "%s %s\n", __func__, (en) ? "on" : "off");

#if 0//def CONFIG_MACH_KYLE_I
	int rc;
	struct vreg *vreg_lcd = NULL;

	printk(KERN_ERR "start %s\n", __func__);
	if (vreg_lcd == NULL) {
		vreg_lcd = regulator_get(NULL, "vlcd");

		if (IS_ERR(vreg_lcd)) {
			printk(KERN_ERR "%s: vreg_get(%s) failed (%ld)\n",
				__func__, "vlcd4", PTR_ERR(vreg_lcd));
			return;
		}

		rc = regulator_set_voltage(vreg_lcd, 3000000, 3000000);
		if (rc) {
			printk(KERN_ERR "%s: TSP set_level failed (%d)\n",
				__func__, rc);
		}
	}

	if (en) {
		rc = regulator_enable(vreg_lcd);
		if (rc) {
			printk(KERN_ERR "%s: TSP enable failed (%d)\n",
				__func__, rc);
		} else {
			printk(KERN_ERR "%s: TSP enable success (%d)\n",
				__func__, rc);
		}
	} else {
		rc = regulator_disable(vreg_lcd);
		if (rc) {
			printk(KERN_ERR "%s: TSP disable failed (%d)\n",
				__func__, rc);
		} else {
			printk(KERN_ERR "%s: TSP disable success (%d)\n",
				__func__, rc);
		}
	}
#else
/*	gpio_request(TSP_PWR_LDO_GPIO, "tsp-power");
	if (en) {
		gpio_direction_output(TSP_PWR_LDO_GPIO, 1);
	} else {
		gpio_direction_output(TSP_PWR_LDO_GPIO, 0);
	}
	gpio_free(TSP_PWR_LDO_GPIO);
*/

	printk("[TSP] %s, %d\n", __func__, en );

	if(touch_regulator_1v8 == NULL) {
		touch_regulator_1v8 = regulator_get(NULL,REGU_NAME_TP1);
		if (IS_ERR(touch_regulator_1v8)) {
			touch_regulator_1v8 = NULL;
			printk("get touch_regulator_1v8 regulator error\n");
			return;
		}
	}
	if(touch_regulator_3v0 == NULL) {
		touch_regulator_3v0 = regulator_get(NULL,REGU_NAME_TP);
		if (IS_ERR(touch_regulator_3v0)) {
			touch_regulator_3v0 = NULL;
			printk("get touch_regulator_3v0 regulator error\n");
			return;
		}
	}

	if(en == TOUCH_ON) {
		regulator_set_voltage(touch_regulator_1v8, 1800000, 1800000);
		ret = regulator_enable(touch_regulator_1v8);
		if (ret) {
			printk(KERN_ERR "%s: touch_regulator_1v8 enable failed (%d)\n",__func__, ret);
		}
		regulator_set_voltage(touch_regulator_3v0, 3300000, 3300000);
		ret = regulator_enable(touch_regulator_3v0);
		if (ret) {
			printk(KERN_ERR "%s: touch_regulator_3v3 enable failed (%d)\n",__func__, ret);
		}
	}
	else 	{
		ret = regulator_disable(touch_regulator_1v8);
		if (ret) {
			printk(KERN_ERR "%s: touch_regulator_1v8 disable failed (%d)\n",__func__, ret);
		}
		ret = regulator_disable(touch_regulator_3v0);
		if (ret) {
			printk(KERN_ERR "%s: touch_regulator_3v3 disable failed (%d)\n",__func__, ret);
		}  
	}
#endif
}

void ts_power_control(int en)
{
	ts_power_enable(en);
}
EXPORT_SYMBOL(ts_power_control);
#if 0
static int mms_init_panel(struct mms_ts_data *ts)
{
	u8 buf = 0x00;
	int ret;
	ret = i2c_master_send(ts->client, &buf, 1);
	ret = i2c_master_send(ts->client, &buf, 1);
	if (ret < 0) {
		printk(KERN_ERR "%s : i2c_master_send() failed\n [%d]", __func__, ret);
		return 0;
	}
	return true;
}
#endif
#ifdef TA_DETECTION
static void tsp_ta_probe(int ta_status)
{
	u8 write_buffer[3];
	printk(KERN_ERR"[TSP] %s : TA is %s. \n", __func__, ta_status ? "on" : "off");
	if (tsp_enabled == false) {
		printk(KERN_ERR"[TSP] tsp_enabled is 0\n");
		return;
	}
	write_buffer[0] = 0xB0;
	write_buffer[1] = 0x11;
	if (ta_status)
		write_buffer[2] = 1;
	else
		write_buffer[2] = 0;
	mms_i2c_write(ts_data->client, (char *)write_buffer, 3);
}
#endif
#ifdef TSP_BOOST
static void TSP_boost(struct mms_ts_data *ts, bool onoff)
{
	printk(KERN_ERR "[TSP] TSP_boost %s\n", is_boost ? "ON" : "Off");
	if (onoff) {
		mms_set_config(ts->client, MIP_POSITION_FILTER_LEVEL, 2);
	} else {
		mms_set_config(ts->client, MIP_POSITION_FILTER_LEVEL, 80);
	}
}
#endif
#ifdef TSP_PATTERN_TRACTKING
/* To do forced calibration when ghost touch occured at the same point
    for several second. */
#define MAX_GHOSTCHECK_FINGER				10
#define MAX_GHOSTTOUCH_COUNT					300
#define MAX_GHOSTTOUCH_BY_PATTERNTRACKING	5
static int tcount_finger[MAX_GHOSTCHECK_FINGER] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int touchbx[MAX_GHOSTCHECK_FINGER] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int touchby[MAX_GHOSTCHECK_FINGER] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int ghosttouchcount;
static int cFailbyPattenTracking;
static void clear_tcount(void)
{
	int i;
	for (i = 0; i < MAX_GHOSTCHECK_FINGER; i++) {
		tcount_finger[i] = 0;
		touchbx[i] = 0;
		touchby[i] = 0;
	}
}
static int diff_two_point(int x, int y, int oldx, int oldy)
{
	int diffx, diffy;
	int distance;
	diffx = x-oldx;
	diffy = y-oldy;
	distance = abs(diffx) + abs(diffy);
	if (distance < 3)
		return 1;
	else
		return 0;
}
static int tsp_pattern_tracking(struct mms_ts_data *ts, int fingerindex, int x, int y)
{
	int i;
	int ghosttouch = 0;
	if (i == fingerindex) {
		if (diff_two_point (x, y, touchbx[i], touchby[i])) {
			tcount_finger[i] = tcount_finger[i]+1;
		} else {
			tcount_finger[i] = 0;
		}
		touchbx[i] = x;
		touchby[i] = y;
		if (tcount_finger[i] > MAX_GHOSTTOUCH_COUNT) {
			ghosttouch = 1;
			ghosttouchcount++;
			printk(KERN_DEBUG "[TSP] SUNFLOWER (PATTERN TRACKING) %d\n", ghosttouchcount);
			clear_tcount();
			cFailbyPattenTracking++;
			if (cFailbyPattenTracking > MAX_GHOSTTOUCH_BY_PATTERNTRACKING) {
				cFailbyPattenTracking = 0;
				printk(KERN_INFO "[TSP] Reboot.\n");
				TSP_reboot();
			} else {
				/* Do something for calibration */
			}
		}
	}
	return ghosttouch;
}
#endif

static void mms_set_noise_mode(struct mms_ts_data *ts)
{
//	struct i2c_client *client = ts->client;
	int ret;
	u8 setLowLevelData[2];

	if (!ts->noise_mode)
		return;

	if (ts->ta_status) {
		printk(KERN_INFO "TA connect!!\n");
		setLowLevelData[0] = 0x30;
		setLowLevelData[1] = 0x01;
		ret = mms_i2c_write(ts->client, setLowLevelData, 2);
	} else {
		printk(KERN_INFO "TA disconnect!!\n");
		setLowLevelData[0] = 0x30;
		setLowLevelData[1] = 0x02;
		ret = mms_i2c_write(ts->client, setLowLevelData, 2);
		ts->noise_mode = 0;
	}
	msleep(10);
}

void charger_enable(int enable)
{
	if (ts == NULL)
		return;
	if (enable == 0)
		ts->ta_status = 0;
	else
		ts->ta_status = 1;

	mms_set_noise_mode(ts);
}

static void mms_ts_get_data(struct work_struct *work)
{
	struct mms_ts_data *ts = container_of(work, struct mms_ts_data, work);
	int ret = 0, i, j;
	uint8_t buf[TS_READ_REGS_LEN]={0,};
	int read_num, FingerID;
	int _touch_is_pressed, line;
	int keyID = 0, touchType = 0, touchState = 0;
	u8 setLowLevelData[2];
	
	if (tsp_enabled == false) {
		printk(KERN_ERR "[TSP ]%s. tsp_disabled.\n", __func__);
		msleep(500);
		return;
	}
#if DEBUG_PRINT
	printk(KERN_ERR "%s start\n", __func__);
	if (ts == NULL)
		printk(KERN_ERR "%s : TS NULL\n", __func__);
#endif
	for (j = 0; j < 3; j++) {
		buf[0] = TS_READ_START_ADDR;
		ret = i2c_master_send(ts->client, buf, 1);
		if (ret < 0) {
			line = __LINE__;
			goto tsp_error;
		}
		ret = i2c_master_recv(ts->client, buf, 1);
		if (ret < 0) {
			line = __LINE__;
			goto tsp_error;
		}
		read_num = buf[0];
		if (read_num < 60)
			break;
	}
	if (read_num > TS_READ_REGS_LEN)
		read_num = TS_READ_REGS_LEN;

	if (read_num > 0) {
		buf[0] = TS_READ_START_ADDR2;
		ret = i2c_master_send(ts->client, buf, 1);
		if (ret < 0) {
			line = __LINE__;
			goto tsp_error;
		}
		ret = i2c_master_recv(ts->client, buf, read_num);
		if (ret < 0) {
			line = __LINE__;
			goto tsp_error;
		}

		if (buf[0] == 0x0E) {
			printk(KERN_INFO "[TSP] noise mode enter!!\n");
			ts->noise_mode = 1 ;
			setLowLevelData[0] = 0x10;
			setLowLevelData[1] = 0x00;
			ret = mms_i2c_write(ts->client, setLowLevelData, 2);
			mms_set_noise_mode(ts);
			}
		for (i = 0; i < read_num; i = i + 6) {
			if (buf[i] == 0x0F) {
				printk(KERN_ERR "%s : ESD-DETECTED!!!\n", __func__);
				line = __LINE__;
				goto tsp_error;
			}
			touchType = (buf[i] >> 5) & 0x03;
			if (touchType == TOUCH_SCREEN) {
				FingerID = (buf[i] & 0x0F) - 1;
				#if DEBUG_PRINT
				printk("FingerID = %d\n",FingerID);
				#endif
				if(FingerID > MELFAS_MAX_TOUCH-1)
					FingerID = MELFAS_MAX_TOUCH-1;
				g_Mtouch_info[FingerID].posX = (uint16_t)(buf[i+1] & 0x0F) << 8 | buf[i+2];
				g_Mtouch_info[FingerID].posY = (uint16_t)(buf[i+1] & 0xF0) << 4 | buf[i+3];
				if ((buf[i] & 0x80) == 0)
					g_Mtouch_info[FingerID].strength = 0;
				else
					g_Mtouch_info[FingerID].strength = buf[i+4];
				g_Mtouch_info[FingerID].width = buf[i+5];
			}
			else if (touchType == TOUCH_KEY) {
				keyID = (buf[i] & 0x0F);
				touchState = (buf[i] & 0x80);
				gMenuKey_Intensity = 0;
				gBackKey_Intensity = 0;
				#if DEBUG_PRINT
				printk(KERN_ERR "[TSP] keyID : %d, touchstate : %d\n"
							, keyID, touchState);
				#endif
				if (keyID == 0x1) {
					if (touchState)
						menu_pressed = 1;
					else
						menu_pressed = 0;
					gMenuKey_Intensity = buf[i + 5];
					input_report_key(
							ts->input_dev, KEY_MENU,
							touchState ?
							PRESS_KEY : RELEASE_KEY);
				}
				if (keyID == 0x2) {
					if (touchState)
						back_pressed = 1;
					else
						back_pressed = 0;
					gBackKey_Intensity = buf[i + 5];
					input_report_key(
						ts->input_dev, KEY_BACK,
						touchState ?
						PRESS_KEY : RELEASE_KEY);
				}
			}
		}
	}
	_touch_is_pressed = 0;
	for (i = 0; i < MELFAS_MAX_TOUCH; i++) {
		if (g_Mtouch_info[i].strength == -1)
			continue;
#ifdef TSP_PATTERN_TRACTKING
		tsp_pattern_tracking(ts, i,  g_Mtouch_info[i].posX,  g_Mtouch_info[i].posY);
#endif
		if (g_Mtouch_info[i].strength) {
			input_mt_slot(ts->input_dev, i);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE, g_Mtouch_info[i].strength);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, g_Mtouch_info[i].posX);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, g_Mtouch_info[i].posY);
			input_report_key(ts->input_dev, BTN_TOUCH, g_Mtouch_info[i].strength);
		} else {
			input_mt_slot(ts->input_dev, i);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);
		}
#if DEBUG_PRINT
		printk(KERN_ERR "[TSP] ID, S, x, y, z, w : %d, %d, %d, %d,  %d %d\n",
		i, (g_Mtouch_info[i].strength > 0),
		g_Mtouch_info[i].posX, g_Mtouch_info[i].posY,
		g_Mtouch_info[i].strength, g_Mtouch_info[i].width);
#endif
		if (g_Mtouch_info[i].strength == 0)
			g_Mtouch_info[i].strength = -1;
		if (g_Mtouch_info[i].strength > 0)
			_touch_is_pressed = 1;
	}
	input_sync(ts->input_dev);
	touch_is_pressed = _touch_is_pressed;
	return;
tsp_error:
	printk(KERN_ERR "[TSP] %s: i2c failed(%d)\n", __func__, line);
	TSP_reboot();
}
static irqreturn_t mms_ts_irq_handler(int irq, void *handle)
{
	struct mms_ts_data *ts = (struct mms_ts_data *)handle;
#if DEBUG_PRINT
	printk(KERN_ERR "mms_ts_irq_handler\n");
#endif
	mms_ts_get_data(&ts->work);
#if TOUCH_BOOSTER
	set_dvfs_lock(ts, !!touch_is_pressed);
#endif
	return IRQ_HANDLED;
}
#ifdef SEC_TSP
static int mms_i2c_read(struct i2c_client *client, u16 addr, u16 length, u8 *value)
{
	struct i2c_adapter *adapter = client->adapter;
	struct i2c_msg msg[2];
	msg[0].addr  = client->addr;
	msg[0].flags = 0x00;
	msg[0].len   = 1;
	msg[0].buf   = (u8 *) &addr;
	msg[1].addr  = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len   = length;
	msg[1].buf   = (u8 *) value;
	if  (i2c_transfer(adapter, msg, 2) == 2)
		return 0;
	else
		return -EIO;
}
static int mms_i2c_read_without_addr(struct i2c_client *client,
	u16 length, u8 *value)
{
	struct i2c_adapter *adapter = client->adapter;
	struct i2c_msg msg[1];
	msg[0].addr  = client->addr;
	msg[0].flags = I2C_M_RD;
	msg[0].len   = length;
	msg[0].buf   = (u8 *) value;
	if  (i2c_transfer(adapter, msg, 1) == 1)
		return 0;
	else
		return -EIO;
}
static int mms_i2c_busrt_write(struct i2c_client *client, u16 length,
	u8 *value)
{
	struct i2c_adapter *adapter = client->adapter;
	struct i2c_msg msg[1];
	msg[0].addr  = client->addr;
	msg[0].flags = 0;
	msg[0].len   = length;
	msg[0].buf   = (u8 *) value;
	if  (i2c_transfer(adapter, msg, 1) == 1)
		return 0;
	else
		return -EIO;
}

static int mms_i2c_write(struct i2c_client *client, char *buf, int length)
{
	int i;
	char data[TS_WRITE_REGS_LEN];
	if (length > TS_WRITE_REGS_LEN) {
		pr_err("[TSP] size error - %s\n", __func__);
		return -EINVAL;
	}
	for (i = 0; i < length; i++)
		data[i] = *buf++;
	i = i2c_master_send(client, (char *)data, length);
	if (i == length)
		return length;
	else
		return -EIO;
}
int mms_fw_i2c_read(u16 addr, u8 *value, u16 length)
{
	if (mms_i2c_read(ts->client, addr, length, value) == 0)
		return 1;
	else
		return 0;
}
int mms_fw_i2c_write(char *buf, int length)
{
	int ret;
	ret = mms_i2c_write(ts->client, buf, length);
	printk(KERN_ERR "<MELFAS> mass erase start write ret%d\n\n", ret);
	if (ret > 0)
		return 1;
	else
		return 0;
}
int mms_fw_i2c_read_without_addr(u8 *value, u16 length)
{
	if (mms_i2c_read_without_addr(ts->client, length, value) == 0)
		return 1;
	else
		return 0;
}
int mms_fw_i2c_busrt_write(u8 *value, u16 length)
{
	if (mms_i2c_busrt_write(ts->client, length, value) == 0)
		return 1;
	else
		return 0;
}
#if 0
static ssize_t set_tsp_firm_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf (buf, sizeof(buf), "%#02x, %#02x, %#02x\n", ts->version->core, ts->version->private, ts->version->public);
}
static ssize_t set_tsp_firm_version_read_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 fw_latest_version, privatecustom_version, publiccustom_version;
	int ret;
	uint8_t buff[4] = {0,};
	buff[0] = MIP_TSP_REVISION;
	ret = i2c_master_send(ts->client, buff, 1);
	if (ret < 0) {
		printk(KERN_ERR "%s : i2c_master_send [%d]\n", __func__, ret);
	}
	ret = i2c_master_recv(ts->client, buff, 7);
	if (ret < 0) {
		printk(KERN_ERR "%s : i2c_master_recv [%d]\n", __func__, ret);
	}
	fw_latest_version		= buff[3];
	privatecustom_version	= buff[4];
	publiccustom_version	= buff[5];
	return snprintf (buf, sizeof(buf), "%#02x, %#02x, %#02x\n", fw_latest_version, privatecustom_version, publiccustom_version);
}
static ssize_t set_tsp_threshold_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 threshold;
	mms_i2c_read(ts->client, P5_THRESHOLD, 1, &threshold);
	return snprintf (buf, sizeof(buf), "%d\n", threshold);
}
#endif
ssize_t set_tsp_for_inputmethod_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk(KERN_ERR "[TSP] %s is called.. is_inputmethod=%d\n", __func__, is_inputmethod);
	if (is_inputmethod)
		*buf = '1';
	else
		*buf = '0';
	return 0;
}
ssize_t set_tsp_for_inputmethod_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
//	unsigned int register_address = 0;
	if (tsp_enabled == false) {
		printk(KERN_ERR "[TSP ]%s. tsp_enabled is 0\n", __func__);
		return 1;
	}
	if (*buf == '1' && (!is_inputmethod)) {
		is_inputmethod = 1;
		printk(KERN_ERR "[TSP] Set TSP inputmethod IN\n");
		/* to do */
	} else if (*buf == '0' && (is_inputmethod)) {
		is_inputmethod = 0;
		printk(KERN_ERR "[TSP] Set TSP inputmethod OUT\n");
		/* to do */
	}
	return 1;
}
static ssize_t tsp_call_release_touch(struct device *dev, struct device_attribute *attr, char *buf)
{

	printk(KERN_ERR " %s is called\n", __func__);
	TSP_reboot();
	return snprintf (buf, sizeof(buf), "0\n");
}
#if 0 
static ssize_t tsp_touchtype_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char temp[15];
	snprintf (temp, sizeof(temp), "TSP : MMS134S\n");
	return 1;
}
#endif
#ifdef TSP_BOOST
ssize_t set_tsp_for_boost_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk(KERN_ERR "[TSP] %s is called.. is_inputmethod=%d\n", __func__, is_boost);
	if (is_boost)
		*buf = '1';
	else
		*buf = '0';

	return 0;
}
ssize_t set_tsp_for_boost_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	u16 obj_address = 0;
	u16 size_one;
	int ret;
	u8 value;
	int jump_limit = 0;
	int mrgthr = 0;
	u8 val = 0;
	unsigned int register_address = 0;
	if (tsp_enabled == false) {
		printk(KERN_ERR "[TSP ]%s. tsp_enabled is 0\n", __func__);
		return 1;
	}
	if (*buf == '1' && (!is_boost)) {
		is_boost = 1;
	} else if (*buf == '0' && (is_boost)) {
		is_boost = 0;
	}
	printk(KERN_ERR "[TSP] set_tsp_for_boost_store() called. %s!\n", is_boost ? "On" : "Off");
	TSP_boost(ts, is_boost);
	return 1;
}
#endif
#if 0
static DEVICE_ATTR(tsp_threshold, S_IRUGO | S_IWUSR | S_IWGRP, set_tsp_threshold_mode_show, NULL);
static DEVICE_ATTR(set_tsp_for_inputmethod, S_IRUGO | S_IWUSR | S_IWGRP, set_tsp_for_inputmethod_show, set_tsp_for_inputmethod_store); /* For 3x4 Input Method, Jump limit changed API */
static DEVICE_ATTR(call_release_touch, S_IRUGO | S_IWUSR | S_IWGRP, tsp_call_release_touch, NULL);
static DEVICE_ATTR(mxt_touchtype, S_IRUGO | S_IWUSR | S_IWGRP,	tsp_touchtype_show, NULL);
#ifdef TSP_BOOST
static DEVICE_ATTR(set_tsp_boost, S_IRUGO | S_IWUSR | S_IWGRP, set_tsp_for_boost_show, set_tsp_for_boost_store); /* Control wait_filter to boost response. */
#endif

static struct attribute *sec_touch_attributes[] = {
	&dev_attr_tsp_firm_version_phone.attr,
	&dev_attr_tsp_firm_version_panel.attr,
	&dev_attr_tsp_threshold.attr,
	&dev_attr_set_tsp_for_inputmethod.attr,
	&dev_attr_call_release_touch.attr,
	&dev_attr_mxt_touchtype.attr,
#ifdef TSP_BOOST
	&dev_attr_set_tsp_boost.attr,
#endif
	NULL,
};
#endif
#if 0
static struct attribute_group sec_touch_attr_group = {
	.attrs = sec_touch_attributes,
};
#endif
#endif
#ifdef TSP_FACTORY_TEST
//static bool debug_print = true;
//static u16 inspection_data[180] = { 0, };
static u16 lntensity_data[180] = { 0, };
static u16 CmDelta_data[228] = { 0, }; /* inspection */
static u16 CmABS_data[228] = { 0, }; /* reference */
static ssize_t set_tsp_module_on_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;
	ret = mms_ts_resume(ts->client);
	if (ret  == 0)
		*buf = '1';
	else
		*buf = '0';
	msleep(500);
	return 0;
}
static ssize_t set_tsp_module_off_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;
	ret = mms_ts_suspend(ts->client, PMSG_SUSPEND);
	if (ret  == 0)
		*buf = '1';
	else
		*buf = '0';
	return 0;
}
/* CM ABS */
static int check_debug_data(struct mms_ts_data *ts)
{
	u8 setLowLevelData[4];
	u8 read_data_buf[50] = {0,};
//	u16 read_data_buf1[50] = {0,};
	int sensing_ch, exciting_ch;
	int ret, i, j, status=0;
	int size;
	tsp_testmode = 1;
	printk(KERN_ERR "[TSP] %s entered. line : %d\n", __func__, __LINE__);
	printk(KERN_ERR "[TSP] %s disable IRQ( %d)\n", __func__, __LINE__);
	disable_irq(ts->client->irq);
	exciting_ch = g_exciting_ch;
	sensing_ch = g_sensing_ch;
	/* Read Reference Data */
	setLowLevelData[0] = 0xA0; /* UNIVERSAL_CMD */
	setLowLevelData[1] = 0x40; /* UNIVCMD_ENTER_TEST_MODE */
	ret = mms_i2c_write(ts->client, setLowLevelData, 2);
	while (gpio_get_value(GPIO_I2C_INT))
		;

	ret = mms_i2c_read(ts->client, 0xAE, 1, read_data_buf);
	printk(KERN_ERR "\n\n --- CM_ABS --- ");
	/* Read Reference Data */
	setLowLevelData[0] = 0xA0;
	setLowLevelData[1] = 0x43; /* UNIVCMD_TEST_CM_ABS */
	ret = mms_i2c_write(ts->client, setLowLevelData, 2);
	while (gpio_get_value(GPIO_I2C_INT))
		;

	ret = mms_i2c_read(ts->client, 0xAE, 1, read_data_buf);
	printk(KERN_ERR "[TSP] %s ret= %d\n", __func__, ret);
	for (i = 0; i < sensing_ch; i++) {
		for (j = 0; j < exciting_ch; j++) {
			setLowLevelData[0] = 0xA0;
			setLowLevelData[1] = 0x44;
			setLowLevelData[2] = j;
			setLowLevelData[3] = i;
			ret = mms_i2c_write(ts->client, setLowLevelData, 4);
			while (gpio_get_value(GPIO_I2C_INT))
				;

			ret = mms_i2c_read(ts->client, 0xAE,
				1, read_data_buf);
			size = read_data_buf[0];
			ret = mms_i2c_read(ts->client, 0xAF,
				size, read_data_buf);
			CmABS_data[(i * exciting_ch) + j]
				= (read_data_buf[0] |  read_data_buf[1] << 8);
			if ((CmABS_data[(i * exciting_ch) + j]
					>= SCR_ABS_LOWER_SPEC[i][j])
				&& (CmABS_data[(i * exciting_ch) + j]
					<= SCR_ABS_UPPER_SPEC[i][j]))
				status = 1; /* fail */
			else
				status = 0; /* pass */
		}
	}
	printk(KERN_ERR "[TSP] CmABS_data\n");
	for (i = 0; i < exciting_ch * sensing_ch; i++) {
		if (0 == i % exciting_ch)
			printk(KERN_INFO "\n");
		printk(KERN_ERR "%4d, ", CmABS_data[i]);
	}
	printk(KERN_INFO "\n");
	/* Read Reference Data */
	setLowLevelData[0] = 0xA0;
	setLowLevelData[1] = 0x4F;
	ret = mms_i2c_write(ts->client, setLowLevelData, 2);

	printk(KERN_ERR "[TSP] %s enable IRQ( %d)\n", __func__, __LINE__);
	enable_irq(ts->client->irq);
	tsp_testmode = 0;
	TSP_reboot();
	printk(KERN_ERR "%s : end\n", __func__);
	return status;
}
/* inspection = CmDelta_data */
static int check_delta_data(struct mms_ts_data *ts)
{
	u8 setLowLevelData[4];
	u8 read_data_buf[50] = {0,};
//	u16 read_data_buf1[50] = {0,};
	int sensing_ch, exciting_ch;
	int ret, i, j, status=0;
	int gpio;
	int size;
	printk(KERN_ERR "[TSP] %s entered. line : %d,\n", __func__, __LINE__);

	printk(KERN_ERR "[TSP] %s disable IRQ( %d)\n", __func__, __LINE__);
	gpio = irq_to_gpio(ts->client->irq);
	disable_irq(ts->client->irq);
	exciting_ch = g_exciting_ch;
	sensing_ch	 = g_sensing_ch;
	/* Read Reference Data */
	setLowLevelData[0] = 0xA0; /* UNIVERSAL_CMD */
	setLowLevelData[1] = 0x40; /* UNIVCMD_ENTER_TEST_MODE */
	ret = mms_i2c_write(ts->client, setLowLevelData, 2);
	while (gpio_get_value(gpio))
		;

	ret = mms_i2c_read(ts->client, 0xAE, 1, read_data_buf);
	printk(KERN_ERR "\n\n --- CM_DELTA --- ");
	/* Read Reference Data */
	setLowLevelData[0] = 0xA0;
	setLowLevelData[1] = 0x41;
	ret = mms_i2c_write(ts->client, setLowLevelData, 2);
	while (gpio_get_value(gpio))
		;

	ret = mms_i2c_read(ts->client, 0xAE, 1, read_data_buf);
	for (i = 0; i < sensing_ch; i++) {
		for (j = 0; j < exciting_ch; j++) {
			setLowLevelData[0] = 0xA0;
			setLowLevelData[1] = 0x42;
			setLowLevelData[2] = j; /* Exciting CH. */
			setLowLevelData[3] = i; /* Sensing CH. */
			ret = mms_i2c_write(ts->client, setLowLevelData, 4);
			while (gpio_get_value(gpio))
				;

			ret = mms_i2c_read(ts->client, 0xAE,
				1, read_data_buf);
			size = read_data_buf[0];
			ret = mms_i2c_read(ts->client, 0xAF,
				read_data_buf[0], read_data_buf);
			CmDelta_data[(i * exciting_ch) + j]
				= (read_data_buf[0] |  read_data_buf[1] << 8);
		}
	}
	printk(KERN_ERR "[TSP] CmDelta_data\n");
	for (i = 0; i < exciting_ch * sensing_ch; i++) {
		if (0 == i % exciting_ch)
			printk(KERN_INFO "\n");
		printk(KERN_ERR "%4d, ", CmDelta_data[i]);
	}
	printk(KERN_INFO "\n");
	/* Read Reference Data */
	setLowLevelData[0] = 0xA0;
	setLowLevelData[1] = 0x4F;
	ret = mms_i2c_write(ts->client, setLowLevelData, 2);
	printk(KERN_ERR "[TSP] %s enable IRQ( %d)\n", __func__, __LINE__);
	enable_irq(ts->client->irq);
	tsp_testmode = 0;
	TSP_reboot();
	printk(KERN_ERR "%s : end\n", __func__);
	return status;
}
static int atoi(const char *str)
{
	int result = 0;
	int count = 0;
	if (str == NULL)
		return result;
	while ((str[count] != '\0') && (str[count] >= '0') && (str[count] <= '9')) {
		result = result * 10 + str[count] - '0';
		++count;
	}
	return result;
}
ssize_t disp_all_refdata_show(struct device *dev, \
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 5, "%u\n", CmABS_data[index]);
}
ssize_t disp_all_refdata_store(struct device *dev, \
	struct device_attribute *attr, const char *buf, size_t size)
{
	index = atoi(buf);
	printk(KERN_ERR "%s : value %d\n", __func__, index);
	return size;
}
static ssize_t set_all_delta_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 status = 0;
/* Delta */
	check_delta_data(ts);
	set_tsp_module_off_show(dev, attr, buf);
	set_tsp_module_on_show(dev, attr, buf);
	return snprintf (buf, sizeof(buf), "%u\n", status);
}
static ssize_t set_all_refer_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 status = 0;
/* ABS */	
	status = check_debug_data(ts);
	set_tsp_module_off_show(dev, attr, buf);
	set_tsp_module_on_show(dev, attr, buf);
	return snprintf (buf, sizeof(buf), "%u\n", status);
}
ssize_t disp_all_deltadata_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk(KERN_ERR "disp_all_deltadata_show : value %d\n", CmDelta_data[index]);
	return snprintf (buf, sizeof(buf), "%u\n",  CmDelta_data[index]);
}
ssize_t disp_all_deltadata_store(struct device *dev, struct device_attribute *attr,
								   const char *buf, size_t size)
{
	index = atoi(buf);
	printk(KERN_ERR "Delta data %d", index);
	return size;
}
static void check_intensity_data(struct mms_ts_data *ts)
{
	int16_t menu = 0;
	int16_t back = 0;
	u8 setLowLevelData[6];
	u8 read_data_buf[50] = {0,};
	int sensing_ch, exciting_ch;
	int ret, i, j;
	printk(KERN_ERR "[TSP] %s entered. line : %d\n", __func__, __LINE__);
	menu = gMenuKey_Intensity;
	back = gBackKey_Intensity;
	exciting_ch = g_exciting_ch;
	sensing_ch = g_sensing_ch;
	printk(KERN_ERR "[TSP] %s disable IRQ( %d)\n", __func__, __LINE__);
	disable_irq(ts->client->irq);
	for (i = 0; i < sensing_ch; i++) {
		for (j = 0 ; j < exciting_ch; j++) {
			setLowLevelData[0] = 0xB0; /* VENDOR_SPECIFIC_CMD */
			setLowLevelData[1] = 0x1A; /* VENDOR_CMD_SS_TSP_S */
			setLowLevelData[2] = j;
			setLowLevelData[3] = i;
			setLowLevelData[4] = 0x00; /* Reserved */
			setLowLevelData[5] = 0x04;
			ret = mms_i2c_write(ts->client, setLowLevelData, 6);
			ret = mms_i2c_read(ts->client, 0xBF,
				1, read_data_buf);
			ts->intensity[(i * exciting_ch) + j] 
				= read_data_buf[0];
			lntensity_data[(i * exciting_ch) + j]
				= read_data_buf[0];
		}
/*
		if (i == 0)
				lntensity_data[(i * exciting_ch) + j-1] = menu;
		else if (i == 1)
				lntensity_data[(i * exciting_ch) + j-1] = back;
		*/
	}
	setLowLevelData[0] = 0xB0; /* VENDOR_SPECIFIC_CMD */
	setLowLevelData[1] = 0x1A; /* VENDOR_CMD_SS_TSP_S */
	setLowLevelData[2] = 0x00;
	setLowLevelData[3] = 0x00;
	setLowLevelData[4] = 0x00; /* Reserved */
	setLowLevelData[5] = 0x05; /* VENDOR_CMD_SS_TSP_S_EXIT_MODE */
	ret = mms_i2c_write(ts->client, setLowLevelData, 6);
#if 1
	printk(KERN_ERR "[TSP] lntensity_data\n");
	for (i = 0; i < exciting_ch * sensing_ch; i++) {
		if (0 == i % exciting_ch)
			printk(KERN_INFO"\n");
		printk(KERN_ERR"%4d, ", lntensity_data[i]);
	}
	printk(KERN_INFO"\n");
#endif

	printk(KERN_ERR "[TSP] %s enable IRQ( %d)\n", __func__, __LINE__);
	enable_irq(ts->client->irq);
	msleep(20);
//	check_delta_data(ts);
	printk(KERN_ERR "%s : end\n", __func__);
}
/*
static ssize_t set_refer0_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 refrence = 0;
	check_intensity_data(ts);
	refrence = inspection_data[28];
	return snprintf (buf, sizeof(buf), "%u\n", refrence);
}
static ssize_t set_refer1_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 refrence = 0;
	refrence = inspection_data[288];
	return snprintf (buf, sizeof(buf), "%u\n", refrence);
}
static ssize_t set_refer2_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 refrence = 0;
	refrence = inspection_data[194];
	return snprintf (buf, sizeof(buf), "%u\n", refrence);
}
static ssize_t set_refer3_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 refrence = 0;
	refrence = inspection_data[49];
	return snprintf (buf, sizeof(buf), "%u\n", refrence);
}
static ssize_t set_refer4_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 refrence = 0;
	refrence = inspection_data[309];
	return snprintf (buf, sizeof(buf), "%u\n", refrence);
}
static ssize_t set_intensity0_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 intensity = 0;
	intensity = lntensity_data[28];
	return snprintf (buf, sizeof(buf), "%u\n", intensity);
}
static ssize_t set_intensity1_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 intensity = 0;
	intensity = lntensity_data[288];
	return snprintf (buf, sizeof(buf), "%u\n", intensity);
}
static ssize_t set_intensity2_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 intensity = 0;
	intensity = lntensity_data[194];
	return snprintf (buf, sizeof(buf), "%u\n", intensity);
}
static ssize_t set_intensity3_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 intensity = 0;
	intensity = lntensity_data[49];
	return snprintf (buf, sizeof(buf), "%u\n", intensity);
}
static ssize_t set_intensity4_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 intensity = 0;
	intensity = lntensity_data[309];
	return snprintf (buf, sizeof(buf), "%u\n", intensity);
}
*/
/*
static DEVICE_ATTR(set_refer0, S_IRUGO | S_IWUSR | S_IWGRP, set_refer0_mode_show, NULL);
static DEVICE_ATTR(set_delta0, S_IRUGO | S_IWUSR | S_IWGRP, set_intensity0_mode_show, NULL);
static DEVICE_ATTR(set_refer1, S_IRUGO | S_IWUSR | S_IWGRP, set_refer1_mode_show, NULL);
static DEVICE_ATTR(set_delta1, S_IRUGO | S_IWUSR | S_IWGRP, set_intensity1_mode_show, NULL);
static DEVICE_ATTR(set_refer2, S_IRUGO | S_IWUSR | S_IWGRP, set_refer2_mode_show, NULL);
static DEVICE_ATTR(set_delta2, S_IRUGO | S_IWUSR | S_IWGRP, set_intensity2_mode_show, NULL);
static DEVICE_ATTR(set_refer3, S_IRUGO | S_IWUSR | S_IWGRP, set_refer3_mode_show, NULL);
static DEVICE_ATTR(set_delta3, S_IRUGO | S_IWUSR | S_IWGRP, set_intensity3_mode_show, NULL);
static DEVICE_ATTR(set_refer4, S_IRUGO | S_IWUSR | S_IWGRP, set_refer4_mode_show, NULL);
static DEVICE_ATTR(set_delta4, S_IRUGO | S_IWUSR | S_IWGRP, set_intensity4_mode_show, NULL);
static DEVICE_ATTR(set_threshould, S_IRUGO | S_IWUSR | S_IWGRP,
	set_tsp_threshold_mode_show, NULL);
static struct attribute *sec_touch_facotry_attributes[] = {
	&dev_attr_set_refer0.attr,
	&dev_attr_set_delta0.attr,
	&dev_attr_set_refer1.attr,
	&dev_attr_set_delta1.attr,
	&dev_attr_set_refer2.attr,
	&dev_attr_set_delta2.attr,
	&dev_attr_set_refer3.attr,
	&dev_attr_set_delta3.attr,
	&dev_attr_set_refer4.attr,
	&dev_attr_set_delta4.attr,
	&dev_attr_set_threshould.attr,
	NULL,
};
*/
#endif
static void release_all_fingers(struct mms_ts_data *ts)
{
	int i;
	printk(KERN_ERR "%s start.\n", __func__);
	for (i = 0; i < MELFAS_MAX_TOUCH; i++) {
		if (-1 == g_Mtouch_info[i].strength) {
			g_Mtouch_info[i].posX = 0;
			g_Mtouch_info[i].posY = 0;
			continue;
		}
		printk(KERN_ERR "%s %s(%d)\n", __func__,
				ts->input_dev->name, i);
		g_Mtouch_info[i].strength = 0;
		input_mt_slot(ts->input_dev, i);
		input_mt_report_slot_state(ts->input_dev,
							MT_TOOL_FINGER, false);
		g_Mtouch_info[i].posX = 0;
		g_Mtouch_info[i].posY = 0;
		if (0 == g_Mtouch_info[i].strength)
			g_Mtouch_info[i].strength = -1;
		}
	input_sync(ts->input_dev);
	#if TOUCH_BOOSTER
	set_dvfs_lock(ts, 2);
	printk(KERN_INFO "[TSP] dvfs_lock free.\n ");
#endif
}
static void TSP_reboot(void)
{
#if 1
	printk(KERN_ERR "%s start!\n", __func__);
	printk(KERN_ERR "[TSP] %s disable IRQ( %d)\n", __func__, __LINE__);
	disable_irq_nosync(ts->client->irq);
	tsp_enabled = false;

	ts_power_enable(0);
	msleep(60);

	release_all_fingers(ts);

	msleep(60);
	ts_power_enable(1);
	msleep(60);

	printk(KERN_ERR "[TSP] %s enable IRQ( %d)\n", __func__, __LINE__);
	enable_irq(ts->client->irq);
	tsp_enabled = true;

#else
	if (tsp_enabled == false)
		return;
	printk(KERN_ERR "%s satrt!\n", __func__);
	disable_irq_nosync(ts->client->irq);
	tsp_enabled = false;
	touch_is_pressed = 0;
	release_all_fingers(ts);
	ts->gpio();
	ts->power(false);
	msleep(200);
	ts->power(true);
#ifdef TSP_BOOST
	TSP_boost(ts, is_boost);
#endif
	printk(KERN_ERR "[TSP] %s enable IRQ( %d)\n", __func__, __LINE__);
	enable_irq(ts->client->irq);

	tsp_enabled = true;
#endif
};
void TSP_force_released(void)
{
	printk(KERN_ERR "%s satrt!\n", __func__);
	if (tsp_enabled == false) {
		printk(KERN_ERR "[TSP] Disabled\n");
		return;
	}
	release_all_fingers(ts);
	touch_is_pressed = 0;
};
EXPORT_SYMBOL(TSP_force_released);
void TSP_ESD_seq(void)
{
	TSP_reboot();
	printk(KERN_ERR "%s satrt!\n", __func__);
};
EXPORT_SYMBOL(tsp_call_release_touch);
#ifdef SET_TSP_CONFIG
static int mms_set_config(struct i2c_client *client, u8 reg, u8 value)
{
	u8 buffer[2];
	int ret;
	struct mms_ts_data *ts = i2c_get_clientdata(client);
	buffer[0] = reg;
	buffer[1] = value;
	ret = mms_i2c_write(ts->client, (char *)buffer, 2);
	return ret;
}
#endif
int tsp_i2c_read_mms(u8 reg, unsigned char *rbuf, int buf_size)
{
	int i, ret = -1;
	struct i2c_msg rmsg;
	uint8_t start_reg;
	int retry = 3;
	for (i = 0; i < retry; i++) {
		rmsg.addr = ts->client->addr;
		rmsg.flags = 0;
		rmsg.len = 1;
		rmsg.buf = &start_reg;
		start_reg = reg;
		ret = i2c_transfer(ts->client->adapter, &rmsg, 1);
		if (ret >= 0) {
			rmsg.flags = I2C_M_RD;
			rmsg.len = buf_size;
			rmsg.buf = rbuf;
			ret = i2c_transfer(ts->client->adapter, &rmsg, 1);
			if (ret >= 0)
				break;
		}
		if (i == (retry - 1)) {
			printk(KERN_ERR "[TSP] Error code : %d, %d\n", __LINE__, ret);
		}
	}
	return ret;
}
#if 0
static int mms_i2c_read(struct i2c_client *p_client, u8 reg, u8 *data, int len)
{
	struct i2c_msg msg;
	/* set start register for burst read */
	/* send separate i2c msg to give STOP signal after writing. */
	/* Continous start is not allowed for cypress touch sensor. */
	msg.addr = p_client->addr;
	msg.flags = 0;
	msg.len = 1;
	msg.buf = &reg;
	if (1 != i2c_transfer(p_client->adapter, &msg, 1)) {
		printk(KERN_ERR "[TSP][MMS128][%s] set data pointer fail! reg(%x)\n", __func__, reg);
		return -EIO;
	}
	/* begin to read from the starting address */
	msg.addr = p_client->addr;
	msg.flags = I2C_M_RD;
	msg.len = len;
	msg.buf = data;
	if (1 != i2c_transfer(p_client->adapter, &msg, 1)) {
		printk(KERN_ERR "[TSP][MMS128][%s] fail! reg(%x)\n", __func__, reg);
		return -EIO;
	}
	return 0;
}
#endif
static ssize_t firmware_panel_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
//	extern unsigned char TSP_PanelVersion;
	int panel;
	//unsigned char read_buffer;
	//mms_fw_i2c_read(0x1D, &read_buffer, 1);
	//TSP_PanelVersion = read_buffer;
//	check_firmware_version_func();
	panel = TSP_PanelVersion;
	printk(KERN_ERR "firmware_panel_show : [%d]\n", panel);
	return snprintf(buf, sizeof(buf), "%x\n", panel);
}
static ssize_t firmware_phone_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
//	extern unsigned char TSP_PhoneVersion;
	int NEW_FIRMWARE_VERSION = 0;
//	check_firmware_version_func();
	NEW_FIRMWARE_VERSION = TSP_PhoneVersion;
	return snprintf(buf, sizeof(buf), "%x\n", NEW_FIRMWARE_VERSION);
}
static ssize_t threshold_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int threshold;
	
	if (IC_type == 0x0F)
		threshold = 15;
	else
		threshold = 30;
	
	return snprintf(buf, sizeof(buf), "%d\n", threshold);
}
static ssize_t firmware_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 buf1[6] = {0,};
	int hw_rev, fw_ver, phone_ver;
	if (0 == mms_i2c_read(ts->client, MIP_TSP_REVISION, 6, buf1))
	{
		hw_rev = buf1[1];
		fw_ver = buf1[5];
		phone_ver = FW_VERSION;
		snprintf(buf, sizeof(buf), "%03X%02X%02X\n",
			hw_rev, fw_ver, phone_ver);
		printk(KERN_ERR "[TSP][MMS134s][%s]  phone_ver=%d, fw_ver=%d, hw_rev=%d\n",
			buf, phone_ver, fw_ver, hw_rev);
	} else {
		printk(KERN_ERR "[TSP][MMS134s][%s] Can't find HW Ver, FW ver!\n",
				__func__);
	}
	return snprintf(buf, sizeof(buf), "%s", buf);
}
static ssize_t firmware_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	int ret;
	printk(KERN_INFO "START firmware store\n");
	ts_power_enable(0);
	msleep(500);
	ts_power_enable(1);
	msleep(500);

	printk(KERN_ERR "[TSP] %s disable IRQ( %d)\n", __func__, __LINE__);
	disable_irq(ts->client->irq);
	local_irq_disable();
	ret = MFS_ISC_update();
	local_irq_enable();
	printk(KERN_ERR "[TSP] %s enable IRQ( %d)\n", __func__, __LINE__);
	enable_irq(ts->client->irq);
#if 0
	hrtimer_start(&ts->timer, ktime_set(0, 200000000), HRTIMER_MODE_REL);
#if defined (__TOUCH_TA_CHECK__)
	b_Firmware_store = false;
#endif
	if (ret == MCSDL_RET_SUCCESS)
		firmware_ret_val = 1;
	else
		firmware_ret_val = 0;
#endif
	printk(KERN_INFO"[TSP] Firmware update end!!\n");
	ts_power_enable(0);
	msleep(500);
	ts_power_enable(1);
	printk(KERN_INFO "firmware store END\n");
	return 0;
}
static ssize_t tsp_threshold_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	u8 threshold;
	
	if (IC_type == 0x0F)
		threshold = 15;
	else
		threshold = 30;
	
	return snprintf(buf, sizeof(buf), "%d\n", threshold);
}
static ssize_t tsp_firm_update_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	u8 firm_update_status = 0;
	return snprintf(buf, sizeof(buf), "%d\n", firm_update_status);
}
static ssize_t touchkey_back_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", gBackKey_Intensity);
}
static ssize_t touchkey_menu_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", gMenuKey_Intensity);
}
#if 0
static ssize_t	check_init_lowleveldata()
{
	u8 read_buf[1] = {0,};
	int ret = 1;
		ret = mms_i2c_read(ts->client, 0x2e, 1, read_buf);
		if (ret < 0) {
			printk(KERN_ERR "[TSP]Exciting CH. mms_i2c_read fail! %s : %d, \n",
				__func__, __LINE__);
			return 0;
		}
		g_exciting_ch = read_buf[0];
		ret = mms_i2c_read(ts->client, 0x2f, 1, read_buf);
		if (ret < 0) {
			printk(KERN_ERR "[TSP]Sensing CH. mms_i2c_read fail! %s : %d, \n",
				__func__, __LINE__);
			return 0;
		}
		g_sensing_ch = read_buf[0];

	return ret;
}
#endif
//static int start_rawcounter = 1;
static ssize_t tkey_rawcounter_store(struct device *dev, \
struct device_attribute *attr, const char *buf, size_t size)
{
	char *after;
	unsigned long value = simple_strtoul(buf, &after, 10);
	printk(KERN_INFO "[TSP] %s, %d, value=%ld\n", __func__, __LINE__, value);
	return size;
}
static ssize_t tkey_rawcounter_show(struct device *dev, \
			struct device_attribute *attr, char *buf)
{
	printk(KERN_ERR "[TSP] menukey : %d, backKey : %d\n", \
	gMenuKey_Intensity, gBackKey_Intensity);
	mdelay(1);
	return snprintf(buf, 10, "%d %d\n", \
	gMenuKey_Intensity, gBackKey_Intensity);
}
static ssize_t set_module_off_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	TSP_reboot();
	msleep(300);
	printk(KERN_INFO "set_tsp_test_mode_disable0 \n");
	tsp_testmode = 0;
	return 0;
}
static ssize_t set_module_on_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	tsp_testmode = 1;
	printk(KERN_INFO "set_tsp_test_mode_enable0 \n");
	mdelay(50);
	ts_power_enable(0);
	mdelay(500);
	ts_power_enable(1);
	return 0;
}
static ssize_t touchtype_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 15, "MMS134\n");
}
static ssize_t set_all_intensity_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int status = 0;
	check_intensity_data(ts);
	set_tsp_module_off_show(dev, attr, buf);
	set_tsp_module_on_show(dev, attr, buf);
	return snprintf (buf, sizeof(buf), "%u\n", status);
}
ssize_t disp_all_intendata_store(struct device *dev, struct device_attribute *attr,
								   const char *buf, size_t size)
{
	index = atoi(buf);
	printk(KERN_ERR "Intensity data %d", index);
	return size;
}
ssize_t disp_all_intendata_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	printk(KERN_ERR "disp_all_intendata_show : value %d, index=%d\n",
		   lntensity_data[index], index);
	return snprintf (buf, sizeof(buf), "%u\n",  lntensity_data[index]);
}
static ssize_t rawdata_pass_fail_melfas(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 setLowLevelData[2] = {0x09, 0x01,};
	u8 read_data_buf[50] = {0,};
	u16 read_data_buf1[50] = {0,};
	int read_data_len, sensing_ch;
	int ret, i, j;
	tsp_testmode = 1;

	printk(KERN_ERR "[TSP] %s disable IRQ( %d)\n", __func__, __LINE__);
	disable_irq(ts->client->irq);
	read_data_len = g_exciting_ch * 2;
	sensing_ch	 = g_sensing_ch;
	for (i = 0; i < sensing_ch; i++) {
		ret = mms_i2c_write(ts->client, setLowLevelData, 2);
		while (gpio_get_value(GPIO_I2C_INT)) {
			udelay(50);
		}
		udelay(300);
		ret = mms_i2c_read(ts->client, 0xb2, read_data_len, read_data_buf);
		if (ret < 0)
			printk(KERN_ERR "can't read rawdata_pass_fail_tst200 Data %dth\n", i);
		udelay(5);
		for (j = 0 ; j < read_data_len / 2; j++) {
			read_data_buf1[j] = (read_data_buf[j*2] << 8) + read_data_buf[j*2+1];
			if ((SCR_ABS_UPPER_SPEC[i][j] < read_data_buf1[j])
				|| (SCR_ABS_LOWER_SPEC[i][j] > read_data_buf1[j])) {
				printk(KERN_ERR "\n SCR_ABS_UPPER_SPEC[i][j] = %d",
						SCR_ABS_UPPER_SPEC[i][j]);
				printk(KERN_ERR "\n SCR_ABS_LOWER_SPEC[i][j] = %d",
						SCR_ABS_LOWER_SPEC[i][j]);
				printk(KERN_ERR "\n i=%d, j=%d, read_data_buf1[j]=%d",
						i, j, read_data_buf1[j]);
				printk(KERN_ERR "[TSP] %s enable IRQ( %d)\n", __func__, __LINE__);
				enable_irq(ts->client->irq);
				udelay(10);
				TSP_reboot();
				return snprintf(buf, sizeof(buf), "0");
			}
		}
		printk(KERN_INFO "\n");
#if 1
		printk(KERN_ERR "[%d]:", i);
		for (j = 0; j < read_data_len; j++) {
			printk(KERN_ERR "[%03d],", read_data_buf[j]);
		}
		printk(KERN_INFO "\n");
#endif
		msleep(1);
	}

	printk(KERN_ERR "[TSP] %s enable IRQ( %d)\n", __func__, __LINE__);
	enable_irq(ts->client->irq);
	tsp_testmode = 0;
	TSP_reboot();
    return snprintf (buf, sizeof(buf), "1");
}
static ssize_t touch_sensitivity_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, sizeof(int), "%x\n", 0);
}
#if 0 
static ssize_t touchkey_firm_store(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret, i;
	for (i = 0; i < DOWNLOAD_RETRY_CNT; i++) {
		ret = MFS_ISC_update();
		printk(KERN_ERR "mcsdl_download_binary_data : [%d]\n", ret);
		if (ret != 0)
			printk(KERN_ERR "SET Download Fail - error code [%d]\n",
				ret);
		else
			break;
	}
	return printk(KERN_INFO "\n[Melfas]TSP firmware update by kyestring");
}
#endif
static DEVICE_ATTR(tsp_firm_version_phone,
			S_IRUGO | S_IWUSR | S_IWGRP, firmware_phone_show, NULL);
static DEVICE_ATTR(tsp_firm_version_panel, S_IRUGO | S_IWUSR | S_IWGRP,
			firmware_panel_show, NULL);
static DEVICE_ATTR(tsp_firm_update, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
			firmware_store);
static DEVICE_ATTR(tsp_threshold, S_IRUGO | S_IWUSR | S_IWGRP,
			tsp_threshold_show, NULL);
static DEVICE_ATTR(tsp_firm_update_status, S_IRUGO | S_IWUSR | S_IWGRP,
			tsp_firm_update_status_show, NULL);
static DEVICE_ATTR(set_all_reference, S_IRUGO | S_IWUSR | S_IWGRP,
			set_all_refer_mode_show, NULL);
static DEVICE_ATTR(disp_all_refdata, S_IRUGO | S_IWUSR | S_IWGRP,
			disp_all_refdata_show, disp_all_refdata_store);
static DEVICE_ATTR(set_all_inspection, S_IRUGO | S_IWUSR | S_IWGRP,
			set_all_delta_mode_show, NULL);
static DEVICE_ATTR(disp_all_insdata, S_IRUGO | S_IWUSR | S_IWGRP,
			disp_all_deltadata_show, disp_all_refdata_store);
static DEVICE_ATTR(set_all_intensity, S_IRUGO | S_IWUSR | S_IWGRP,
			set_all_intensity_mode_show, NULL);
static DEVICE_ATTR(disp_all_intdata, S_IRUGO | S_IWUSR | S_IWGRP,
			disp_all_intendata_show, disp_all_intendata_store);
static DEVICE_ATTR(touchtype, S_IRUGO | S_IWUSR | S_IWGRP,
			touchtype_show, NULL);
static DEVICE_ATTR(set_module_off, S_IRUGO | S_IWUSR | S_IWGRP,
			set_module_off_show, NULL);
static DEVICE_ATTR(set_module_on, S_IRUGO | S_IWUSR | S_IWGRP,
			set_module_on_show, NULL);
static DEVICE_ATTR(firmware	, S_IRUGO | S_IWUSR | S_IWGRP,
			firmware_show, firmware_store);
static DEVICE_ATTR(raw_value, 0444, rawdata_pass_fail_melfas, NULL) ;
static DEVICE_ATTR(touchkey_back, S_IRUGO | S_IWUSR | S_IWGRP,
			touchkey_back_show, NULL);
static DEVICE_ATTR(touchkey_menu, S_IRUGO | S_IWUSR | S_IWGRP,
			touchkey_menu_show, NULL);
static DEVICE_ATTR(touchkey_raw_data, S_IRUGO | \
	S_IWUSR | S_IWGRP, tkey_rawcounter_show, tkey_rawcounter_store);
static DEVICE_ATTR(touch_sensitivity, S_IRUGO | S_IWUSR | S_IWGRP,
			touch_sensitivity_show, NULL);
static DEVICE_ATTR(touchkey_threshold, S_IRUGO | S_IWUSR | S_IWGRP,
			threshold_show, NULL);
static DEVICE_ATTR(touchkey_firm_version_phone,
			S_IRUGO | S_IWUSR | S_IWGRP, firmware_phone_show, NULL);
static DEVICE_ATTR(touchkey_firm_version_panel, S_IRUGO | S_IWUSR | S_IWGRP,
			firmware_panel_show, NULL);
#ifdef SEC_TSP_FACTORY_TEST
static void set_default_result(struct mms_ts_data *ts)
{
	char delim = ':';
	memset(ts->cmd_result, 0x00, ARRAY_SIZE(ts->cmd_result));
	memcpy(ts->cmd_result, ts->cmd, strlen(ts->cmd));
	strncat(ts->cmd_result, &delim, 1);
}
static void set_cmd_result(struct mms_ts_data *ts, char *buff, int len)
{
	strncat(ts->cmd_result, buff, len);
}
//static inline int msm_irq_to_gpio(unsigned irq)
//{
//	/* TODO : Need to verify chip->base=0 */
//	return irq - MSM_GPIO_TO_INT(0);
//}
static void get_raw_data_all(struct mms_ts_data *ts, u8 cmd)
{
	u8 w_buf[6];
	u8 read_buffer[2]; /* 52 */
	char buff[TSP_CMD_STR_LEN] = {0};
//	int gpio;
	int ret;
	int i, j;
	u32 max_value, min_value;
	u32 raw_data;
//	gpio = msm_irq_to_gpio(ts->irq);
	printk(KERN_ERR "[TSP] %s disable IRQ( %d)\n", __func__, __LINE__);
	disable_irq(ts->irq);

	w_buf[0] = MMS_VSC_CMD;		/* vendor specific command id */
	w_buf[1] = MMS_VSC_MODE;	/* mode of vendor */
	w_buf[2] = 0;			/* tx line */
	w_buf[3] = 0;			/* rx line */
	w_buf[4] = 0;			/* reserved */
	w_buf[5] = 0;			/* sub command */
	if (cmd == MMS_VSC_CMD_EXIT) {
		w_buf[5] = MMS_VSC_CMD_EXIT; /* exit test mode */
		ret = i2c_smbus_write_i2c_block_data(ts->client,
			w_buf[0], 5, &w_buf[1]);
		if (ret < 0)
			goto err_i2c;
		touch_is_pressed = 0;
		release_all_fingers(ts);
		msleep(50);
		ts_power_enable(0);
		msleep(500);
		ts_power_enable(1);
		msleep(300);
		printk(KERN_ERR "[TSP] %s enable IRQ( %d)\n"
			, __func__, __LINE__);
		enable_irq(ts->irq);
		return ;
	}
	/* MMS_VSC_CMD_CM_DELTA or MMS_VSC_CMD_CM_ABS
	 * this two mode need to enter the test mode
	 * exit command must be followed by testing.
	 */
	if (cmd == MMS_VSC_CMD_CM_DELTA || cmd == MMS_VSC_CMD_CM_ABS) {
		/* enter the debug mode */
		w_buf[2] = 0x0; /* tx */
		w_buf[3] = 0x0; /* rx */
		w_buf[5] = MMS_VSC_CMD_ENTER;
		ret = i2c_smbus_write_i2c_block_data(ts->client,
			w_buf[0], 5, &w_buf[1]);
		if (ret < 0)
			goto err_i2c;
		/* wating for the interrupt */
		while (gpio_get_value(GPIO_I2C_INT))
			udelay(100);
	}
	max_value = 0;
	min_value = 0;
	for (i = 0; i < RX_NUM; i++) {
		for (j = 0; j < TX_NUM; j++) {
			w_buf[2] = j; /* tx */
			w_buf[3] = i; /* rx */
			w_buf[5] = cmd;
			ret = i2c_smbus_write_i2c_block_data(ts->client,
					w_buf[0], 5, &w_buf[1]);
			if (ret < 0)
				goto err_i2c;
			usleep_range(1, 5);
			ret = i2c_smbus_read_i2c_block_data(ts->client, 0xBF,
					2, read_buffer);
			if (ret < 0)
				goto err_i2c;
			raw_data = ((u16)read_buffer[1] << 8) | read_buffer[0];

			if (i == 0 && j == 0) {
				max_value = min_value = raw_data;
			} else {
				max_value = max(max_value, raw_data);
				min_value = min(min_value, raw_data);
			}
			if (cmd == MMS_VSC_CMD_INTENSITY) {
				ts->intensity[j * RX_NUM + i] = raw_data;
				dev_dbg(&ts->client->dev, "[TSP] intensity[%d][%d] = %d\n",
					i, j, ts->intensity[j * RX_NUM + i]);
			} else if (cmd == MMS_VSC_CMD_CM_DELTA) {
				ts->inspection[j * RX_NUM + i] = raw_data;
				dev_dbg(&ts->client->dev, "[TSP] delta[%d][%d] = %d\n",
					i, j, ts->inspection[j * RX_NUM + i]);
			} else if (cmd == MMS_VSC_CMD_CM_ABS) {
				ts->raw[j * RX_NUM + i] = raw_data;
				dev_dbg(&ts->client->dev, "[TSP] raw[%d][%d] = %d\n",
					i, j, ts->raw[j * RX_NUM + i]);
			} else if (cmd == MMS_VSC_CMD_REFER) {
				ts->reference[j * RX_NUM + i] =
						raw_data >> 3;
				dev_dbg(&ts->client->dev, "[TSP] reference[%d][%d] = %d\n",
					i, j, ts->reference[j * RX_NUM + i]);
			}
		}
	}
	snprintf(buff, sizeof(buff), "%d,%d", min_value, max_value);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));

	printk(KERN_ERR "[TSP] %s enable IRQ( %d)\n", __func__, __LINE__);
	enable_irq(ts->irq);
err_i2c:
	dev_err(&ts->client->dev, "%s: fail to i2c (cmd=%d)\n",
			__func__, cmd);
}
#if defined(ESD_DEBUG) || defined(SEC_TKEY_FACTORY_TEST)
static u32 get_raw_data_one(struct mms_ts_data *ts, u16 rx_idx, u16 tx_idx,
		u8 cmd)
{
	u8 w_buf[6];
	u8 read_buffer[2];
	int ret;
	u32 raw_data;
	w_buf[0] = MMS_VSC_CMD;		/* vendor specific command id */
	w_buf[1] = MMS_VSC_MODE;	/* mode of vendor */
	w_buf[2] = 0;			/* tx line */
	w_buf[3] = 0;			/* rx line */
	w_buf[4] = 0;			/* reserved */
	w_buf[5] = 0;			/* sub command */
	if (cmd != MMS_VSC_CMD_INTENSITY && cmd != MMS_VSC_CMD_RAW &&
		cmd != MMS_VSC_CMD_REFER && cmd != VSC_INTENSITY_TK &&
		cmd != VSC_RAW_TK) {
		dev_err(&ts->client->dev, "%s: not profer command(cmd=%d)\n",
				__func__, cmd);
		return FAIL;
	}
	w_buf[2] = tx_idx;	/* tx */
	w_buf[3] = rx_idx;	/* rx */
	w_buf[5] = cmd;		/* sub command */
	ret = i2c_smbus_write_i2c_block_data(ts->client, w_buf[0], 5,
			&w_buf[1]);
	if (ret < 0)
		goto err_i2c;
	ret = i2c_smbus_read_i2c_block_data(ts->client, 0xBF, 2,
			read_buffer);
	if (ret < 0)
		goto err_i2c;
	raw_data = ((u16)read_buffer[1] << 8) | read_buffer[0];
	if (cmd == MMS_VSC_CMD_REFER)
		raw_data = raw_data >> 4;
	return raw_data;
err_i2c:
	dev_err(&ts->client->dev, "%s: fail to i2c (cmd=%d)\n",
			__func__, cmd);
	return FAIL;
}
#endif
static ssize_t show_close_tsp_test(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mms_ts_data *ts = dev_get_drvdata(dev);
	get_raw_data_all(ts, MMS_VSC_CMD_EXIT);
	ts->ft_flag = 0;

	return snprintf(buf, TSP_BUF_SIZE, "%u\n", 0);
}
static int check_rx_tx_num(void *device_data)
{
	struct mms_ts_data *ts = (struct mms_ts_data *)device_data;
	char buff[TSP_CMD_STR_LEN] = {0};
	int node;
	if (ts->cmd_param[0] < 0 ||
			ts->cmd_param[0] >= TX_NUM  ||
			ts->cmd_param[1] < 0 ||
			ts->cmd_param[1] >= RX_NUM) {
		snprintf(buff, sizeof(buff) , "%s", "NG");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = 3;

		dev_info(&ts->client->dev, "%s: parameter error: %u,%u\n",
				__func__, ts->cmd_param[0],
				ts->cmd_param[1]);
		node = -1;
		return node;
	}
	node = ts->cmd_param[1] * TX_NUM + ts->cmd_param[0];
	dev_info(&ts->client->dev, "%s: node = %d\n", __func__,
			node);
	return node;
}
static void not_support_cmd(void *device_data)
{
	struct mms_ts_data *ts = (struct mms_ts_data *)device_data;
	char buff[16] = {0};
	set_default_result(ts);
	snprintf(buff, sizeof(buff), "%s", "NA");
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = 4;
	dev_info(&ts->client->dev, "%s: \"%s(%d)\"\n", __func__,
				buff, strnlen(buff, sizeof(buff)));
	return;
}
static void fw_update(void *device_data)
{
	struct mms_ts_data *ts = (struct mms_ts_data *)device_data;
	struct i2c_client *client = ts->client;
	int ret, i;
	set_default_result(ts);
	for (i = 0; i < DOWNLOAD_RETRY_CNT; i++) {
		ret = MFS_ISC_update();
		printk(KERN_ERR "mcsdl_download_binary_data by kyestring : [%d]\n",
			ret);
		if (ret != 0) {
			printk(KERN_ERR \
			"SET Download Fail in factory mode-error code [%d]\n", ret);
			switch (ret) {
			case MRET_CHECK_COMPATIBILITY_ERROR:
				dev_err(&client->dev,
				"fw version update does not need - old module\n");
				goto do_not_need_update;

			case MRET_CHECK_VERSION_ERROR:
				dev_info(&client->dev,
				"fw version update does not need\n");
				goto do_not_need_update;

			case MRET_FIRMWARE_VERIFY_ERROR:
				dev_info(&client->dev, "fw verify error\n");
				goto do_not_need_update;

			default:
				dev_err(&client->dev, "invalid fw file type!!\n");
				goto not_support;
			}
		} else {
			ts->cmd_state = 2;
			break;
		}
	}
	return;
not_support:
do_not_need_update:
	ts->cmd_state = 2;
	return;
}
static void get_fw_ver_bin(void *device_data)
{
//	extern unsigned char TSP_PhoneVersion;
	struct mms_ts_data *ts = (struct mms_ts_data *)device_data;
	int NEW_FIRMWARE_VERSION = 0;
	char buff[16] = {0};
	check_firmware_version_func();
	NEW_FIRMWARE_VERSION = TSP_PhoneVersion;
	set_default_result(ts);
	snprintf(buff, sizeof(buff), "%#02x", NEW_FIRMWARE_VERSION);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = 2;
	dev_info(&ts->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

//hskang gps power onoff
extern int gps_power_ctl(int is_on);
//to reset gps power gps_power_reset init value should be 0
//not to reset gps power gps_power_reset init value should be 1
static int gps_power_reset = 0; 


static void get_fw_ver_ic(void *device_data)
{
//	extern unsigned char TSP_PanelVersion;
	struct mms_ts_data *ts = (struct mms_ts_data *)device_data;
	char buff[16] = {0};
	int ver;
	ts->fw_ic_ver = TSP_PanelVersion;
	set_default_result(ts);
	ver = ts->fw_ic_ver;
	snprintf(buff, sizeof(buff), "%#02x", ver);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = 2;
	dev_info(&ts->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));

}
static void get_config_ver(void *device_data)
{
	struct mms_ts_data *ts = (struct mms_ts_data *)device_data;
	char buff[20] = {0};
	set_default_result(ts);
	snprintf(buff, sizeof(buff), "%s", ts->config_fw_version);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = 2;
	dev_info(&ts->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}
static void get_threshold(void *device_data)
{
	struct mms_ts_data *ts = (struct mms_ts_data *)device_data;
	char buff[16] = {0};
	int threshold;
	if (IC_type == 0x0F)
		threshold = 15;
	else
		threshold = 30;

	set_default_result(ts);
	/*
	mms_i2c_read(ts->client, P5_THRESHOLD, 1, &threshold);
	*/
	if (threshold < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = 3;
		return;
	}
	snprintf(buff, sizeof(buff), "%d", threshold);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = 2;
	dev_info(&ts->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}
static void module_off_master(void *device_data)
{
	struct mms_ts_data *ts = (struct mms_ts_data *)device_data;
	char buff[3] = {0};
	ts->enabled = false;
	touch_is_pressed = 0;

	set_default_result(ts);
	ts_power_enable(0);
	snprintf(buff, sizeof(buff), "%s", "OK");

	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	if (strncmp(buff, "OK", 2) == 0)
		ts->cmd_state = 2;
	else
		ts->cmd_state = 3;
	dev_info(&ts->client->dev, "%s: %s\n", __func__, buff);
}
static void module_on_master(void *device_data)
{

	struct mms_ts_data *ts = (struct mms_ts_data *)device_data;
	char buff[3] = {0};
	ts->enabled = true;

	set_default_result(ts);
	ts_power_enable(1);
	snprintf(buff, sizeof(buff), "%s", "OK");

	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	if (strncmp(buff, "OK", 2) == 0)
		ts->cmd_state = 2;
	else
		ts->cmd_state = 3;
	dev_info(&ts->client->dev, "%s: %s\n", __func__, buff);

}
static void get_chip_vendor(void *device_data)
{
	struct mms_ts_data *ts = (struct mms_ts_data *)device_data;
	char buff[16] = {0};
	set_default_result(ts);
	snprintf(buff, sizeof(buff), "%s", "MELFAS");
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = 2;
	dev_info(&ts->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}
static void get_chip_name(void *device_data)
{
	struct mms_ts_data *ts = (struct mms_ts_data *)device_data;
	char buff[16] = {0};
	set_default_result(ts);
#if defined(CONFIG_MACH_KYLETD) || defined(CONFIG_MACH_VASTOI)
	snprintf(buff, sizeof(buff), "%s", "MMS134S");
#else
	snprintf(buff, sizeof(buff), "%s", "MMS133s");
#endif
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = 2;
	dev_info(&ts->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}
static void get_reference(void *device_data)
{
	struct mms_ts_data *ts = (struct mms_ts_data *)device_data;
	char buff[16] = {0};
	unsigned int val;
	int node;
	set_default_result(ts);
	node = check_rx_tx_num(ts);
	if (node < 0)
		return ;
	val = ts->reference[node];
	snprintf(buff, sizeof(buff), "%u", val);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = 2;
	dev_info(&ts->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}
static void get_cm_abs(void *device_data)
{
	struct mms_ts_data *ts = (struct mms_ts_data *)device_data;
	char buff[16] = {0};
	unsigned int val;
	int node;
	set_default_result(ts);
	node = check_rx_tx_num(ts);
	if (node < 0)
		return;
	val = ts->raw[node];
	snprintf(buff, sizeof(buff), "%u", val);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = 2;
	dev_info(&ts->client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));
	}
static void get_cm_delta(void *device_data)
{
	struct mms_ts_data *ts = (struct mms_ts_data *)device_data;
	char buff[16] = {0};
	unsigned int val;
	int node;
	set_default_result(ts);
	node = check_rx_tx_num(ts);
	if (node < 0)
		return;
	val = ts->inspection[node];
	snprintf(buff, sizeof(buff), "%u", val);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = 2;
	dev_info(&ts->client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));
}
/* CM ABS */
static int check_debug_value(struct mms_ts_data *ts)
{
	u8 setLowLevelData[4];
	u8 read_data_buf[50] = {0,};
//	u16 read_data_buf1[50] = {0,};
	char buff[TSP_CMD_STR_LEN] = {0};
	int sensing_ch, exciting_ch;
	int ret, i, j, status;
	int size;
	u32 max_value, min_value;
	u32 raw_data;
	tsp_testmode = 1;
	printk(KERN_ERR "[TSP] %s entered. line : %d\n", __func__, __LINE__);

	printk(KERN_ERR "[TSP] %s disable IRQ( %d)\n", __func__, __LINE__);
	disable_irq(ts->client->irq);
	exciting_ch = g_exciting_ch;
	sensing_ch = g_sensing_ch;
	/* Read Reference Data */
	setLowLevelData[0] = 0xA0; /* UNIVERSAL_CMD */
	setLowLevelData[1] = 0x40; /* UNIVCMD_ENTER_TEST_MODE */
	ret = mms_i2c_write(ts->client, setLowLevelData, 2);
	while (gpio_get_value(GPIO_I2C_INT))
		;

	ret = mms_i2c_read(ts->client, 0xAE, 1, read_data_buf);
	printk(KERN_ERR "\n\n --- CM_ABS --- ");
	/* Read Reference Data */
	setLowLevelData[0] = 0xA0;
	setLowLevelData[1] = 0x43; /* UNIVCMD_TEST_CM_ABS */
	ret = mms_i2c_write(ts->client, setLowLevelData, 2);
	while (gpio_get_value(GPIO_I2C_INT))
		;

	ret = mms_i2c_read(ts->client, 0xAE, 1, read_data_buf);
	printk(KERN_ERR "[TSP] %s ret= %d\n", __func__, ret);
	max_value = 0;
	min_value = 0;
	for (i = 0; i < sensing_ch; i++) {
		for (j = 0; j < exciting_ch; j++) {
			setLowLevelData[0] = 0xA0;
			setLowLevelData[1] = 0x44;
			setLowLevelData[2] = j;
			setLowLevelData[3] = i;
			ret = mms_i2c_write(ts->client, setLowLevelData, 4);
			while (gpio_get_value(GPIO_I2C_INT))
				;

			ret = mms_i2c_read(ts->client, 0xAE,
				1, read_data_buf);
			size = read_data_buf[0];
			ret = mms_i2c_read(ts->client, 0xAF,
				size, read_data_buf);
			CmABS_data[(i * exciting_ch) + j]
				= (read_data_buf[0] |  read_data_buf[1] << 8);
			raw_data = CmABS_data[(i * exciting_ch) + j];
			ts->raw[(i * exciting_ch) + j] = raw_data;
			if (i == 0 && j == 0) {
				max_value = min_value = raw_data;
			} else {
				max_value = max(max_value, raw_data);
				min_value = min(min_value, raw_data);
			}
			if ((CmABS_data[(i * exciting_ch) + j]
					>= SCR_ABS_LOWER_SPEC[i][j])
				&& (CmABS_data[(i * exciting_ch) + j]
					<= SCR_ABS_UPPER_SPEC[i][j]))
				status = 1; /* fail */
			else
				status = 0; /* pass */
		}
	}
	printk(KERN_ERR "[TSP] CmABS_data\n");
	for (i = 0; i < exciting_ch * sensing_ch; i++) {
		if (0 == i % exciting_ch)
			printk(KERN_INFO "\n");
		printk(KERN_ERR "%4d, ", CmABS_data[i]);
	}
	printk(KERN_INFO "\n");
	snprintf(buff, sizeof(buff), "%d,%d", min_value, max_value);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	/* Read Reference Data */
	setLowLevelData[0] = 0xA0;
	setLowLevelData[1] = 0x4F;
	ret = mms_i2c_write(ts->client, setLowLevelData, 2);

	printk(KERN_ERR "[TSP] %s enable IRQ( %d)\n", __func__, __LINE__);
	enable_irq(ts->client->irq);
	tsp_testmode = 0;
	TSP_reboot();
	printk(KERN_ERR "%s : end\n", __func__);
	return status;
}
static int check_delta_value(struct mms_ts_data *ts)
{
	u8 setLowLevelData[4];
	u8 read_data_buf[50] = {0,};
//	u16 read_data_buf1[50] = {0,};
	char buff[TSP_CMD_STR_LEN] = {0};
	int sensing_ch, exciting_ch;
	int ret, i, j, status=0;
	int gpio;
	int size;
	u32 max_value, min_value;
	u32 raw_data;
	printk(KERN_ERR "[TSP] %s entered. line : %d,\n", __func__, __LINE__);

	printk(KERN_ERR "[TSP] %s disable IRQ( %d)\n", __func__, __LINE__);
	gpio = irq_to_gpio(ts->client->irq);
	disable_irq(ts->client->irq);
	exciting_ch = g_exciting_ch;
	sensing_ch	 = g_sensing_ch;
	/* Read Reference Data */
	setLowLevelData[0] = 0xA0; /* UNIVERSAL_CMD */
	setLowLevelData[1] = 0x40; /* UNIVCMD_ENTER_TEST_MODE */
	ret = mms_i2c_write(ts->client, setLowLevelData, 2);
	while (gpio_get_value(gpio))
		;

	ret = mms_i2c_read(ts->client, 0xAE, 1, read_data_buf);
	printk(KERN_ERR "\n\n --- CM_DELTA --- ");
	/* Read Reference Data */
	setLowLevelData[0] = 0xA0;
	setLowLevelData[1] = 0x41;
	ret = mms_i2c_write(ts->client, setLowLevelData, 2);
	while (gpio_get_value(gpio))
		;

	ret = mms_i2c_read(ts->client, 0xAE, 1, read_data_buf);
	max_value = 0;
	min_value = 0;
	for (i = 0; i < sensing_ch; i++) {
		for (j = 0; j < exciting_ch; j++) {
			setLowLevelData[0] = 0xA0;
			setLowLevelData[1] = 0x42;
			setLowLevelData[2] = j; /* Exciting CH. */
			setLowLevelData[3] = i; /* Sensing CH. */
			ret = mms_i2c_write(ts->client, setLowLevelData, 4);
			while (gpio_get_value(gpio))
				;

			ret = mms_i2c_read(ts->client, 0xAE,
				1, read_data_buf);
			size = read_data_buf[0];
			ret = mms_i2c_read(ts->client, 0xAF,
				read_data_buf[0], read_data_buf);
			CmDelta_data[(i * exciting_ch) + j]
				= (read_data_buf[0] |  read_data_buf[1] << 8);
			raw_data = CmDelta_data[(i * exciting_ch) + j];
			ts->inspection[(i * exciting_ch) + j] = raw_data;
			if (i == 0 && j == 0) {
				max_value = min_value = raw_data;
			} else {
				max_value = max(max_value, raw_data);
				min_value = min(min_value, raw_data);
			}
		}
	}
	printk(KERN_ERR "[TSP] CmDelta_data\n");
	for (i = 0; i < exciting_ch * sensing_ch; i++) {
		if (0 == i % exciting_ch)
			printk(KERN_INFO "\n");
		printk(KERN_ERR "%4d, ", CmDelta_data[i]);
	}
	printk(KERN_ERR "min:%d,max:%d", min_value, max_value);
	printk(KERN_INFO "\n");
	snprintf(buff, sizeof(buff), "%d,%d", min_value, max_value);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	/* Read Reference Data */
	setLowLevelData[0] = 0xA0;
	setLowLevelData[1] = 0x4F;
	ret = mms_i2c_write(ts->client, setLowLevelData, 2);
	printk(KERN_ERR "[TSP] %s enable IRQ( %d)\n", __func__, __LINE__);
	enable_irq(ts->client->irq);
	tsp_testmode = 0;
	TSP_reboot();
	printk(KERN_ERR "%s : end\n", __func__);
	return status;
}
static void get_intensity(void *device_data)
{
	struct mms_ts_data *ts = (struct mms_ts_data *)device_data;

	char buff[16] = {0};
	unsigned int val;
	int node;
	set_default_result(ts);
	node = check_rx_tx_num(ts);
	if (node < 0)
		return ;
	val = ts->intensity[node];
	snprintf(buff, sizeof(buff), "%u", val);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = 2;
	dev_info(&ts->client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));
}
static void get_x_num(void *device_data)
{
	struct mms_ts_data *ts = (struct mms_ts_data *)device_data;
	char buff[16] = {0};
	int val;
	int exciting_ch;
	set_default_result(ts);
/*
	val = i2c_smbus_read_byte_data(ts->client, 0xEF);
*/
	exciting_ch = g_exciting_ch;
	val = exciting_ch;
	if (val < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = 3;
		dev_info(&ts->client->dev,
			"%s: fail to read num of x (%d).\n", __func__, val);
		return;
	}
	snprintf(buff, sizeof(buff), "%u", val);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = 2;
	dev_info(&ts->client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));
}
static void get_y_num(void *device_data)
{
	struct mms_ts_data *ts = (struct mms_ts_data *)device_data;
	char buff[16] = {0};
	int val;
	int sensing_ch;
	set_default_result(ts);
/*
	val = i2c_smbus_read_byte_data(ts->client, 0xEE);
*/
	sensing_ch = g_sensing_ch;
	val = sensing_ch;
	if (val < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = 3;
		dev_info(&ts->client->dev,
			"%s: fail to read num of y (%d).\n", __func__, val);
		return;
	}
	snprintf(buff, sizeof(buff), "%u", val);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = 2;
	dev_info(&ts->client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));
}
static void run_reference_read(void *device_data)
{
	struct mms_ts_data *ts = (struct mms_ts_data *)device_data;
	set_default_result(ts);
	get_raw_data_all(ts, MMS_VSC_CMD_REFER);
	ts->cmd_state = 2;
	dev_info(&ts->client->dev, "%s:\n", __func__);
}
static void run_cm_abs_read(void *device_data)
{
	struct mms_ts_data *ts = (struct mms_ts_data *)device_data;
	set_default_result(ts);
	check_debug_value(ts);
	ts->cmd_state = 2;
	dev_info(&ts->client->dev, "%s:\n", __func__);
}
static void run_cm_delta_read(void *device_data)
{
	struct mms_ts_data *ts = (struct mms_ts_data *)device_data;
	set_default_result(ts);
	check_delta_value(ts);
	ts->cmd_state = 2;
	dev_info(&ts->client->dev, "%s:\n", __func__);
}
static void run_intensity_read(void *device_data)
{
	struct mms_ts_data *ts = (struct mms_ts_data *)device_data;
	set_default_result(ts);
	get_raw_data_all(ts, MMS_VSC_CMD_INTENSITY);
	ts->cmd_state = 2;
	dev_info(&ts->client->dev, "%s:\n", __func__);
}
static ssize_t store_cmd(struct device *dev, struct device_attribute
		*devattr, const char *buf, size_t count)
{
	struct mms_ts_data *ts = dev_get_drvdata(dev);
	struct i2c_client *client = ts->client;
	char *cur, *start, *end;
	char buff[TSP_CMD_STR_LEN] = {0};
	int len, i;
	struct tsp_cmd *tsp_cmd_ptr = NULL;
	char delim = ',';
	bool cmd_found = false;
	int param_cnt = 0;
	if (ts->cmd_is_running == true) {
		dev_err(&ts->client->dev, "tsp_cmd: other cmd is running.\n");
		goto err_out;
	}
	/* check lock  */
	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = true;
	mutex_unlock(&ts->cmd_lock);
	ts->cmd_state = 1;
	for (i = 0; i < ARRAY_SIZE(ts->cmd_param); i++)
		ts->cmd_param[i] = 0;

	len = (int)count;
	if (*(buf + len - 1) == '\n')
		len--;
	memset(ts->cmd, 0x00, ARRAY_SIZE(ts->cmd));
	memcpy(ts->cmd, buf, len);
	cur = strchr(buf, (int)delim);
	if (cur)
		memcpy(buff, buf, cur - buf);
	else
		memcpy(buff, buf, len);
	/* find command */
	list_for_each_entry(tsp_cmd_ptr, &ts->cmd_list_head, list) {
		if (!strcmp(buff, tsp_cmd_ptr->cmd_name)) {
			cmd_found = true;
			break;
		}
	}
	/* set not_support_cmd */
	if (!cmd_found) {
		list_for_each_entry(tsp_cmd_ptr, &ts->cmd_list_head, list) {
			if (!strcmp("not_support_cmd", tsp_cmd_ptr->cmd_name))
				break;
		}
	}
	/* parsing parameters */
	if (cur && cmd_found) {
		cur++;
		start = cur;
		memset(buff, 0x00, ARRAY_SIZE(buff));
		do {
			if (*cur == delim || cur - buf == len) {
				end = cur;
				memcpy(buff, start, end - start);
				*(buff + strlen(buff)) = '\0';
				if (kstrtoint(buff, 10,
					ts->cmd_param + param_cnt) < 0)
					goto err_out;
				start = cur + 1;
				memset(buff, 0x00, ARRAY_SIZE(buff));
				param_cnt++;
			}
			cur++;
		} while (cur - buf <= len);
	}
	dev_info(&client->dev, "cmd = %s\n", tsp_cmd_ptr->cmd_name);
	for (i = 0; i < param_cnt; i++)
		dev_info(&client->dev, "cmd param %d= %d\n", i,
							ts->cmd_param[i]);
	/*for*/
	tsp_cmd_ptr->cmd_func(ts);
err_out:
	return count;
}
static ssize_t show_cmd_status(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct mms_ts_data *ts = dev_get_drvdata(dev);
	char buff[16] = {0};
	dev_info(&ts->client->dev, "tsp cmd: status:%d\n",
			ts->cmd_state);
	if (ts->cmd_state == 0)
		snprintf(buff, sizeof(buff), "WAITING");
	else if (ts->cmd_state == 1)
		snprintf(buff, sizeof(buff), "RUNNING");
	else if (ts->cmd_state == 2)
		snprintf(buff, sizeof(buff), "OK");
	else if (ts->cmd_state == 3)
		snprintf(buff, sizeof(buff), "FAIL");
	else if (ts->cmd_state == 4)
		snprintf(buff, sizeof(buff), "NOT_APPLICABLE");
	else
		snprintf(buff, sizeof(buff), "%d", ts->cmd_state);
	return snprintf(buf, TSP_BUF_SIZE, "%s\n", buff);
}
static ssize_t show_cmd_result(struct device *dev, struct device_attribute
		*devattr, char *buf)
{
	struct mms_ts_data *ts = dev_get_drvdata(dev);
	dev_info(&ts->client->dev, "tsp cmd: result: %s\n", ts->cmd_result);
	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);
	ts->cmd_state = 0;
	return snprintf(buf, TSP_BUF_SIZE, "%s\n", ts->cmd_result);
}
#ifdef ESD_DEBUG
static bool intensity_log_flag;
static ssize_t show_intensity_logging_on(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct mms_ts_data *ts = dev_get_drvdata(dev);
	struct i2c_client *client = ts->client;
	struct file *fp;
	char log_data[160] = {0,};
	char buff[16] = {0,};
	mm_segment_t old_fs;
	long nwrite;
	u32 val;
	int i, y, c;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
#define MELFAS_DEBUG_LOG_PATH "/sdcard/melfas_log"
	dev_info(&client->dev, "%s: start.\n", __func__);
	fp = filp_open(MELFAS_DEBUG_LOG_PATH, O_RDWR|O_CREAT,
			S_IRWXU|S_IRWXG|S_IRWXO);
	if (IS_ERR(fp)) {
		dev_err(&client->dev, "%s: fail to open log file\n", __func__);
		goto open_err;
	}
	intensity_log_flag = 1;
	do {
		for (y = 0; y < 3; y++) {
			/* for tx chanel 0~2 */
			memset(log_data, 0x00, 160);
			snprintf(buff, 16, "%1u: ", y);
			strncat(log_data, buff, strnlen(buff, 16));
			for (i = 0; i < RX_NUM; i++) {
				val = get_raw_data_one(ts, i, y,
						MMS_VSC_CMD_INTENSITY);
				snprintf(buff, 16, "%5u, ", val);
				strncat(log_data, buff, strnlen(buff, 16));
			}
			memset(buff, '\n', 2);
			c = (y == 2) ? 2 : 1;
			strncat(log_data, buff, c);
			nwrite = vfs_write(fp, (const char __user *)log_data,
					strnlen(log_data, 160), &fp->f_pos);
		}
		usleep_range(5000);
	} while (intensity_log_flag);
	filp_close(fp, current->files);
	set_fs(old_fs);
	return 0;
 open_err:
	set_fs(old_fs);
	return FAIL;
}
static ssize_t show_intensity_logging_off(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct mms_ts_data *ts = dev_get_drvdata(dev);
	intensity_log_flag = 0;
	usleep_range(10000);
	get_raw_data_all(ts, MMS_VSC_CMD_EXIT);
	return 0;
}
#endif
#ifdef SEC_TKEY_FACTORY_TEST
static ssize_t tkey_threshold_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_data *ts = dev_get_drvdata(dev);
	struct i2c_client *client = ts->client;
	int tkey_threshold;
	tkey_threshold = i2c_smbus_read_byte_data(ts->client,
						VSC_THRESHOLD_TK);
	dev_info(&client->dev, "touch key threshold: %d\n", tkey_threshold);

	return snprintf(buf, sizeof(int), "%d\n", tkey_threshold);
}
static ssize_t back_key_state_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_data *ts = dev_get_drvdata(dev);
	struct i2c_client *client = ts->client;
	int i, ret, val;
	for (i = 0; i < ARRAY_SIZE(ts->keycode); i++) {
		if (ts->keycode[i] == KEY_BACK)
			break;
	}
	dev_info(&client->dev, "back key state: %d\n", ts->key_pressed[i]);
	/* back key*/
	printk(KERN_ERR "[TSP] %s disable IRQ( %d)\n", __func__, __LINE__);
	disable_irq(ts->irq);

	ret = get_raw_data_one(ts, 0, 0, VSC_INTENSITY_TK);
	if (ret < 0)
		dev_err(&client->dev, "%s: fail to read (%d)\n", __func__, ret);

	printk(KERN_ERR "[TSP] %s enable IRQ( %d)\n", __func__, __LINE__);
	enable_irq(ts->irq);
	val = (u16)ret;
	dev_info(&client->dev, "%s: val=%d\n", __func__, val);
	return snprintf(buf, sizeof(buf), "%d\n", val);
}
static ssize_t home_key_state_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_data *ts = dev_get_drvdata(dev);
	struct i2c_client *client = ts->client;
	int i, ret, val;
	for (i = 0; i < ARRAY_SIZE(ts->keycode); i++) {
		if (ts->keycode[i] == KEY_HOMEPAGE)
			break;
	}
	dev_info(&client->dev, "back key state: %d\n", ts->key_pressed[i]);
	/* home key*/
	printk(KERN_ERR "[TSP] %s disable IRQ( %d)\n", __func__, __LINE__);
	disable_irq(ts->irq);

	ret = get_raw_data_one(ts, 0, 1, VSC_INTENSITY_TK);
	if (ret < 0)
		dev_err(&client->dev, "%s: fail to read (%d)\n", __func__, ret);

	printk(KERN_ERR "[TSP] %s enable IRQ( %d)\n", __func__, __LINE__);
	enable_irq(ts->irq);
	val = (u16)ret;
	dev_info(&client->dev, "%s: val=%d\n", __func__, val);
	return snprintf(buf, sizeof(buf), "%d\n", val);
}
static ssize_t recent_key_state_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_data *ts = dev_get_drvdata(dev);
	struct i2c_client *client = ts->client;
	int i, ret, val;
	for (i = 0; i < ARRAY_SIZE(ts->keycode); i++) {
		if (ts->keycode[i] == KEY_F3)
			break;
	}
	dev_info(&client->dev, "back key state: %d\n", ts->key_pressed[i]);
	/* recent key*/
	printk(KERN_ERR "[TSP] %s disable IRQ( %d)\n", __func__, __LINE__);
	disable_irq(ts->irq);

	ret = get_raw_data_one(ts, 0, 2, VSC_INTENSITY_TK);
	if (ret < 0)
		dev_err(&client->dev, "%s: fail to read (%d)\n", __func__, ret);

	printk(KERN_ERR "[TSP] %s enable IRQ( %d)\n", __func__, __LINE__);
	enable_irq(ts->irq);
	val = (u16)ret;
	dev_info(&client->dev, "%s: val=%d\n", __func__, val);
	return snprintf(buf, sizeof(buf), "%d\n", val);
}
static ssize_t menu_key_state_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_data *ts = dev_get_drvdata(dev);
	struct i2c_client *client = ts->client;
	int i, ret, val;
	for (i = 0; i < ARRAY_SIZE(ts->keycode); i++) {
		if (ts->keycode[i] == KEY_MENU)
			break;
	}
	dev_info(&client->dev, "back key state: %d\n", ts->key_pressed[i]);
	/* recent key*/
	printk(KERN_ERR "[TSP] %s disable IRQ( %d)\n", __func__, __LINE__);
	disable_irq(ts->irq);

	ret = get_raw_data_one(ts, 0, 3, VSC_INTENSITY_TK);
	if (ret < 0)
		dev_err(&client->dev, "%s: fail to read (%d)\n", __func__, ret);

	printk(KERN_ERR "[TSP] %s enable IRQ( %d)\n", __func__, __LINE__);
	enable_irq(ts->irq);
	val = (u16)ret;
	dev_info(&client->dev, "%s: val=%d\n", __func__, val);
	return snprintf(buf, sizeof(buf), "%d\n", val);
}
static ssize_t tkey_rawcounter_show0(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_data *ts = dev_get_drvdata(dev);
	struct i2c_client *client = ts->client;
	u32 ret;
	u16 val;
	/* back key*/
	printk(KERN_ERR "[TSP] %s disable IRQ( %d)\n", __func__, __LINE__);
	disable_irq(ts->irq);

	ret = get_raw_data_one(ts, 0, 0, VSC_RAW_TK);
	if (ret < 0)
		dev_err(&client->dev, "%s: fail to read (%d)\n", __func__, ret);

	printk(KERN_ERR "[TSP] %s enable IRQ( %d)\n", __func__, __LINE__);
	enable_irq(ts->irq);
	val = (u16)ret;
	dev_info(&client->dev, "%s: val=%d\n", __func__, val);
	return snprintf(buf, sizeof(buf), "%d\n", val);
}
static ssize_t tkey_rawcounter_show1(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_data *ts = dev_get_drvdata(dev);
	struct i2c_client *client = ts->client;
	int ret;
	u16 val;
	/* home key*/
	printk(KERN_ERR "[TSP] %s disable IRQ( %d)\n", __func__, __LINE__);
	disable_irq(ts->irq);

	ret = get_raw_data_one(ts, 0, 1, VSC_RAW_TK);
	if (ret < 0)
		dev_err(&client->dev, "%s: fail to read (%d)\n", __func__, ret);

	printk(KERN_ERR "[TSP] %s enable IRQ( %d)\n", __func__, __LINE__);
	enable_irq(ts->irq);
	val = (u16)ret;
	dev_info(&client->dev, "%s: val=%d\n", __func__, val);
	return snprintf(buf, sizeof(buf), "%d\n", val);
}
static ssize_t tkey_rawcounter_show2(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_data *ts = dev_get_drvdata(dev);
	struct i2c_client *client = ts->client;
	int ret;
	u16 val;
	/* recent key*/
	printk(KERN_ERR "[TSP] %s disable IRQ( %d)\n", __func__, __LINE__);
	disable_irq(ts->irq);

	ret = get_raw_data_one(ts, 0, 2, VSC_RAW_TK);
	if (ret < 0)
		dev_err(&client->dev, "%s: fail to read (%d)\n", __func__, ret);

	printk(KERN_ERR "[TSP] %s enable IRQ( %d)\n", __func__, __LINE__);
	enable_irq(ts->irq);
	val = (u16)ret;
	dev_info(&client->dev, "%s: val=%d\n", __func__, val);
	return snprintf(buf, sizeof(buf), "%d\n", val);
}
static ssize_t tkey_rawcounter_show3(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_data *ts = dev_get_drvdata(dev);
	struct i2c_client *client = ts->client;
	int ret;
	u16 val;
	/* menu key*/
	printk(KERN_ERR "[TSP] %s disable IRQ( %d)\n", __func__, __LINE__);
	disable_irq(ts->irq);

	ret = get_raw_data_one(ts, 0, 3, VSC_RAW_TK);
	if (ret < 0)
		dev_err(&client->dev, "%s: fail to read (%d)\n", __func__, ret);

	printk(KERN_ERR "[TSP] %s enable IRQ( %d)\n", __func__, __LINE__);
	enable_irq(ts->irq);
	val = (u16)ret;
	dev_info(&client->dev, "%s: val=%d\n", __func__, val);
	return snprintf(buf, sizeof(buf), "%d\n", val);
}
#endif
#ifdef SEC_TKEY_FACTORY_TEST
static DEVICE_ATTR(touchkey_threshold, S_IRUGO, tkey_threshold_show, NULL);
static DEVICE_ATTR(touchkey_back, S_IRUGO, back_key_state_show, NULL);
static DEVICE_ATTR(touchkey_home, S_IRUGO, home_key_state_show, NULL);
static DEVICE_ATTR(touchkey_recent, S_IRUGO, recent_key_state_show, NULL);
static DEVICE_ATTR(touchkey_menu, S_IRUGO, menu_key_state_show, NULL);
static DEVICE_ATTR(touchkey_raw_data0, S_IRUGO, tkey_rawcounter_show0, NULL);
static DEVICE_ATTR(touchkey_raw_data1, S_IRUGO, tkey_rawcounter_show1, NULL);
static DEVICE_ATTR(touchkey_raw_data2, S_IRUGO, tkey_rawcounter_show2, NULL);
static DEVICE_ATTR(touchkey_raw_data3, S_IRUGO, tkey_rawcounter_show3, NULL);
static struct attribute *touchkey_attributes[] = {
	&dev_attr_touchkey_threshold.attr,
	&dev_attr_touchkey_back.attr,
	&dev_attr_touchkey_home.attr,
	&dev_attr_touchkey_recent.attr,
	&dev_attr_touchkey_menu.attr,
	&dev_attr_touchkey_raw_data0.attr,
	&dev_attr_touchkey_raw_data1.attr,
	&dev_attr_touchkey_raw_data2.attr,
	&dev_attr_touchkey_raw_data3.attr,
	NULL,
};
static struct attribute_group touchkey_attr_group = {
	.attrs = touchkey_attributes,
};
static int factory_init_tk(struct mms_ts_data *ts)
{
	struct i2c_client *client = ts->client;
	int ret;
	ts->dev_tk = device_create(sec_class, NULL, (dev_t)NULL, ts,
								"sec_touchkey");
	if (IS_ERR(ts->dev_tk)) {
		dev_err(&client->dev, "Failed to create fac touchkey dev\n");
		ret = -ENODEV;
		ts->dev_tk = NULL;
		goto err_create_dev_tk;
	}
	ret = sysfs_create_group(&ts->dev_tk->kobj, &touchkey_attr_group);
	if (ret) {
		dev_err(&client->dev,
			"Failed to create sysfs (touchkey_attr_group).\n");
		ret = (ret > 0) ? -ret : ret;
		goto err_create_tk_sysfs;
	}
	ts->key_pressed = kzalloc(sizeof(bool) * ARRAY_SIZE(ts->keycode),
								GFP_KERNEL);
	if (!ts->key_pressed) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_alloc;
	}
	return 0;
err_alloc:
	sysfs_remove_group(&ts->dev_tk->kobj, &touchkey_attr_group);
err_create_tk_sysfs:
err_create_dev_tk:
	return ret;
}
#endif
static DEVICE_ATTR(close_tsp_test, S_IRUGO, show_close_tsp_test, NULL);
static DEVICE_ATTR(cmd, S_IWUSR | S_IWGRP, NULL, store_cmd);
static DEVICE_ATTR(cmd_status, S_IRUGO, show_cmd_status, NULL);
static DEVICE_ATTR(cmd_result, S_IRUGO, show_cmd_result, NULL);
#ifdef ESD_DEBUG
static DEVICE_ATTR(intensity_logging_on, S_IRUGO, show_intensity_logging_on,
		NULL);
static DEVICE_ATTR(intensity_logging_off, S_IRUGO, show_intensity_logging_off,
		NULL);
#endif
static struct attribute *sec_touch_facotry_attributes[] = {
	&dev_attr_close_tsp_test.attr,
	&dev_attr_cmd.attr,
	&dev_attr_cmd_status.attr,
	&dev_attr_cmd_result.attr,
#ifdef ESD_DEBUG
	&dev_attr_intensity_logging_on.attr,
	&dev_attr_intensity_logging_off.attr,
#endif
	NULL,
};
static struct attribute_group sec_touch_factory_attr_group = {
	.attrs = sec_touch_facotry_attributes,
};
#endif /* SEC_TSP_FACTORY_TEST */
//static int tsp_reboot_count;
static int mms_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
#if 0
	struct device *qt602240_noise_test;
#ifdef TA_DETECTION
	bool ta_status;
#endif
#endif
#ifdef SEC_TSP_FACTORY_TEST
	struct device *fac_dev_ts;
#endif
	int ret = 0, i;
#ifdef CONFIG_MACH_KYLETD
	u8 read_data_buf[1] = {0,};
	int crc_b1, crc_b2;
#endif
#if DEBUG_PRINT
	printk(KERN_ERR "%s start.\n", __func__);
#endif
	tsp_enabled = false;
	
	ts_power_enable(1);
	msleep(60);
	
	ts_power_enable(0);
	msleep(60);
	ts_power_enable(1);
	msleep(80);

	g_exciting_ch = 19;
	g_sensing_ch = 12;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "%s: need I2C_FUNC_I2C\n", __func__);
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}
	ts = kmalloc(sizeof(struct mms_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		printk(KERN_ERR "%s: failed to create a state of mms-ts\n", __func__);
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}
#if 0
	ts_data = ts;
	data = client->dev.platform_data;
	ts->power = data->power;
	ts->gpio = data->gpio;
	ts->version = data->version;
#ifdef TA_DETECTION
	ts->register_cb = data->register_cb;
	ts->read_ta_status = data->read_ta_status;
#endif
	ts->client = client;
	i2c_set_clientdata(client, ts);
	ts->power(true);
#endif
	ts->client = client;
	i2c_set_clientdata(client, ts);
#if 1
	printk(KERN_ERR "%s: i2c_master_send() [%d], Add[%d]\n", __func__,
		ret, ts->client->addr);
#endif
#if SET_DOWNLOAD_BY_GPIO
#if 0
	buf[0] = TS_READ_VERSION_ADDR;
	ret = i2c_master_send(ts->client, &buf, 1);
	if (ret < 0) {
		printk(KERN_ERR "mms_ts_work_func : i2c_master_send [%d]\n", ret);
	}
	ret = i2c_master_recv(ts->client, &buf, 3);
#endif
	for (i = 0; i < DOWNLOAD_RETRY_CNT; i++) {
		ret = MFS_ISC_update();
		printk(KERN_ERR "mcsdl_download_binary_data : [%d]\n", ret);
		if (ret != 0)
			printk(KERN_ERR "SET Download Fail - error code [%d]\n", ret);
		else
			break;
	}
#ifdef CONFIG_MACH_KYLETD	/* not use Kyle-i TSP firmware */
	if (TSP_PanelVersion <= TSP_PhoneVersion) {
		ret = mms_i2c_read(ts->client, 0xb1,
			1, read_data_buf);
		crc_b1 = read_data_buf[0];
		printk(KERN_ERR "[TSP] PROBE CRC 0xb1=%d)\n", crc_b1);

		ret = mms_i2c_read(ts->client, 0xb2,
			1, read_data_buf);
		crc_b2 = read_data_buf[0];
		printk(KERN_ERR "[TSP] PROBE CRC 0xb2=%d)\n", crc_b2);

		if(crc_b2 != GFF_V20_CRC) {
//		if ((IC_type == 0x0F && crc_b2 != G1F_V20_CRC) ||(IC_type == 0x0D &&  crc_b2 != G1M_V34_CRC))  {
			printk(KERN_ERR "[TSP]Enter CRC_check_errror=%d\n");
			for (i = 0; i < DOWNLOAD_RETRY_CNT; i++) {
				ret = MFS_ISC_force_update();
			printk(KERN_ERR "mcsdl_download_binary_data_in_CRC_check : [%d]\n",
					ret);
			if (ret != 0)
				printk(KERN_ERR "SET Download Fail - error code_in_CRC_check [%d]\n",
						ret);
			else
				break;
			}
		}
	}
#endif
#endif
	ts->input_dev = input_allocate_device();
	if (!ts->input_dev) {
		printk(KERN_ERR "%s: Not enough memory\n", __func__);
		ret = -ENOMEM;
		goto err_input_dev_alloc_failed;
	}
	ts->input_dev->name = "sec_touchscreen" ;
	ts->input_dev->evbit[0] = BIT_MASK(EV_ABS) | BIT_MASK(EV_KEY);
	ts->input_dev->keybit[BIT_WORD(KEY_MENU)] |= BIT_MASK(KEY_MENU);
	ts->input_dev->keybit[BIT_WORD(KEY_HOME)] |= BIT_MASK(KEY_HOME);
	ts->input_dev->keybit[BIT_WORD(KEY_BACK)] |= BIT_MASK(KEY_BACK);
	ts->input_dev->keybit[BIT_WORD(KEY_SEARCH)] |= BIT_MASK(KEY_SEARCH);
	input_mt_init_slots(ts->input_dev, MELFAS_MAX_TOUCH);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X,
						0, TS_MAX_X_COORD, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y,
						0, TS_MAX_Y_COORD, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR,
						0, TS_MAX_Z_TOUCH, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID,
						0, MELFAS_MAX_TOUCH-1, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR,
						0, TS_MAX_W_TOUCH, 0, 0);
	__set_bit(EV_LED, ts->input_dev->evbit);
	__set_bit(LED_MISC, ts->input_dev->ledbit);
	ret = input_register_device(ts->input_dev);
	if (ret) {
		printk(KERN_ERR "%s: Failed to register device\n", __func__);
		ret = -ENOMEM;
		goto err_input_register_device_failed;
	}

#if TOUCH_BOOSTER
	mutex_init(&ts->dvfs_lock);
	INIT_DELAYED_WORK(&ts->work_dvfs_off, set_dvfs_off);
	INIT_DELAYED_WORK(&ts->work_dvfs_chg, change_dvfs_lock);
	ts->dvfs_lock_status = false;
#endif
	ret = gpio_request(GPIO_I2C_INT, "mms_tsp_int");
	if(!ret)
	{
		gpio_direction_input(GPIO_I2C_INT);
	}
	else
	{
		printk("gpio request fail!\n");
	}
	ts->client->irq = gpio_to_irq(GPIO_I2C_INT);

	if (ts->client->irq) {
#if DEBUG_PRINT
		printk(KERN_ERR "%s: trying to request irq: %s-%d\n", __func__,
								ts->client->name, ts->client->irq);
#endif
		ret = request_threaded_irq(client->irq, NULL, mms_ts_irq_handler,
							IRQF_TRIGGER_LOW| IRQF_ONESHOT, ts->client->name, ts);
		if (ret > 0) {
			printk(KERN_ERR "%s: Can't allocate irq %d, ret %d\n",
				__func__, ts->client->irq, ret);
			ret = -EBUSY;
			goto err_request_irq;
		}
	}
	for (i = 0; i < MELFAS_MAX_TOUCH ; i++)  /* _SUPPORT_MULTITOUCH_ */
		g_Mtouch_info[i].strength = -1;

#if 0
	printk(KERN_ERR "[TSP] tsp_enabled is %d", tsp_enabled);
	data->register_cb(tsp_ta_probe);
	if (data->read_ta_status) {
		data->read_ta_status(&ta_status);
		printk(KERN_ERR "[TSP] ta_status is %d", ta_status);
		tsp_ta_probe(ta_status);
	}
#endif
#if DEBUG_PRINT
	printk(KERN_ERR "%s: succeed to register input device\n", __func__);
#endif
#if 0
	sec_touchscreen = device_create(sec_class, NULL, 0, ts, "sec_touchscreen");
	if (IS_ERR(sec_touchscreen))
		pr_err("[TSP] Failed to create device for the sysfs\n");
	ret = sysfs_create_group(&sec_touchscreen->kobj, &sec_touch_attr_group);
	if (ret)
		pr_err("[TSP] Failed to create sysfs group\n");
#endif
#if 0
	qt602240_noise_test = device_create(sec_class, NULL, 0, ts, "qt602240_noise_test");
	if (IS_ERR(qt602240_noise_test))
		pr_err("[TSP] Failed to create device for the sysfs\n");
	ret = sysfs_create_group(&qt602240_noise_test->kobj, &sec_touch_factory_attr_group);
	if (ret)
		pr_err("[TSP] Failed to create sysfs group\n");
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	printk(KERN_ERR "%s: register earlysuspend.\n", __func__);
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = mms_ts_early_suspend;
	ts->early_suspend.resume = mms_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif
#ifdef SEC_TSP_FACTORY_TEST
	INIT_LIST_HEAD(&ts->cmd_list_head);
	for (i = 0; i < ARRAY_SIZE(tsp_cmds); i++)
		list_add_tail(&tsp_cmds[i].list, &ts->cmd_list_head);
	mutex_init(&ts->cmd_lock);
	ts->cmd_is_running = false;
	ts->cmd_state = 0;
	ts->noise_mode = 0;
	if (IC_type == 0x0F)
		ts->config_fw_version = "S7568_0526_G1F";
	else{
		ts->config_fw_version = "S7568_0526_G1M";
		#if defined(CONFIG_MACH_KYLETD) || defined(CONFIG_MACH_VASTOI)	
		ts->config_fw_version = "S7568_0526_GFF";
		#endif
	}

	fac_dev_ts = device_create(sec_class,
			NULL, 0, ts, "tsp");
	if (IS_ERR(fac_dev_ts))
		dev_err(&client->dev, "Failed to create device for the sysfs\n");
	ret = sysfs_create_group(&fac_dev_ts->kobj,
			       &sec_touch_factory_attr_group);
	if (ret)
		dev_err(&client->dev, "Failed to create sysfs group\n");
#endif
#ifdef USE_TEST_RAW_TH_DATA_MODE
	sema_init(&touch_dev->raw_data_lock, 1);
	misc_touch_dev = touch_dev;
	ret = misc_register(&touch_misc_device);
	if (ret) {
		zinitix_debug_msg("Fail to register touch misc device.\n");
	}
	if (device_create_file(touch_misc_device.this_device,
		&dev_attr_get_touch_test_raw_data) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n",
			dev_attr_get_touch_test_raw_data.attr.name);
	if (device_create_file(touch_misc_device.this_device,
		&dev_attr_raw_enable) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n",
			dev_attr_raw_enable.attr.name);
	if (device_create_file(touch_misc_device.this_device,
		&dev_attr_raw_disable) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n",
			dev_attr_raw_disable.attr.name);
	if (device_create_file(touch_misc_device.this_device,
		&dev_attr_raw_show) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n",
			dev_attr_raw_show.attr.name);
#endif
	sec_touchscreen_dev = device_create(sec_class,
	NULL, 0, NULL, "sec_touchscreen");
	if (IS_ERR(sec_touchscreen_dev))
		pr_err("Failed to create device(sec_touchscreen)!\n");
	if (device_create_file(sec_touchscreen_dev,
			&dev_attr_tsp_firm_version_phone) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_tsp_firm_version_phone.attr.name);
	if (device_create_file(sec_touchscreen_dev,
			&dev_attr_tsp_firm_version_panel) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_tsp_firm_version_panel.attr.name);
	if (device_create_file(sec_touchscreen_dev,
			&dev_attr_tsp_firm_update) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_tsp_firm_update.attr.name);
	if (device_create_file(sec_touchscreen_dev,
			&dev_attr_tsp_threshold) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_tsp_threshold.attr.name);
	if (device_create_file(sec_touchscreen_dev,
			&dev_attr_tsp_firm_update_status) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_tsp_firm_update_status.attr.name);
	if (device_create_file(sec_touchscreen_dev,
			&dev_attr_set_all_reference) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_set_all_reference.attr.name);
	if (device_create_file(sec_touchscreen_dev,
			&dev_attr_disp_all_refdata) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_disp_all_refdata.attr.name);
	if (device_create_file(sec_touchscreen_dev,
			&dev_attr_set_all_inspection) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_set_all_inspection.attr.name);
	if (device_create_file(sec_touchscreen_dev,
			&dev_attr_disp_all_insdata) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_disp_all_insdata.attr.name);
	if (device_create_file(sec_touchscreen_dev,
			&dev_attr_set_all_intensity) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_set_all_intensity.attr.name);
	if (device_create_file(sec_touchscreen_dev,
			&dev_attr_disp_all_intdata) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_disp_all_intdata.attr.name);
	if (device_create_file(sec_touchscreen_dev,
			&dev_attr_firmware) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_firmware.attr.name);
	if (device_create_file(sec_touchscreen_dev,
			&dev_attr_raw_value) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_raw_value.attr.name);
	if (device_create_file(sec_touchscreen_dev,
			&dev_attr_touchtype) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_touchtype.attr.name);
	if (device_create_file(sec_touchscreen_dev,
			&dev_attr_set_module_off) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_set_module_off.attr.name);
	if (device_create_file(sec_touchscreen_dev,
			    &dev_attr_set_module_on) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_set_module_on.attr.name);
	sec_touchkey_dev = device_create(sec_class,
			NULL, 0, NULL, "sec_touchkey");
	if (IS_ERR(sec_touchkey_dev))
		pr_err("Failed to create device(sec_touchscreen)!\n");
	if (device_create_file(sec_touchkey_dev,
			&dev_attr_touchkey_back) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_touchkey_back.attr.name);
	if (device_create_file(sec_touchkey_dev,
			&dev_attr_touchkey_menu) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_touchkey_menu.attr.name);
	if (device_create_file(sec_touchkey_dev,
			&dev_attr_touchkey_raw_data) < 0)
		pr_err("Failed to create device file(%s)!\n",
			dev_attr_touchkey_raw_data.attr.name);
	if (device_create_file(sec_touchkey_dev, &dev_attr_touch_sensitivity) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_touch_sensitivity.attr.name);
	if (device_create_file(sec_touchkey_dev,
			&dev_attr_touchkey_threshold) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_touchkey_threshold.attr.name);
	if (device_create_file(sec_touchkey_dev,
			&dev_attr_touchkey_firm_version_phone) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_touchkey_firm_version_phone.attr.name);
	if (device_create_file(sec_touchkey_dev,
			&dev_attr_touchkey_firm_version_panel) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_touchkey_firm_version_panel.attr.name);
	tsp_enabled = true;
	return 0;
#if 0
	TSP_boost(ts, is_boost);
#endif
#if DEBUG_PRINT
	printk(KERN_ERR "%s: Start touchscreen. name: %s, irq: %d\n",
		__func__, ts->client->name, ts->client->irq);
#endif
	return 0;

err_detect_failed:
	ts->power(false);
	printk(KERN_ERR "mms-ts: err_detect failed\n");
	kfree(ts);
err_request_irq:
	printk(KERN_ERR "mms-ts: err_request_irq failed\n");
	free_irq(client->irq, ts);
err_input_register_device_failed:
	printk(KERN_ERR "mms-ts: err_input_register_device failed\n");
	input_free_device(ts->input_dev);
err_input_dev_alloc_failed:
	printk(KERN_ERR "mms-ts: err_input_dev_alloc failed\n");
err_alloc_data_failed:
	printk(KERN_ERR "mms-ts: err_alloc_data failed_\n");
#if 0
	if (tsp_reboot_count < 3) {
		tsp_reboot_count++;
		goto init_again;
	}
#endif
err_check_functionality_failed:
	printk(KERN_ERR "mms-ts: err_check_functionality failed_\n");
	return ret;
}
static int mms_ts_remove(struct i2c_client *client)
{
	struct mms_ts_data *ts = i2c_get_clientdata(client);
	unregister_early_suspend(&ts->early_suspend);
	free_irq(client->irq, ts);
	ts->power(false);
	input_unregister_device(ts->input_dev);
	kfree(ts);
	return 0;
}
static int mms_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct mms_ts_data *ts = i2c_get_clientdata(client);
	int ret;
	u8 setLowLevelData[2];
	tsp_enabled = false;
	printk(KERN_ERR "[TSP] %s disable IRQ( %d)\n", __func__, __LINE__);
	disable_irq(client->irq);
	release_all_fingers(ts);
	touch_is_pressed = 0;

	setLowLevelData[0] = 0xB0;
	setLowLevelData[1] = 0x01;
	ret = mms_i2c_write(ts->client, setLowLevelData, 2);

	msleep(100);
	ts_power_enable(0);
	return 0;
}
static int mms_ts_resume(struct i2c_client *client)
{
//	extern unsigned char TSP_PanelVersion;
//	extern unsigned char TSP_PhoneVersion;
	struct mms_ts_data *ts = i2c_get_clientdata(client);
#if !defined(CONFIG_MACH_KYLETD) && !defined(CONFIG_MACH_VASTOI)/* not use Kyle-i TSP firmware */	
	u8 read_data_buf[1] = {0,};
	int ret, i;
	int crc_b1, crc_b2;
#endif
#if 0
	bool ta_status = 0;
#endif

#if 0
	TSP_boost(ts, is_boost);
#endif
	ts_power_enable(1);

	msleep(50);
	mms_set_noise_mode(ts);
#if !defined(CONFIG_MACH_KYLETD) && !defined(CONFIG_MACH_VASTOI)	/* not use Kyle-i TSP firmware */
	if (TSP_PanelVersion <= TSP_PhoneVersion) {
		ret = mms_i2c_read(ts->client, 0xb1,
			1, read_data_buf);
		crc_b1 = read_data_buf[0];
		printk(KERN_ERR "[TSP] RESUME CRC 0xb1=%d)\n", crc_b1);

		ret = mms_i2c_read(ts->client, 0xb2,
			1, read_data_buf);
		crc_b2 = read_data_buf[0];
		printk(KERN_ERR "[TSP] RESUME CRC 0xb2=%d)\n", crc_b2);

		if(crc_b2 != GFF_V19_CRC) {
//		if ((IC_type == 0x0F && crc_b2 != G1F_V20_CRC) ||(IC_type == 0x0D &&  crc_b2 != G1M_V34_CRC))  {
			printk(KERN_ERR "[TSP]Enter CRC_check_errror=%d\n");
			for (i = 0; i < DOWNLOAD_RETRY_CNT; i++) {
				ret = MFS_ISC_force_update();
			printk(KERN_ERR "mcsdl_download_binary_data_in_CRC_check : [%d]\n",
					ret);
			if (ret != 0)
				printk(KERN_ERR "SET Download Fail - error code_in_CRC_check [%d]\n",
						ret);
			else
				break;
			}
		}
	}
#endif
	tsp_enabled = true;
	printk(KERN_ERR "[TSP] %s enable IRQ( %d)\n", __func__, __LINE__);
	enable_irq(client->irq);
#if 0
	if (ts->read_ta_status) {
		ts->read_ta_status(&ta_status);
		printk(KERN_ERR "[TSP] ta_status is %d", ta_status);
		tsp_ta_probe(ta_status);
	}
#endif

	return 0;
}
#ifdef CONFIG_HAS_EARLYSUSPEND
static void mms_ts_early_suspend(struct early_suspend *h)
{
	struct mms_ts_data *ts;
	ts = container_of(h, struct mms_ts_data, early_suspend);
	mms_ts_suspend(ts->client, PMSG_SUSPEND);


	// hskang gps power onoff  
	/* should have some better way..  */
	#if 0
	if(unlikely(gps_power_reset == 0))
	{
		printk(" gps reset in touch\n");
		gps_power_ctl(0);
		msleep(100);
		gps_power_ctl(1);
		gps_power_reset = 1;
	}
	#endif
		
}
static void mms_ts_late_resume(struct early_suspend *h)
{
	struct mms_ts_data *ts;
	ts = container_of(h, struct mms_ts_data, early_suspend);
	mms_ts_resume(ts->client);
}
#endif
static const struct i2c_device_id mms_ts_id[] = {
	{ "mms_ts", 0 },
	{ }
};
static struct i2c_driver mms_ts_driver = {
    .driver = {
    .name = "mms_ts",
    },
    .id_table = mms_ts_id,
    .probe = mms_ts_probe,
    .remove = __devexit_p(mms_ts_remove),
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend		= mms_ts_suspend,
	.resume		= mms_ts_resume,
#endif
};
#ifdef CONFIG_BATTERY_SEC
extern unsigned int is_lpcharging_state(void);
#endif
static int __devinit mms_ts_init(void)
{
#ifdef CONFIG_BATTERY_SEC
	if (is_lpcharging_state()) {
		pr_info("%s : LPM Charging Mode! return 0\n", __func__);
		return 0;
	}
#endif
	FW_VERSION = 0x19;
	return i2c_add_driver(&mms_ts_driver);
}
static void __exit mms_ts_exit(void)
{
	i2c_del_driver(&mms_ts_driver);
}
#if 0
#if defined(SEC_TSP) && !defined(CONFIG_SEC_DEBUG)
struct class *sec_class;

static int __init rhea_class_create(void)
{
        sec_class = class_create(THIS_MODULE, "sec");
        if (IS_ERR(sec_class)) {
                pr_err("Failed to create class(sec)!\n");
                return PTR_ERR(sec_class);
        }

        return 0;
}

subsys_initcall(rhea_class_create);
#endif
#endif

MODULE_DESCRIPTION("Driver for Melfas MTSI Touchscreen Controller");
MODULE_AUTHOR("MinSang, Kim <kimms@melfas.com>");
MODULE_VERSION("0.1");
MODULE_LICENSE("GPL");
module_init(mms_ts_init);
module_exit(mms_ts_exit);
