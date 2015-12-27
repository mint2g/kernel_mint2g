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

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/switch.h>
#include <linux/input.h>
#include <mach/gpio.h>
#include <linux/headset.h>
#include <mach/board.h>
#include <linux/regulator/consumer.h>
#include <mach/regulator.h>
#include <mach/adc.h>

#ifdef CONFIG_MACH_MINT
#define HEADSET_BUTTON_GPIO_ACTIVE_LOW 1
#endif

#define CONFIG_METHOD_HP_DETECT_WORK_QUEUE 1

#ifndef HEADSET_DETECT_GPIO
#define HEADSET_DETECT_GPIO 165
#endif
#ifndef HEADSET_BUTTON_GPIO
#define HEADSET_BUTTON_GPIO 164
#endif

#ifndef HEADSET_DETECT_GPIO_ACTIVE_LOW
#define HEADSET_DETECT_GPIO_ACTIVE_LOW 1
#endif
#ifndef HEADSET_BUTTON_GPIO_ACTIVE_LOW
#define HEADSET_BUTTON_GPIO_ACTIVE_LOW 0
#endif

#ifndef HEADSET_DETECT_GPIO_DEBOUNCE_SW
#define HEADSET_DETECT_GPIO_DEBOUNCE_SW 300
#endif
#ifndef HEADSET_BUTTON_GPIO_DEBOUNCE_SW
#define HEADSET_BUTTON_GPIO_DEBOUNCE_SW 100
#endif


#define HEADSET_DETECT_ADC_VALUE 821

/*+ BUTTON STATUS +*/
#define RELEASE 0
#define PUSH 1

///////////////////////////////////////////////////////////////////
//jinwon.baek 120808 for earloopback switch checking 
int ret_estate = KEY_RESERVED;
unsigned int code; 
/*- BUTTON STATUS -*/

#if CONFIG_METHOD_HP_DETECT_WORK_QUEUE
static struct _headset_detect_queue_priv {
	int active;
	struct _headset_gpio *hgp;
} headset_detect_queue_priv;
static struct work_struct headset_detect_queue;
#endif

static enum hrtimer_restart report_headset_button_status(int active, struct _headset_gpio *hgp);
static enum hrtimer_restart report_headset_detect_status(int active, struct _headset_gpio *hgp);
static struct _headset headset = {
	.sdev = {
		.name = "h2w",
	},
	.detect = {
		.desc = "headset detect",
		.active_low = HEADSET_DETECT_GPIO_ACTIVE_LOW,
		.gpio = HEADSET_DETECT_GPIO,
		.debounce = 0,
		.debounce_sw = HEADSET_DETECT_GPIO_DEBOUNCE_SW,
		.irq_enabled = 1,
		.callback = report_headset_detect_status,
	},
	.button = {
		.desc = "headset button",
		.active_low = HEADSET_BUTTON_GPIO_ACTIVE_LOW,
		.gpio = HEADSET_BUTTON_GPIO,
		.debounce = 0,
		.debounce_sw = HEADSET_BUTTON_GPIO_DEBOUNCE_SW,
		.irq_enabled = 1,
		.callback = report_headset_button_status,
		//.timeout_ms = 800, /* 800ms for long button down */
	},
};

#ifndef headset_gpio_init
#define headset_gpio_init(gpio, desc) \
	do { \
		gpio_request(gpio, desc); \
		gpio_direction_input(gpio); \
	} while (0)
#endif

#ifndef headset_gpio_free
#define headset_gpio_free(gpio) \
	gpio_free(gpio)
#endif

#ifndef headset_gpio2irq_free
#define headset_gpio2irq_free(irq, args) { }
#endif

#ifndef headset_gpio2irq
#define headset_gpio2irq(gpio) \
	gpio_to_irq(gpio)
#endif

#ifndef headset_gpio_set_irq_type
#define headset_gpio_set_irq_type(irq, type) \
	irq_set_irq_type(irq, type)
#endif

#ifndef headset_gpio_get_value
#define headset_gpio_get_value(gpio) \
	gpio_get_value(gpio)
#endif

#ifndef headset_gpio_debounce
#define headset_gpio_debounce(gpio, ms) \
	gpio_set_debounce(gpio, ms)
#endif

#ifndef headset_hook_detect
#define headset_hook_detect(status) { }
#endif

