/* drivers/video/sc8800g/sc8800g_lcd_hx8357.c
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
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <mach/regulator.h>
#include "lcdpanel.h"
#include <linux/init.h>
#include <linux/string.h>

//#define  LCD_DEBUG
#ifdef LCD_DEBUG
#define LCD_PRINT printk
#else
#define LCD_PRINT(...)
#endif

#define  LCD_PANEL_ID_HX8357			(0x8357|0x00)

uint32_t g_bootmode = 0;
uint32_t g_HwVersion = 0;

static int32_t hx8357_init(struct panel_spec *self)
{
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	#if 0
	send_cmd_data_t send_cmd_data = self->info.mcu->ops->send_cmd_data;
	#endif
	send_data_t send_data = self->info.mcu->ops->send_data;

	LCD_PRINT("hx8357_init\n");
      //mdelay(130);
	//send_cmd(0x11);        // Sleep out
	//mdelay(150);
	send_cmd(0x21);        // Display Inversion ON
	send_cmd(0x3A);
	send_data(0x06);       // Interface
	send_cmd(0xB9);
	send_data(0xFF);
	send_data(0x83);
	send_data(0x57);       // Set EXTC
	mdelay(5);
	send_cmd(0xCC);
	send_data(0x07);       // Set panel

#ifndef CONFIG_FB_LCD_NOFMARK
      send_cmd(0x35);  // TE on
      send_data(0x00);  //TE Mode 0
#endif

	//send_cmd_data(0x36, 0x00); //tong test
	//send_cmd_data(0x36, 0xc0); //tong test
	send_cmd(0x36);
       send_data(0x00);

	send_cmd(0xB3);
	//send_data(0x40);
        send_data(0x00); //tong test

	send_data(0x00);
	send_data(0x06);
	send_data(0x06);       // Set RGB I/F 
	send_cmd(0xB6);
	send_data(0x4D);       // VCOMDC
	send_cmd(0xB1);
	send_data(0x00);
	send_data(0x12);
	send_data(0x19);
	send_data(0x1A);
	send_data(0x83);
	send_data(0x48);       // [2]0012); ,VG); =3DDVD); ,VGL=(-2DDVD); ) [3]VSPROUT=4.27V [4]VSNROUT=(-4.31)V
	send_cmd(0xB4);
	send_data(0x02);
	send_data(0x40);
	send_data(0x00);
	send_data(0x2A);
	send_data(0x2A);
	send_data(0x0D);
	send_data(0x40);       // [1]0000); =Column,0001); =1-dot,0002); =2-dot,0003); =4-dot  
	send_cmd(0xC0);
	send_data(0x36);
	send_data(0x30);
	send_data(0x01);
	send_data(0x3C);
	send_data(0xC8);
	send_data(0x08);          
	send_cmd(0xB5);
	send_data(0x05);
	send_data(0x05);         
      
  send_cmd(0xE0);    // Gamma
  send_data(0x00);   // P0
  send_data(0x06);   // P1
  send_data(0x0A);   // P2
  send_data(0x1F);   // P3
  send_data(0x29);   // P4
  send_data(0x3D);   //; P5
  send_data(0x4B);   //; P6
  send_data(0x53);   //; P7
  send_data(0x46);   //; P8
  send_data(0x40);   //; P9
  send_data(0x3A);   //; P10	
  send_data(0x37);   //; P11	
  send_data(0x2F);   //; P12	
  send_data(0x2A);   //; P13	
  send_data(0x28);   //; P14	
  send_data(0x07);   //; P15
 
  send_data(0x00);   //; N0	
  send_data(0x06);   //; N1	
  send_data(0x0A);   //; N2	
  send_data(0x1F);   //; N3 							
  send_data(0x29);   //; N4	
  send_data(0x3D);   //; N5	
  send_data(0x4B);   //; N6
  send_data(0x53);   //; N7
  send_data(0x46);   //; N8
  send_data(0x40);   //; N9
  send_data(0x3A);   //; N10
  send_data(0x37);   //; N11
  send_data(0x2F);   //; N12
  send_data(0x2A);   //; N13 			
  send_data(0x28);   //; N14
  send_data(0x07);   //; N15
 
  send_data(0x00);   //
  send_data(0x01); 
      
	mdelay(130);
	send_cmd(0x11);        // Sleep out
	mdelay(150);
	send_cmd(0x29);    // display on
	mdelay(5);

#if 0
	send_cmd(0x2A);
	send_data(0  >> 8);
	send_data(0  & 0xff);
	send_data(319  >> 8);
	send_data(319  & 0xff);


	send_cmd(0x2B);
	send_data(0  >> 8);
	send_data(0  & 0xff);
	send_data(479  >> 8);
	send_data(479  & 0xff);
#endif

	send_cmd(0x002C);
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
	send_data_t send_data = self->info.mcu->ops->send_data;

	LCD_PRINT("hx8357_set_window: %d, %d, %d, %d\n",left, top, right, bottom);
    
	/* set window size  */

