/* drivers/video/sc8800g/sc8800g_lcd_hx8357_openphone.c
 *
 * Support for HX8357 LCD device
 *
 * Copyright (C) 2010 Spreadtrum
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
#include "lcdpanel.h"

static int32_t hx8357_init(struct panel_spec *self)
{
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	send_cmd_data_t send_cmd_data = self->info.mcu->ops->send_cmd_data;
	send_data_t send_data = self->info.mcu->ops->send_data;

	pr_debug("hx8357_init\n");

	send_cmd_data(0x00ff, 0x00);  //Command page 0
	LCD_DelayMS(15);  //delay
	send_cmd_data(0x001a, 0x0004);  //VGH VGL VCL DDVDH
	send_cmd_data(0x001b, 0x001c);

	/*power setting*/
	send_cmd_data(0x0023, 0x0091);// set VCOM offset,VMF=0x52
	send_cmd_data(0x0024, 0x0069);//set VCOMH voltage,VHH=0x64
	send_cmd_data(0x0025, 0x0063); //set VCOML voltage,VML=0x71
	send_cmd_data(0x0019, 0x0001);
	LCD_DelayMS(10);  //delay
	send_cmd_data(0x001a, 0x0001); //VGH VGL VCL DDVDH
	send_cmd_data(0x001f, 0x008a);
	send_cmd_data(0x0001, 0x0000);
	send_cmd_data(0x001c, 0x0005);
	send_cmd_data(0x001f, 0x0082);
	LCD_DelayMS(10); //delay
	send_cmd_data(0x001f, 0x0092);
	LCD_DelayMS(10); //delay
	send_cmd_data(0x001f, 0x00d4);

	/* set GRAM area 320*480 */
	send_cmd_data(0x0002, 0x0000);
	send_cmd_data(0x0003, 0x0000);
	send_cmd_data(0x0004, 0x0001);
	send_cmd_data(0x0005, 0x003f);
	send_cmd_data(0x0006, 0x0000);
	send_cmd_data(0x0007, 0x0000);
	send_cmd_data(0x0008, 0x0001);

	send_cmd_data(0x0016, 0x0009); //set my mx mv bgr
	send_cmd_data(0x0017, 0x0005); //16-bit pixel
	send_cmd_data(0x0018, 0x0022); //Fosc=130%*5.2MHZ 21
	send_cmd_data(0x001d, 0x0000); //FS0[1:0]=01,set the operating frequency of the step-up circuit 1
	send_cmd_data(0x001e, 0x0000);

	send_cmd_data(0x0026, 0x0033);
	send_cmd_data(0x0027, 0x0001);
	send_cmd_data(0x0029, 0x0000);
	send_cmd_data(0x002a, 0x0000);
	send_cmd_data(0x002b, 0x0001); //0A
	send_cmd_data(0x002c, 0x000a);
	send_cmd_data(0x002d, 0x0020);
	send_cmd_data(0x002e, 0x00a3);
	send_cmd_data(0x002f, 0x0000);
	send_cmd_data(0x0031, 0x0000);
	send_cmd_data(0x0032, 0x0000);
	send_cmd_data(0x0033, 0x0008);
	send_cmd_data(0x0034, 0x0002);
	send_cmd_data(0x0036, 0x0002);  //REV

	/* Gamma  */
	send_cmd_data(0x0040, 0x0001);
	send_cmd_data(0x0041, 0x0012);
	send_cmd_data(0x0042, 0x0019);
	send_cmd_data(0x0043, 0x0026);
	send_cmd_data(0x0044, 0x002c);
	send_cmd_data(0x0045, 0x003c);
	send_cmd_data(0x0046, 0x000b);
	send_cmd_data(0x0047, 0x005f);
	send_cmd_data(0x0048, 0x0000);
	send_cmd_data(0x0049, 0x0008);
	send_cmd_data(0x004a, 0x000b);
	send_cmd_data(0x004b, 0x0010);
	send_cmd_data(0x004c, 0x0016);

	send_cmd_data(0x0050, 0x0001);
	send_cmd_data(0x0051, 0x001d);
	send_cmd_data(0x0052, 0x0021);
	send_cmd_data(0x0053, 0x0034);
	send_cmd_data(0x0054, 0x0037);
	send_cmd_data(0x0055, 0x003f);
	send_cmd_data(0x0056, 0x0029);
	send_cmd_data(0x0057, 0x007f);
	send_cmd_data(0x0058, 0x0001);
	send_cmd_data(0x0059, 0x0012);
	send_cmd_data(0x005a, 0x001b);
	send_cmd_data(0x005b, 0x001e);
	send_cmd_data(0x005c, 0x001a);
	send_cmd_data(0x005d, 0x0055);

	send_cmd_data(0x0060, 0x0008);
	send_cmd_data(0x00f2, 0x0000);
	send_cmd_data(0x00e4, 0x001f); //EQVCI_M1=0x00
	send_cmd_data(0x00e5, 0x001f); //EQGND_M1=0x1c
	send_cmd_data(0x00e6, 0x0020); //EQVCI_M0=0x1c
	send_cmd_data(0x00e7, 0x0000); //EQGND_M0=0x1c
	send_cmd_data(0x00e8, 0x00d1);
	send_cmd_data(0x00e9, 0x00c0);
	send_cmd_data(0x0028, 0x0038);
	LCD_DelayMS(80);  //delay
	send_cmd_data(0x0028, 0x003c); //GON=0,DTE=0,D[1:0]=01

	send_cmd_data(0x0080, 0x0000);
	send_cmd_data(0x0081, 0x0000);
	send_cmd_data(0x0082, 0x0000);
	send_cmd_data(0x0083, 0x0000);

	send_cmd(0x0022);

	if (0) {
		int i;
		for (i=0; i<320*480/2; i++)
			send_data(0x1234);
		for (i=0; i< 320*480/2; i++)
			send_data(0x4321);
	}

	return 0;
}