#define HEADSET_DEBOUNCE_ROUND_UP(dw) \
	dw = (((dw ? dw : 1) + HEADSET_GPIO_DEBOUNCE_SW_SAMPLE_PERIOD - 1) / \
		HEADSET_GPIO_DEBOUNCE_SW_SAMPLE_PERIOD) * HEADSET_GPIO_DEBOUNCE_SW_SAMPLE_PERIOD;

static struct _headset_keycap headset_key_capability[20] = {
	{ EV_KEY, KEY_MEDIA },
	{ EV_KEY, KEY_VOLUMEUP },
	{ EV_KEY, KEY_VOLUMEDOWN },
};

static unsigned int (*headset_get_button_code_board_method)(int v);
static unsigned int (*headset_map_code2push_code_board_method)(unsigned int code, int push_type);
static __devinit int headset_button_probe(struct platform_device *pdev)
{
	struct _headset_button *headset_button = platform_get_drvdata(pdev);
	headset_get_button_code_board_method = headset_button->headset_get_button_code_board_method;
	headset_map_code2push_code_board_method = headset_button->headset_map_code2push_code_board_method;
	memcpy(headset_key_capability, headset_button->cap, sizeof headset_button->cap);
	return 0;
}

static struct platform_driver headset_button_driver = {
	.driver = {
		.name = "headset-button",
		.owner = THIS_MODULE,
	},
	.probe = headset_button_probe,
};

static unsigned int headset_get_button_code(int v)
{
	unsigned int code;

	pr_info("headset_get_button_code headset_get_button_code_board_method [%d] \n",headset_get_button_code_board_method);
											
	if (headset_get_button_code_board_method)
		code = headset_get_button_code_board_method(v);
	else
		code = KEY_MEDIA;
	return code;
}

static unsigned int headset_map_code2key_type(unsigned int code)
{
	unsigned int key_type = EV_KEY;
	int i;

	pr_info("headset_map_code2key_type [%d] \n",code);
	
	for(i = 0; headset_key_capability[i].key != KEY_RESERVED &&
		headset_key_capability[i].key != code && i < ARRY_SIZE(headset_key_capability); i++);
	if (i < ARRY_SIZE(headset_key_capability) &&
		headset_key_capability[i].key == code)
		key_type = headset_key_capability[i].type;
	else
		pr_err("headset not find code [0x%x]'s maping type\n", code);

	return key_type;
}

static void headset_gpio_irq_enable(int enable, struct _headset_gpio *hgp);
#define HEADSET_GPIO_DEBOUNCE_SW_SAMPLE_PERIOD	50 /* 10 */



static ssize_t estate_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", ret_estate);	
}
static DEVICE_ATTR(estate, S_IRUGO | S_IWUSR, estate_show, NULL);
//////////////////////////////////////////////////////////////////////
static enum hrtimer_restart report_headset_button_status(int active, struct _headset_gpio *hgp)
{
	enum hrtimer_restart restart;
	unsigned int key_type  = KEY_RESERVED;

	if (!hgp->parent->detect.active) {
		active = -1;
		pr_warn("%s --> mismatch, detect has been pluged out, force button active to -1\n", __func__);
	}

	pr_info("report_headset_button_status-Active [%d] \n",active);

	ret_estate = active;//jinwon.baek 120808 for earloopback switch checking 
	
	if (active < 0) {
		code = KEY_RESERVED;
		ret_estate = KEY_RESERVED;
		pr_info("KEY_RESERVED eport_headset_button_status-Active [%d] \n",active);	
		return HRTIMER_NORESTART;

	}
	if (active) {
		code = headset_get_button_code(0);

	} else {
		pr_info("HRTIMER_NORESTART eport_headset_button_status-Active [%d] \n",active);	
	}

 	{
		key_type = headset_map_code2key_type(code);
		switch (key_type) {
		case EV_KEY:
			pr_info("headset button-ret_estate %d\n",ret_estate);
			if(ret_estate == PUSH)
			{
			input_event(hgp->parent->input, key_type, code, 1);
				pr_info("[%s] ret_estate : %d BUTTON PUSH\n", __func__, ret_estate);
		input_sync(hgp->parent->input);
			}else{
			input_event(hgp->parent->input, key_type, code, 0);
				pr_info("[%s] ret_estate : %d, BUTTON RELEASE\n", __func__, ret_estate);
				ret_estate = KEY_RESERVED;
		input_sync(hgp->parent->input);
			}
			pr_info("headset button-%d[%dms]\n", key_type, hgp->holded);
			break;
		default:
			pr_err("headset not support key type [%d]\n", key_type);
		}
	}
		return HRTIMER_NORESTART;
}