send_cmd(0x2A);
send_data(left  >> 8);
send_data(left  & 0xff);
send_data(right  >> 8);
send_data(right  & 0xff);


send_cmd(0x2B);
send_data(top  >> 8);
send_data(top  & 0xff);
send_data(bottom  >> 8);
send_data(bottom  & 0xff);

send_cmd(0x002C);

	return 0;
}

static int32_t hx8357_invalidate(struct panel_spec *self)
{
	LCD_PRINT("hx8357_invalidate\n");

	return self->ops->panel_set_window(self, 0, 0, 
			self->width - 1, self->height - 1);
	
}

static int32_t hx8357_invalidate_rect(struct panel_spec *self,
				uint16_t left, uint16_t top,
				uint16_t right, uint16_t bottom)
{

	LCD_PRINT("hx8357_invalidate_rect \n");

	// TE scaneline
	//send_cmd_data(0x000b, (top >> 8));
	//send_cmd_data(0x000c, (top & 0xff));
	return self->ops->panel_set_window(self, left, top, 
			right, bottom);
}


static int32_t hx8357_set_direction(struct panel_spec *self, uint16_t direction)
{

	LCD_PRINT("hx8357_set_direction\n");
	self->direction = direction;
	
	return 0;
}

static int32_t hx8357_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;

	if(is_sleep) 
	{
		send_cmd(0x10); // SLEEP
	}
	else
	{
		send_cmd(0x11); // SLPOUT
	}
	return 0;
}
static uint32_t hx8357_read_id(struct panel_spec *self)
{
      send_data_t send_cmd = self->info.mcu->ops->send_cmd;
      read_data_t read_data = self->info.mcu->ops->read_data;
      send_data_t send_data = self->info.mcu->ops->send_data;
      uint32_t uID = 0;
      uint32_t uICID[5] = {0};
      uint32_t i;

      /*ZTE: added by tong.weili Éú²úÄ£Ê½Ä¬ÈÏÎªhx8357 20120724 ++*/
      printk("[tong]hx8357_read_id: g_bootmode = 0x%x\n", g_bootmode);     
      if(g_bootmode)
      {
            return LCD_PANEL_ID_HX8357;
      }
      /*ZTE: added by tong.weili Éú²úÄ£Ê½Ä¬ÈÏÎªhx8357 20120724 --*/

/*ZTE: modified by tong.weili ¸ü¸ÄLCD ¶ÁÈ¡ID ·½Ê½ 20120720 ++*/
#if 0
      send_cmd(0xB0);
      send_data(0x00);

      send_cmd(0xBF);
      for(i = 0; i < 5; i++)
      {
          uICID[i] = read_data();
          printk("[tong]hx8357_read_id: uICID[%d] = 0x%x\n", i, uICID[i]);
      }

      if((uICID[1] == 0x01) && (uICID[2] == 0x22) && (uICID[3] == 0x15) && (uICID[4] == 0x81))
      {
          printk("[tong]LCD driver IC: r61581\n");
          return -1;
      }
      else
      {
          printk("[tong]LCD driver IC: hx8357c\n");
      }
#else
      send_cmd(0xB9);
      send_data(0xFF);
      send_data(0x83);
      send_data(0x57);

      send_cmd(0xD0);
      for(i = 0; i < 2; i++)
      {
          uICID[i] = read_data();
          printk("[tong]hx8357_read_id: uICID[%d] = 0x%x\n", i, uICID[i]);
      }

      if((uICID[1] == 0x90))
      {
          printk("[tong]LCD driver IC: hx8357c\n");
      }
      else
      {
          printk("[tong]LCD driver IC: r61581\n");
          return -1;
      }
#endif
/*ZTE: modified by tong.weili ¸ü¸ÄLCD ¶ÁÈ¡ID ·½Ê½ 20120720 --*/

      send_cmd(0xDA);
      uID = read_data();
      printk("[tong]hx8357_read_id: 0x%x from addr:0xDA\n", uID);

	return (0x8357|uID);
}
static struct panel_operations lcd_hx8357_operations = {
	.panel_init = hx8357_init,
	.panel_set_window = hx8357_set_window,
	.panel_invalidate_rect= hx8357_invalidate_rect,
	.panel_invalidate = hx8357_invalidate,
	.panel_set_direction = hx8357_set_direction,
	.panel_enter_sleep = hx8357_enter_sleep,
	.panel_readid          = hx8357_read_id,
};