static int32_t hx8357_set_window(struct panel_spec *self,
		uint16_t left, uint16_t top, uint16_t right, uint16_t bottom)
{
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	send_cmd_data_t send_cmd_data = self->info.mcu->ops->send_cmd_data;
	send_data_t send_data = self->info.mcu->ops->send_data;

	pr_debug("hx8357_set_window: %d, %d, %d, %d\n",left, top, right, bottom);

	/* set window size  */
	send_cmd_data(0x0002, (left  >> 8));
	send_cmd_data(0x0003, (left  & 0xff));
	send_cmd_data(0x0004, (right >> 8));
	send_cmd_data(0x0005, (right & 0xff));

	send_cmd_data(0x0006, (top    >> 8));
	send_cmd_data(0x0007, (top    &  0xff));
	send_cmd_data(0x0008, (bottom >> 8));
	send_cmd_data(0x0009, (bottom &  0xff));

	switch (self->direction) {
	case LCD_DIRECT_NORMAL:
	default:
		send_cmd_data(0x0080, (left >> 8));
		send_cmd_data(0x0081, (left &  0xff));
		send_cmd_data(0x0082, (top  >> 8));
		send_cmd_data(0x0083, (top  &  0xff));
		break;

	case LCD_DIRECT_ROT_90:
		send_cmd_data(0x0080, (top >> 8));
		send_cmd_data(0x0081, (top & 0xff));
		send_cmd_data(0x0082, (left >> 8));
		send_cmd_data(0x0083, (left & 0xff));
	    break;

	case LCD_DIRECT_ROT_180:
	case LCD_DIRECT_MIR_HV:
		send_cmd_data(0x0080, (right >> 8));
		send_cmd_data(0x0081, (right & 0xff));
		send_cmd_data(0x0082, (bottom >> 8));
		send_cmd_data(0x0083, (bottom & 0xff));
		break;

	case LCD_DIRECT_ROT_270:
		send_cmd_data(0x0080, (bottom >> 8));
		send_cmd_data(0x0081, (bottom & 0xff));
		send_cmd_data(0x0082, (left >> 8));
		send_cmd_data(0x0083, (left & 0xff));
	    break;

	case LCD_DIRECT_MIR_H:
		send_cmd_data(0x0080, (left >> 8));
		send_cmd_data(0x0081, (left & 0xff));
		send_cmd_data(0x0082, (bottom >> 8));
		send_cmd_data(0x0083, (bottom & 0xff));
		break;

	case LCD_DIRECT_MIR_V:
		send_cmd_data(0x0080, (right >> 8));
		send_cmd_data(0x0081, (right & 0xff));
		send_cmd_data(0x0082, (top >> 8));
		send_cmd_data(0x0083, (top & 0xff));
	    break;
	}

	send_cmd(0x0022);

	return 0;
}