static int report_headset_detect_status_callback(int active, struct _headset_gpio *hgp)
{
	struct regulator *mic_regulator;
	mic_regulator = regulator_get(NULL, REGU_NAME_MIC);
	int adc_value, temp;
	int check= 0;

	if (active) {
		if (!IS_ERR(mic_regulator)) {
			regulator_set_voltage(mic_regulator, 2800000, 2800000);
			regulator_enable(mic_regulator);
		}
		msleep(150);
		headset_hook_detect(1);
		hgp->parent->headphone = 0;
		/* hgp->parent->headphone = hgp->parent->button.active_low ^ headset_gpio_get_value(hgp->parent->button.gpio); */
		temp = sci_adc_get_value(ADC_CHANNEL_TEMP, true);
		adc_value = ((2800 * (temp)) / 0x3FC);
              pr_info("[%s]temp : %d, adc_value_pole : %d\n",__func__,temp,  adc_value);
		hgp->parent->headphone = adc_value > HEADSET_DETECT_ADC_VALUE? 0 : 1;

		if (hgp->parent->headphone) {
			switch_set_state(&hgp->parent->sdev, BIT_HEADSET_NO_MIC);
			if (!IS_ERR(mic_regulator))
				regulator_disable(mic_regulator);
			pr_info("headphone plug in\n");
		} else {
			switch_set_state(&hgp->parent->sdev, BIT_HEADSET_MIC);
			pr_info("headset plug in\n");
			headset_gpio_set_irq_type(hgp->parent->button.irq, hgp->parent->button.irq_type_active);
			headset_gpio_irq_enable(1, &hgp->parent->button);
		}
	} else {
		int regulator_is_enabled = hgp->parent->button.irq_enabled;
		pr_info("[%s] check : %d\n",__func__, check++);
		headset_gpio_irq_enable(0, &hgp->parent->button);
		hgp->parent->button.callback(-1, &hgp->parent->button);
		headset_hook_detect(0);
		if (hgp->parent->headphone)
			pr_info("headphone plug out\n");
		else {
			if (!IS_ERR(mic_regulator) && regulator_is_enabled)
				regulator_disable(mic_regulator);
			pr_info("headset plug out\n");
		}
		switch_set_state(&hgp->parent->sdev, BIT_HEADSET_OUT);
                pr_info("[%s] switch_set_state", __func__);
	}
	/* use below code only when gpio irq misses state ? */
	/* headset_gpio_set_irq_type(hgp->irq, active ? hgp->irq_type_inactive : hgp->irq_type_active); */
	pr_info("[%s] check2 : %d\n",__func__, check++);
	return 0;
}

static enum hrtimer_restart report_headset_detect_status(int active, struct _headset_gpio *hgp)
{
#if CONFIG_METHOD_HP_DETECT_WORK_QUEUE
	headset_detect_queue_priv.active = active;
	headset_detect_queue_priv.hgp = hgp;
	schedule_work(&headset_detect_queue);
#else
	if (report_headset_detect_status_callback(active, hgp))
		pr_info("report_headset_detect_status_callback return error\n");
#endif
	return HRTIMER_NORESTART;
}

#if CONFIG_METHOD_HP_DETECT_WORK_QUEUE
static void handle_headset_detect_queue(struct work_struct *work)
{
	pr_info("[%s]\n", __func__);
	if (report_headset_detect_status_callback(headset_detect_queue_priv.active,
				headset_detect_queue_priv.hgp))
		pr_info("report_headset_detect_status_callback return error\n");
}
#endif

