#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <mach/regulator.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/fs.h>

#ifdef CONFIG_WLAN_SDIO
extern void sdhci_bus_scan(void);
extern int sdhci_device_attached();
#endif
/*
 * Control BT/WIFI LDO always power on accoding to Realtek 8723as chip spec
 */
static int wifi_bt_ldo_enable(void)
{
	int err;
	struct regulator *regulator_wifi_bt = NULL;

	printk("RTL871X(adapter): %s enter\n", __func__);

	if (GPIO_WIFI_POWERON > 0) {
		gpio_request(GPIO_WIFI_POWERON, "wifi_pwron");
		gpio_direction_output(GPIO_WIFI_POWERON, 1);
	}

	regulator_wifi_bt = regulator_get(NULL, "vddcmmb1p2");
	if (IS_ERR(regulator_wifi_bt)){
		pr_err("Can't get regulator for vddcmmb1p2\n");
		return -1;
	}

	err = regulator_set_voltage(regulator_wifi_bt, 1800000, 1800000);
	if (err){
		pr_err("RTL871X(adapter): Can't set regulator to voltage 1.8V\n");
		return -1;
	}

	regulator_enable(regulator_wifi_bt);

#ifdef CONFIG_WLAN_SDIO
	regulator_wifi_bt = regulator_get(NULL, "vddsd1");
	if (IS_ERR(regulator_wifi_bt)){
		pr_err("RTL871X(adapter): Can't get regulator for VDD_SDIO\n");
		return -1;
	}

	err = regulator_set_voltage(regulator_wifi_bt, 1800000, 1800000);
	if (err){
		pr_err("RTL871X(adapter): Can't set regulator to valtage 1.8V\n");
		return -1;
	}

	regulator_enable(regulator_wifi_bt);

	if (GPIO_WIFI_RESET > 0) {
		gpio_request(GPIO_WIFI_RESET , "wifi_rst");
		gpio_direction_output(GPIO_WIFI_RESET , 1);
		msleep(5);
		sdhci_bus_scan();
		printk("RTL871X(adapter): %s after bus scan\n", __func__);
	}
#endif

	printk("RTL871X(adapter): %s exit\n", __func__);
	return 0;
}

static void wlan_bt_clk_init(void)
{
	struct clk *wlan_clk;
	struct clk *clk_parent;
	wlan_clk = clk_get(NULL, "clk_aux0");
	if (IS_ERR(wlan_clk)) {
		printk("clock: failed to get clk_aux0\n");
	}

	clk_parent = clk_get(NULL, "ext_32k");
	if (IS_ERR(clk_parent)) {
		printk("failed to get parent ext_32k\n");
	}

	clk_set_parent(wlan_clk, clk_parent);
	clk_set_rate(wlan_clk, 32000);
	clk_enable(wlan_clk);
}

static int __init wlan_bt_device_init(void)
{
	wlan_bt_clk_init();
	return wifi_bt_ldo_enable();
}

#ifdef CONFIG_WLAN_SDIO
static int __init wlan_bt_late_init(void)
{

	int i = 0;

	printk("RTL871X(adapter): %s enter\n", __func__);

	/* sdhci_device_attach in wifi driver will fail
	 * because mmc->card is NULL in first wifi on
	 * after reboot, this will cause suspend/resume
	 * fail, so we add bus_scan here before first
	 * wifi on, but we should push down rst in late_init
	 * because bus_scan is in queue work */
	if (GPIO_WIFI_RESET > 0) {
		for (i = 0; i <= 100; i++) {
			msleep(10);
			if (sdhci_device_attached())
				break;
		}
		printk("RTL871X(adapter): %s after delay %d times (10ms)\n", __func__, i);
		gpio_direction_output(GPIO_WIFI_RESET , 0);
	}
	return 0;
}

late_initcall(wlan_bt_late_init);

#endif

device_initcall(wlan_bt_device_init);

MODULE_DESCRIPTION("Realtek wlan/bt init driver");
MODULE_LICENSE("GPL");

