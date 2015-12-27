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
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <mach/regulator.h>
#include "lcdpanel.h"

static int32_t ili9486_init(struct panel_spec *self)
{
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	send_data_t send_data = self->info.mcu->ops->send_data;

	send_cmd(0xF9);
	send_data(0x00);
	send_data(0x08);

	send_cmd(0xF2);
	send_data(0x18);
	send_data(0xA3);
	send_data(0x02);
	send_data(0x02);
	send_data(0xB2);
	send_data(0x12);
	send_data(0xFF);
	send_data(0x10);
	send_data(0x00);

	send_cmd(0xF1); 
	send_data(0x36);
	send_data(0x04);
	send_data(0x00);
	send_data(0x3C);
	send_data(0x0F);
	send_data(0x8F);

	send_cmd(0xC0);
	send_data(0x07);
	send_data(0x07);

	send_cmd(0xC1);
	send_data(0x45);

	send_cmd(0xC2);
	send_data(0x22);

	LCD_DelayMS(160);

	send_cmd(0x2A); 
	send_data(0x00);
	send_data(0x00);
	send_data(0x01);
	send_data(0x3F);

	send_cmd(0x2B);
	send_data(0x00);
	send_data(0x00);
	send_data(0x01);
	send_data(0xDF);

	send_cmd(0xB1);
	send_data(0xB0);
	send_data(0x11);

	send_cmd(0xB4);
	send_data(0x02);

	send_cmd(0xB5);
	send_data(0x0F);
	send_data(0x0F);
	send_data(0x10);
	send_data(0x0A);

	send_cmd(0xB6);
	send_data(0x02);
	send_data(0x22);
	send_data(0x3B);

	send_cmd(0xB7);
	send_data(0xC6);

	send_cmd(0x20);

	send_cmd(0x13);

	send_cmd(0x35);

	send_cmd(0x3A);
	send_data(0x05);

	send_cmd(0x36);
	send_data(0xD8);

	send_cmd(0xF8);
	send_data(0x21);
	send_data(0x07);

	send_cmd(0xE0);
	send_data(0x01);
	send_data(0x19);
	send_data(0x15);
	send_data(0x0B);
	send_data(0x0D);
	send_data(0x06);
	send_data(0x44);
	send_data(0x88);
	send_data(0x35);
	send_data(0x09);
	send_data(0x10);
	send_data(0x01);
	send_data(0x06);
	send_data(0x03);
	send_data(0x08);

	send_cmd(0xE1);
	send_data(0x0F);
	send_data(0x37);
	send_data(0x32);
	send_data(0x0D);
	send_data(0x0B);
	send_data(0x04);
	send_data(0x46);
	send_data(0x42);
	send_data(0x32);
	send_data(0x03);
	send_data(0x0E);
	send_data(0x05);
	send_data(0x22);
	send_data(0x22);
	send_data(0x09);

	send_cmd(0xE2);
	send_data(0x19);
	send_data(0x19);
	send_data(0x19);
	send_data(0x19);
	send_data(0x19);
	send_data(0x19);
	send_data(0x1A);
	send_data(0x1A);
	send_data(0x1A);
	send_data(0x1A);
	send_data(0x1A);
	send_data(0x1A);
	send_data(0x19);
	send_data(0x19);
	send_data(0x09);
	send_data(0x09);

	send_cmd(0xE3);
	send_data(0x04);
	send_data(0x04);
	send_data(0x04);
	send_data(0x04);
	send_data(0x04);
	send_data(0x04);
	send_data(0x04);
	send_data(0x04);
	send_data(0x04);
	send_data(0x04);
	send_data(0x24);
	send_data(0x24);
	send_data(0x24);
	send_data(0x24);
	send_data(0x24);
	send_data(0x24);
	send_data(0x24);
	send_data(0x24);
	send_data(0x24);
	send_data(0x24);
	send_data(0x24);
	send_data(0x24);
	send_data(0x24);
	send_data(0x25);
	send_data(0x3D);
	send_data(0x4D);
	send_data(0x4D);
	send_data(0x4D);
	send_data(0x4C);
	send_data(0x7C);
	send_data(0x6D);
	send_data(0x6D);
	send_data(0x7D);
	send_data(0x6D);
	send_data(0x6E);
	send_data(0x6D);
	send_data(0x6D);
	send_data(0x6D);
	send_data(0x6D);
	send_data(0x6D);
	send_data(0x5C);
	send_data(0x5C);
	send_data(0x5C);
	send_data(0x6B);
	send_data(0x6B);
	send_data(0x6A);
	send_data(0x5B);
	send_data(0x5B);
	send_data(0x53);
	send_data(0x53);
	send_data(0x53);
	send_data(0x53);
	send_data(0x53);
	send_data(0x53);
	send_data(0x43);
	send_data(0x33);
	send_data(0xB4);
	send_data(0x94);
	send_data(0x74);
	send_data(0x64);
	send_data(0x64);
	send_data(0x43);
	send_data(0x13);
	send_data(0x24);

	send_cmd(0x11);

	LCD_DelayMS(120);

	send_cmd(0x29);

	LCD_DelayMS(100);
	
#if 0	
	if (1) { //  for test the lcd
		int i;
		send_cmd(0x2C); //Write data
		for (i = 0; i < 480*320/3; i++)
			send_data(0xff);
		for (i = 0; i < 480*320/3; i++)
			send_data(0xff00);
		for (i = 0; i < 480*320/3; i++)
			send_data(0xff0000);
	}
	send_cmd(0x29); //Display On 
	LCD_DelayMS(120); //120ms
	send_cmd(0x2C); //Write data
	LCD_DelayMS(12000); //120ms
#endif	

	return 0;
}