static enum hrtimer_restart headset_gpio_timer_func(struct hrtimer *timer)
{
	enum hrtimer_restart restart = HRTIMER_RESTART;
	struct _headset_gpio *hgp =
		container_of(timer, struct _headset_gpio, timer);
	int active = hgp->active_low ^ headset_gpio_get_value(hgp->gpio); /* hgp->active */
	int green_ch = (!active && &hgp->parent->detect == hgp);
	if (active != hgp->active) {
		pr_info("The value %s mismatch [%d:%d] at %dms!\n",
				hgp->desc, active, hgp->active, hgp->holded);
		hgp->holded = 0;
	}
	pr_info("%s : %s %s green_ch[%d], holed=%d, debounce_sw=%d\n", __func__,
			hgp->desc, active ? "active" : "inactive", green_ch, hgp->holded, hgp->debounce_sw);
	hgp->holded += HEADSET_GPIO_DEBOUNCE_SW_SAMPLE_PERIOD;
	if (hgp->holded >= hgp->debounce_sw || green_ch) {
		if (hgp->holded == hgp->debounce_sw || \
			hgp->holded == hgp->timeout_ms || \
			green_ch) {
			pr_info("call headset gpio handler\n");
			restart = hgp->callback(active, hgp);
		} else
			pr_info("gpio <%d> has kept active for %d ms\n", hgp->gpio, hgp->holded);
	}
	if (restart == HRTIMER_RESTART)
		hrtimer_forward_now(timer,
			ktime_set(HEADSET_GPIO_DEBOUNCE_SW_SAMPLE_PERIOD / 1000,
					(HEADSET_GPIO_DEBOUNCE_SW_SAMPLE_PERIOD % 1000) * 1000000)); /* repeat timer */
	return restart;
}

static irqreturn_t headset_gpio_irq_handler(int irq, void *dev)
{
	struct _headset_gpio *hgp = dev;
        pr_info("%s start\n", __func__);
	hrtimer_cancel(&hgp->timer);
	hgp->active = hgp->active_low ^ headset_gpio_get_value(hgp->gpio);
	pr_info("hgp->activ :%d, hgp->active_low :  %d, headset_gpio_get_value(hgp->gpio) : %d\n", hgp->active, hgp->active_low , headset_gpio_get_value(hgp->gpio));
	headset_gpio_set_irq_type(hgp->irq, hgp->active ? hgp->irq_type_inactive : hgp->irq_type_active);
	pr_info("%s : %s %s\n", __func__, hgp->desc, hgp->active ? "active" : "inactive");
	hgp->holded = 0;
	hrtimer_start(&hgp->timer,
			ktime_set(HEADSET_GPIO_DEBOUNCE_SW_SAMPLE_PERIOD / 1000,
				(HEADSET_GPIO_DEBOUNCE_SW_SAMPLE_PERIOD % 1000) * 1000000),
			HRTIMER_MODE_REL);
	return IRQ_HANDLED;
}

static void headset_gpio_irq_enable(int enable, struct _headset_gpio *hgp)
{
	int action = 0;
	pr_info("%s start\n", __func__);
	if (enable) {
		if (!hgp->irq_enabled) {
			hrtimer_cancel(&hgp->timer);
			hgp->irq_enabled = 1;
			action = 1;
			hgp->holded = 0;
			enable_irq(hgp->irq);
			pr_info("%s enable IRQ : %s\n", __func__, hgp->desc);
		}
	} else {
		if (hgp->irq_enabled) {
			disable_irq(hgp->irq);
			hrtimer_cancel(&hgp->timer);
			hgp->irq_enabled = 0;
			action = 1;
			hgp->holded = 0;
			pr_info("%s disable IRQ : %s\n", __func__, hgp->desc);
		}
	}
	pr_info("%s [ irq=%d ] --- %saction %s\n", __func__, hgp->irq_enabled, action ? "do " : "no ", hgp->desc);
}

