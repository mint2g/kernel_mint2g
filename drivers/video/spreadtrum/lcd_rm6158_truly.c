/* drivers/video/sc8800g/lcd_rm6158_truly.c
 *
 * Support for rm6158_truly LCD device
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

//#define  LCD_DEBUG
#ifdef LCD_DEBUG
#define LCD_PRINT printk
#else
#define LCD_PRINT(...)
#endif

extern uint32_t g_bootmode;

#define  LCD_PANEL_ID_RM61581_TRULY	(0x6158|0xA1)  //信利
//#define  LCD_PANEL_ID_RM61581_TRULY	(0x6158|0x00)  //信利(暂未烧录)

static int32_t rm6158_truly_init(struct panel_spec *self)
{
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	send_data_t send_data = self->info.mcu->ops->send_data;

	LCD_PRINT("rm6158_truly_init\n");
      printk("[tong]rm6158_truly_init~~~~~~~~~~~~~~~~~~~~~~~~~~\n");

      send_cmd(0x01);//soft reset
      mdelay(120);

      send_cmd(0xFF);
	send_cmd(0xFF);
	mdelay(5);
	send_cmd(0xFF);
	send_cmd(0xFF);
	send_cmd(0xFF);
	send_cmd(0xFF);
	mdelay(10);

	send_cmd(0xB0);//{setc, [107], W, 0x000B0}
	send_data(0x00);//{setp, [104], W, 0x00000}

	send_cmd(0xB3);
	send_data(0x02);
	send_data(0x00);
	send_data(0x00);
	send_data(0x00);

	send_cmd(0xB4);
	send_data(0x00);

	send_cmd(0xC0);
	send_data(0x17);
	send_data(0x3B);//480
	send_data(0x00);
	send_data(0x00);//BLV=0,PTL=0
	send_data(0x00);
	send_data(0x01);
	send_data(0x00);//
	send_data(0x77);

	send_cmd(0xC1);
	send_data(0x08);// BC=1 //div=0
	send_data(0x16);//CLOCK
	send_data(0x08);
	send_data(0x08);

	send_cmd(0xC4);
	send_data(0x11);
	send_data(0x07);
	send_data(0x03);
	send_data(0x03);

	send_cmd(0xC8);//GAMMA
	send_data(0x04);
	send_data(0x0C);
	send_data(0x0A);
	send_data(0x59);
	send_data(0x06);
	send_data(0x08);
	send_data(0x0f);
	send_data(0x07);

	send_data(0x00);
	send_data(0x32);

	send_data(0x07);
	send_data(0x0f);
	send_data(0x08);
	send_data(0x56);//43/55
	send_data(0x09);
	send_data(0x0A);
	send_data(0x0C);
	send_data(0x04);

	send_data(0x32);
	send_data(0x00);

	send_cmd(0x2A);
	send_data(0x00);
	send_data(0x00);
	send_data(0x01);
	send_data(0x3F);//320

	send_cmd(0x2B);
	send_data(0x00);
	send_data(0x00);
	send_data(0x01);
	send_data(0xDF);//480

#ifndef CONFIG_FB_LCD_NOFMARK
	send_cmd(0x35);
	send_data(0x00);
#endif

	send_cmd(0x36);
	//send_data(0x80);
	send_data(0x40);//tong test

	send_cmd(0x3A);
	send_data(0x66);

	send_cmd(0x44);
	send_data(0x00);
	send_data(0x01);

	send_cmd(0x11);
	mdelay(150);

	send_cmd(0xD0);
	send_data(0x07);
	send_data(0x07);
	send_data(0x1E); //
	send_data(0x33);


	send_cmd(0xD1);
	send_data(0x03);
	send_data(0x4B);//VCM40
	send_data(0x10);//VDV

	send_cmd(0xD2);
	send_data(0x03);
	send_data(0x04);//0X24
	send_data(0x04);

	send_cmd(0x29);
	mdelay(10);  

	send_cmd(0xB4);
	send_data(0x00);
	send_cmd(0x2C);
	mdelay(20);  
    
	return 0;
}

static int32_t rm6158_truly_set_window(struct panel_spec *self,
		uint16_t left, uint16_t top, uint16_t right, uint16_t bottom)
{
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	send_data_t send_data = self->info.mcu->ops->send_data;

	LCD_PRINT("rm6158_truly_set_window: %d, %d, %d, %d\n",left, top, right, bottom);
    
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

static int32_t rm6158_truly_invalidate(struct panel_spec *self)
{
	LCD_PRINT("rm6158_truly_invalidate\n");

	return self->ops->panel_set_window(self, 0, 0, 
			self->width - 1, self->height - 1);
	
}

static int32_t rm6158_truly_invalidate_rect(struct panel_spec *self,
				uint16_t left, uint16_t top,
				uint16_t right, uint16_t bottom)
{
	//send_cmd_data_t send_cmd_data = self->info.mcu->ops->send_cmd_data;

	LCD_PRINT("rm6158_truly_invalidate_rect \n");

	return self->ops->panel_set_window(self, left, top, 
			right, bottom);
}


static int32_t rm6158_truly_set_direction(struct panel_spec *self, uint16_t direction)
{
	//send_cmd_data_t send_cmd_data = self->info.mcu->ops->send_cmd_data;

	LCD_PRINT("rm6158_truly_set_direction\n");
	self->direction = direction;
	
	return 0;
}

static int32_t rm6158_truly_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;

    //printk("[tong]rm6158_truly_enter_sleep: is_sleep = %d\n", is_sleep);//tong test 

      /*ZTE: added by tong.weili 解决快速连续按power键瞬间白屏 20120703 ++*/
	if(is_sleep) 
	{
		send_cmd(0x28);
		mdelay(50);
             send_cmd(0x10);
		//mdelay(200);
		mdelay(100);
	}
	else
	{
		send_cmd(0x11); // SLPOUT
		mdelay(120);
		send_cmd(0x29);		
		mdelay(50);		
	}
      /*ZTE: added by tong.weili 解决快速连续按power键瞬间白屏 20120703 --*/
      
	return 0;
}