static int32_t ili9486_set_window(struct panel_spec *self,
		uint16_t left, uint16_t top, uint16_t right, uint16_t bottom)
{
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	send_data_t send_data = self->info.mcu->ops->send_data;

	pr_debug("ili9486_set_window:%d, %d, %d, %d\n", left, top, right, bottom);

	send_cmd(0x2A);
	send_data((left >> 8));
	send_data((left & 0xFF));
	send_data((right >> 8));
	send_data((right & 0xFF));

	send_cmd(0x2B);
	send_data((top >> 8));
	send_data((top & 0xFF));
	send_data((bottom >> 8));
	send_data((bottom & 0xFF));

	send_cmd(0x2C);

	return 0;
}

static int32_t ili9486_invalidate(struct panel_spec *self)
{

	return self->ops->panel_set_window(self, 0, 0,
			self->width-1, self->height-1);

}

static int32_t ili9486_invalidate_rect(struct panel_spec *self,
				uint16_t left, uint16_t top,
				uint16_t right, uint16_t bottom)
{

	pr_debug("ili9486_invalidate_rect\n");

	return self->ops->panel_set_window(self, left, top,
			right, bottom);
}

static int32_t ili9486_set_direction(struct panel_spec *self, uint16_t direction)
{

	pr_debug("ili9486_set_direction\n");
	return 0;
}

static int32_t ili9486_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	send_data_t send_data = self->info.mcu->ops->send_data;

	if(is_sleep) {
		/*send_cmd(0x10);*/
		/* LCD_DelayMS(120);*/

		send_cmd(0x28);
		mdelay(150);

		send_cmd(0x10);
		mdelay(120);

	}
	else {
		ili9486_init(self);
		#if 0
		/*send_cmd(0x11);
		LCD_DelayMS(120);*/
		send_cmd(0x11); // (SLPOUT)
		mdelay(120); // 100ms
		send_cmd(0x29); // (DISPON)
		mdelay(100); // 100ms
		#endif
	}

	return 0;
}

static uint32_t ili9486_read_id(struct panel_spec *self)
{
	uint32_t read_value = 0;
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	read_data_t read_data = self->info.mcu->ops->read_data;
	
	send_cmd(0x04);

	read_data();
	read_value += read_data()<< 16;
	read_value += read_data()<< 8;
	read_value += read_data();

	return read_value;
}

static struct panel_operations lcd_ili9486_operations = {
	.panel_init            = ili9486_init,
	.panel_set_window      = ili9486_set_window,
	.panel_invalidate      = ili9486_invalidate,
	.panel_invalidate_rect = ili9486_invalidate_rect,
	.panel_set_direction   = ili9486_set_direction,
	.panel_enter_sleep     = ili9486_enter_sleep,
	.panel_readid          = ili9486_read_id,
};

static struct timing_mcu lcd_ili9486_timing[] = {
	[LCD_REGISTER_TIMING] = {
		.rcss = 35,
		.rlpw = 55,
		.rhpw = 100,
		.wcss = 50,
		.wlpw = 25,
		.whpw = 25,
	},
	[LCD_GRAM_TIMING] = {
		.rcss = 35,
		.rlpw = 55,
		.rhpw = 100,
		.wcss = 40,
		.wlpw = 25,
		.whpw = 25,
	}
};

static struct info_mcu lcd_ili9486_info = {
	.bus_mode  = LCD_BUS_8080,
	.bus_width = 16,
	.timing    = lcd_ili9486_timing,
	.ops       = NULL,
};

struct panel_spec lcd_ili9486_spec = {
	.width = 320,
	.height = 480,
	.mode = LCD_MODE_MCU,
	.direction = LCD_DIRECT_NORMAL,
	.info = {
		.mcu = &lcd_ili9486_info
	},
	.ops = &lcd_ili9486_operations,
};

struct panel_cfg lcd_ili9486 = {
	.lcd_cs = -1,
	.lcd_id = 0x5bbc,
	.lcd_name = "lcd_ili9486",
	.panel = &lcd_ili9486_spec,
};

static int lcd_regulator(void)
{
	int err;
	struct regulator *lcd_regulator = NULL;
	struct regulator *lcdio_regulator = NULL;

	lcd_regulator = regulator_get(NULL, REGU_NAME_LCD);
	if (IS_ERR(lcd_regulator)) {
		pr_err("ILI9486:could not get lcd regulator\n");
		return -1;
	}

	err = regulator_enable(lcd_regulator);
	if (err) {
		pr_err("ILI9486:could not enable lcd regulator\n");
		return -1;
	}
	err = regulator_set_voltage(lcd_regulator,3000000,3000000);
	if (err)
		pr_err("ILI9486:could not set lcd to 3000mv.\n");


	lcdio_regulator = regulator_get(NULL, REGU_NAME_LCDIO);
	if (IS_ERR(lcdio_regulator)) {
		pr_err("ILI9486:could not get lcdio regulator\n");
		return -1;
	}

	err = regulator_enable(lcdio_regulator);
	if (err) {
		pr_err("ILI9486:could not enable lcdio regulator\n");
		return -1;
	}

	err =regulator_set_voltage(lcdio_regulator,2800000,2800000);
	if (err)
		pr_err("ILI9486:could not set lcdio to 1800mv.\n");
	return 0;
}

static int __init lcd_ili9486_init(void)
{
	lcd_regulator();
	return sprd_register_panel(&lcd_ili9486);
}

subsys_initcall(lcd_ili9486_init);