static int __init headset_init(void)
{
	int ret, i;
	struct _headset *ht = &headset;
	pr_info("%s start\n", __func__);
	ret = switch_dev_register(&ht->sdev);
	if (ret < 0) {
		pr_err("switch_dev_register failed!\n");
		return ret;
	}
	platform_driver_register(&headset_button_driver); 
	ht->input = input_allocate_device();
	if (ht->input == NULL) {
		pr_err("switch_dev_register failed!\n");
		goto _switch_dev_register;
	}
	//jinwon.baek 120808 for earloopback switch checking 
	ret = device_create_file((&ht->sdev)->dev, &dev_attr_estate);
	if (ret < 0) {
		pr_err("eswitch_dev_register failed!\n");
		return ret;
	}

	ht->input->name = "headset-keyboard";
	ht->input->id.bustype = BUS_HOST;
	ht->input->id.vendor = 0x0001;
	ht->input->id.product = 0x0001;
	ht->input->id.version = 0x0100;
	ret_estate = KEY_RESERVED;

	for(i = 0; headset_key_capability[i].key != KEY_RESERVED; i++) {
		__set_bit(headset_key_capability[i].type, ht->input->evbit);
		input_set_capability(ht->input, headset_key_capability[i].type, headset_key_capability[i].key);
	}

	if (input_register_device(ht->input))
		goto _switch_dev_register;
#if CONFIG_METHOD_HP_DETECT_WORK_QUEUE
	/* Initialize task queue structures */
	INIT_WORK(&headset_detect_queue, handle_headset_detect_queue);
#endif

	headset_gpio_init(ht->detect.gpio, ht->detect.desc);
	headset_gpio_init(ht->button.gpio, ht->button.desc);

	headset_gpio_debounce(ht->detect.gpio, ht->detect.debounce * 1000);
	headset_gpio_debounce(ht->button.gpio, ht->button.debounce * 1000);

	hrtimer_init(&ht->button.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ht->button.timer.function = headset_gpio_timer_func;
	HEADSET_DEBOUNCE_ROUND_UP(ht->button.debounce_sw);
	HEADSET_DEBOUNCE_ROUND_UP(ht->button.timeout_ms);
	ht->button.parent = ht;
	ht->button.irq = headset_gpio2irq(ht->button.gpio);
	ht->button.irq_type_active = ht->button.active_low ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH;
	ht->button.irq_type_inactive = ht->button.active_low ? IRQF_TRIGGER_HIGH : IRQF_TRIGGER_LOW;
	ret = request_irq(ht->button.irq, headset_gpio_irq_handler,
					ht->button.irq_type_active, ht->button.desc, &ht->button);
	if (ret) {
		pr_err("request_irq gpio %d's irq failed!\n", ht->button.gpio);
		goto _gpio_request;
	}
	pr_info("%s irq_disable\n", __func__);
	headset_gpio_irq_enable(0, &ht->button);

	hrtimer_init(&ht->detect.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ht->detect.timer.function = headset_gpio_timer_func;
	HEADSET_DEBOUNCE_ROUND_UP(ht->detect.debounce_sw);
	ht->detect.parent = ht;
	ht->detect.irq = headset_gpio2irq(ht->detect.gpio);
	ht->detect.irq_type_active = ht->detect.active_low ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH;
	ht->detect.irq_type_inactive = ht->detect.active_low ? IRQF_TRIGGER_HIGH : IRQF_TRIGGER_LOW;
	ret = request_irq(ht->detect.irq, headset_gpio_irq_handler,
					ht->detect.irq_type_active, ht->detect.desc, &ht->detect);
	
	headset_gpio_irq_handler(ht->detect.irq, &(ht->detect));
	pr_info("[%s] ht->detect.irq : %d \n", __func__, ht->detect.irq);
	pr_info("%s end\n", __func__);
	if (ret) {
		pr_err("request_irq gpio %d's irq failed!\n", ht->detect.gpio);
		goto _headset_button_gpio_irq_handler;
	}
	return 0;
_headset_button_gpio_irq_handler:
	free_irq(ht->button.irq, &ht->button);
	headset_gpio2irq_free(ht->button.irq, &ht->button);
_gpio_request:
	headset_gpio_free(ht->detect.gpio);
	headset_gpio_free(ht->button.gpio);
	input_free_device(ht->input);
_switch_dev_register:
	platform_driver_unregister(&headset_button_driver);
	switch_dev_unregister(&ht->sdev);
	return ret;
}
module_init(headset_init);

static void __exit headset_exit(void)
{
	struct _headset *ht = &headset;
	headset_gpio_irq_enable(0, &ht->button);
	headset_gpio_irq_enable(0, &ht->detect);
	free_irq(ht->detect.irq, &ht->detect);
	headset_gpio2irq_free(ht->detect.irq, &ht->detect);
	free_irq(ht->button.irq, &ht->button);
	headset_gpio2irq_free(ht->button.irq, &ht->button);
	headset_gpio_free(ht->detect.gpio);
	headset_gpio_free(ht->button.gpio);
	input_free_device(ht->input);
	platform_driver_unregister(&headset_button_driver);
	switch_dev_unregister(&ht->sdev);
}
module_exit(headset_exit);

MODULE_DESCRIPTION("headset & button detect driver");
MODULE_AUTHOR("Luther Ge <luther.ge@spreadtrum.com>");
MODULE_LICENSE("GPL");