static uint32_t rm6158_truly_read_id(struct panel_spec *self)
{
      send_data_t send_cmd = self->info.mcu->ops->send_cmd;
      read_data_t read_data = self->info.mcu->ops->read_data;
      send_data_t send_data = self->info.mcu->ops->send_data;
      uint32_t uID = 0;
      uint32_t uICID[5] = {0};
      uint32_t i;

      /*ZTE: added by tong.weili 生产模式默认为RM61581 20120724 ++*/
      printk("[tong]rm6158_truly_read_id: g_bootmode = 0x%x\n", g_bootmode);   
      if(g_bootmode)
      {
            return LCD_PANEL_ID_RM61581_TRULY;
      }
      /*ZTE: added by tong.weili 生产模式默认为RM61581 20120724 --*/
      
      send_cmd(0x01);//soft reset
      mdelay(120);
#if 0
      send_cmd(0xB0);
      send_data(0x00);
      
      send_cmd(0xBF);
      for(i = 0; i < 5; i++)
      {
          uICID[i] = read_data();
          printk("[tong]rm6158_truly_read_id: uICID[%d] = 0x%x\n", i, uICID[i]);        
      }

      if((uICID[1] == 0x01) && (uICID[2] == 0x22) && (uICID[3] == 0x15) && (uICID[4] == 0x81))
      {
          printk("[tong]LCD driver IC: r61581\n");
      }
      else
      {
          printk("[tong]LCD driver IC: rm6158\n");
          return -1;
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
          printk("[tong]rm6158_truly_read_id: uICID[%d] = 0x%x\n", i, uICID[i]);        
      }
      
      if((uICID[1] == 0x90))
      {
          printk("[tong]LCD driver IC: hx8357c\n");
          return -1;
      }
      else
      {  
          printk("[tong]LCD driver IC: r61581\n");
          
      }

      
#endif               
      send_cmd(0xA1);
      uID = read_data();
      printk("[tong]rm6158_truly_read_id: 0x%x from addr:0xA1\n", uID);
      
	return (0x6158|uID); 
}

static struct panel_operations lcd_rm6158_truly_operations = {
	.panel_init = rm6158_truly_init,
	.panel_set_window = rm6158_truly_set_window,
	.panel_invalidate_rect= rm6158_truly_invalidate_rect,
	.panel_invalidate = rm6158_truly_invalidate,
	.panel_set_direction = rm6158_truly_set_direction,
	.panel_enter_sleep = rm6158_truly_enter_sleep,
	.panel_readid          = rm6158_truly_read_id,
};

static struct timing_mcu lcd_rm6158_truly_timing[] = {
[LCD_REGISTER_TIMING] = {                    // read/write register timing
		.rcss = 170,  
		.rlpw = 190,
		.rhpw = 260,
		.wcss = 30,
		.wlpw = 30,
		.whpw = 30,
},
[LCD_GRAM_TIMING] = {                    // read/write gram timing
	.rcss = 170,
	.rlpw = 170,
	.rhpw = 250,
	.wcss = 30,
	.wlpw = 30,
	.whpw = 30,
	},
};

static struct info_mcu lcd_rm6158_truly_info = {
	.bus_mode = LCD_BUS_8080,
	.bus_width = 18,
	.timing = lcd_rm6158_truly_timing,
	.ops = NULL,
};

struct panel_spec lcd_panel_rm6158_truly = {
	.width = 320,
	.height = 480,
	.mode = LCD_MODE_MCU,
	.direction = LCD_DIRECT_NORMAL,
	.info = {.mcu = &lcd_rm6158_truly_info},
	.ops = &lcd_rm6158_truly_operations,
};

struct panel_cfg lcd_rm6158_truly = {
	/* this panel may on both CS0/1 */
	.lcd_cs = -1,
	.lcd_id = LCD_PANEL_ID_RM61581_TRULY,
	.lcd_name = "lcd_rm6158_truly",
	.panel = &lcd_panel_rm6158_truly,
};

static int __init lcd_rm6158_truly_init(void)
{
	return sprd_register_panel(&lcd_rm6158_truly);
}

subsys_initcall(lcd_rm6158_truly_init);