static struct timing_mcu lcd_hx8357_timing[] = {
[LCD_REGISTER_TIMING] = {                    // read/write register timing
		.rcss = 45,
		.rlpw = 35,
		.rhpw = 130,
		.wcss = 20,
		.wlpw = 25,
		.whpw = 25,
	},
[LCD_GRAM_TIMING] = {                    // read/write gram timing
		.rcss = 355,
		.rlpw = 345,
		.rhpw = 110,
		.wcss = 20,
		.wlpw = 25,
		.whpw = 25,
	},
};

static struct info_mcu lcd_hx8357_info = {
	.bus_mode = LCD_BUS_8080,
	.bus_width = 18,
	.timing = lcd_hx8357_timing,
	.ops = NULL,
};

struct panel_spec lcd_panel_hx8357 = {
	.width = 320,
	.height = 480,
	.fps = 84, /*ZTE: added by tong.weili ±£Ö¤CTSË¢ÐÂÂÊ²âÊÔÍ¨¹ý 20120808*/
	.mode = LCD_MODE_MCU,
	.direction = LCD_DIRECT_NORMAL,
	.info = {.mcu = &lcd_hx8357_info},
	.ops = &lcd_hx8357_operations,
};

struct panel_cfg lcd_hx8357 = {
	/* this panel may on both CS0/1 */
	.lcd_cs = -1,
	.lcd_id = LCD_PANEL_ID_HX8357,
	.lcd_name = "lcd_hx8357",
	.panel = &lcd_panel_hx8357,
};

/*ZTE: modified by tong.weili ÅÐ¶ÏÉú²úÄ£Ê½ºÍÓ²¼þ°æ±¾ºÅ20120731 ++*/
static void judgeBootmode_HwVersion(void)
{
    char *ptr;
    int fd;

    ptr = boot_command_line;
    //printk("[tong]:##############judgeBootmode_HwVersion: boot_command_line=%s\n", boot_command_line);

    g_bootmode = 0;
    g_HwVersion = 2;

    while (ptr && *ptr) {
        char *x = strchr(ptr, ' ');
        if (x != 0) *x++ = 0;

		if(!strncmp(ptr,"boot_mode=1",11))
		{
		    g_bootmode = 1;
		}

            if(!strncmp(ptr,"hardware_zte=2",14))
            {
                g_HwVersion = 2;
            }
            if(!strncmp(ptr,"hardware_zte=1",14))
		{
                g_HwVersion = 1;
            }

        ptr = x;
    }
}
/*ZTE: modified by tong.weili ÅÐ¶ÏÉú²úÄ£Ê½ºÍÓ²¼þ°æ±¾ºÅ20120731 --*/

static int __init lcd_hx8357_init(void)
{
      judgeBootmode_HwVersion();
      printk("[tong]:##############lcd_hx8357_init: g_bootmode=%d\n", g_bootmode);
      printk("[tong]:##############lcd_hx8357_init: g_HwVersion=%d\n", g_HwVersion);

      return sprd_register_panel(&lcd_hx8357);
}

subsys_initcall(lcd_hx8357_init);