static int32_t hx8357_invalidate(struct panel_spec *self)
{
	pr_debug("hx8357_invalidate\n");

	return self->ops->panel_set_window(self, 0, 0,
			self->width - 1, self->height - 1);

}

static int32_t hx8357_invalidate_rect(struct panel_spec *self,
				uint16_t left, uint16_t top,
				uint16_t right, uint16_t bottom)
{
	send_cmd_data_t send_cmd_data = self->info.mcu->ops->send_cmd_data;

	pr_debug("hx8357_invalidate_rect \n");

	// TE scaneline
	send_cmd_data(0x000b, (top >> 8));
	send_cmd_data(0x000c, (top & 0xff));
	return self->ops->panel_set_window(self, left, top,
			right, bottom);
}


static int32_t hx8357_set_direction(struct panel_spec *self, uint16_t direction)
{
	send_cmd_data_t send_cmd_data = self->info.mcu->ops->send_cmd_data;

	pr_debug("hx8357_set_direction\n");

	switch (direction) {
	case LCD_DIRECT_NORMAL:
		send_cmd_data(0x0016, 0x0009);
		break;
	case LCD_DIRECT_ROT_90:
		send_cmd_data(0x0016, 0x0069);
		break;
	case LCD_DIRECT_ROT_180:
		send_cmd_data(0x0016, 0x00c9);
		break;
	case LCD_DIRECT_ROT_270:
		send_cmd_data(0x0016, 0x00a9);
		break;
	case LCD_DIRECT_MIR_H:
		send_cmd_data(0x0016, 0x00c8);
		break;
	case LCD_DIRECT_MIR_V:
		send_cmd_data(0x0016, 0x0008);
		break;
	case LCD_DIRECT_MIR_HV:
		send_cmd_data(0x0016, 0x0048);
		break;
	default:
		pr_debug("unknown lcd direction!\n");
		send_cmd_data(0x0016, 0x0009);
		direction = LCD_DIRECT_NORMAL;
		break;
	}

	self->direction = direction;

	return 0;
}

static int32_t hx8357_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	send_cmd_data_t send_cmd_data = self->info.mcu->ops->send_cmd_data;

	if(is_sleep) {
		send_cmd_data(0x00FF, 0x00);//Select Command Page 0
		// Display off Setting
		send_cmd_data(0x0028, 0x38); // GON=1, DTE=1, D[1:0]=10
		LCD_DelayMS(40);  //delay
		send_cmd_data(0x0028, 0x04); // GON=0, DTE=0, D[1:0]=01
		// Power off Setting
		send_cmd_data(0x001F, 0x90); // Stop VCOMG
		// GAS_EN=1, VCOMG=0, PON=1, DK=0, XDK=0, DDVDH_TRI=0, STB=0
		LCD_DelayMS(5);  //delay
		send_cmd_data(0x001F, 0x88);// Stop step-up circuit
		// GAS_EN=1, VCOMG=1, PON=0, DK=1, XDK=1, DDVDH_TRI=0, STB=0
		send_cmd_data(0x001C, 0x00);// AP=000
		send_cmd_data(0x001F, 0x89);// Enter Standby mode
		//GAS_EN=1, VCOMG=0, PON=0, DK=1, XDK=0, DDVDH_TRI=0, STB=1
		send_cmd_data(0x0019, 0x00);//OSC_EN=0, Stop to Oscillate
	}
	else {
		send_cmd_data(0x00FF, 0x00);//Select Command Page 0
		send_cmd_data(0x0019, 0x01);//OSC_EN=1, Start to Oscillate
		LCD_DelayMS(5);  //delay
		send_cmd_data(0x001F, 0x88);
		//GAS_EN=1, VCOMG=0, PON=0, DK=1, XDK=0, DDVDH_TRI=0, STB=0
		// Power on Setting
		send_cmd_data(0x001C, 0x03);// AP=011
		send_cmd_data(0x001F, 0x80);// Exit standby mode and Step-up circuit 1 enable
		// GAS_EN=1, VCOMG=0, PON=0, DK=0, XDK=0, DDVDH_TRI=0, STB=0
		LCD_DelayMS(5);  //delay
		send_cmd_data(0x001F, 0x90);// Step-up circuit 2 enable
		// GAS_EN=1, VCOMG=0, PON=1, DK=0, XDK=0, DDVDH_TRI=0, STB=0
		LCD_DelayMS(5);  //delay
		send_cmd_data(0x001F, 0xD4);
		// GAS_EN=1, VCOMG=1, PON=1, DK=0, XDK=1, DDVDH_TRI=0, STB=0
		LCD_DelayMS(5);  //delay
		// Display on Setting
		send_cmd_data(0x0028, 0x08);// GON=0, DTE=0, D[1:0]=01
		LCD_DelayMS(40);  //delay
		send_cmd_data(0x0028, 0x38);// GON=1, DTE=1, D[1:0]=10
		LCD_DelayMS(40);  //delay
		send_cmd_data(0x0028, 0x3C);// GON=1, DTE=1, D[1:0]=11
	}
	return 0;
}

static struct panel_operations lcd_hx8357_operations = {
	.panel_init = hx8357_init,
	.panel_set_window = hx8357_set_window,
	.panel_invalidate_rect= hx8357_invalidate_rect,
	.panel_invalidate = hx8357_invalidate,
	.panel_set_direction = hx8357_set_direction,
	.panel_enter_sleep = hx8357_enter_sleep,
};

static struct timing_mcu lcd_hx8357_timing[] = {
[LCD_REGISTER_TIMING] = {                    // read/write register timing
		.rcss = 15,  // 15ns
		.rlpw = 60,
		.rhpw = 60,
		.wcss = 10,
		.wlpw = 35,
		.whpw = 35,
	},
[LCD_GRAM_TIMING] = {                    // read/write gram timing
		.rcss = 15,  // 15ns
		.rlpw = 60,
		.rhpw = 60,
		.wcss = 10,
		.wlpw = 35,
		.whpw = 35,
	},
};

static struct info_mcu lcd_hx8357_info = {
	.bus_mode = LCD_BUS_8080,
	.bus_width = 16,
	.timing = lcd_hx8357_timing,
	.ops = NULL,
};

struct panel_spec lcd_hx8357_spec = {
	.width = 320,
	.height = 480,
	.mode = LCD_MODE_MCU,
	.direction = LCD_DIRECT_NORMAL,
	.info = {.mcu = &lcd_hx8357_info},
	.ops = &lcd_hx8357_operations,
};

struct panel_cfg lcd_hx8357 = {
	/* this panel may on both CS0/1 */
	.lcd_cs = -1,
	.lcd_id = 0x57,
	.lcd_name = "lcd_hx8357",
	.panel = &lcd_hx8357_spec,
};

static int __init lcd_hx8357_init(void)
{
	return sprd_register_panel(&lcd_hx8357);
}

subsys_initcall(lcd_hx8357_init);
