/* drivers/video/sc8825/lcd_nt35516_spi.c
 *
 * Support for nt35516 spi LCD device
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
#include "sprdfb_panel.h"

#define NT35516_SpiWriteCmd(cmd) \ 
{ \
	spi_send_cmd((cmd >>8));\
	spi_send_cmd((cmd & 0xFF));\
}

#define  NT35516_SpiWriteData(data)\
{ \
	spi_send_data((data >> 8));\
	spi_send_data((data & 0xFF));\
}

static int32_t nt35516_rgb_spi_init(struct panel_spec *self)
{
	uint32_t data = 0;
	spi_send_cmd_t spi_send_cmd = self->info.rgb->bus_info.spi->ops->spi_send_cmd; 
	spi_send_data_t spi_send_data = self->info.rgb->bus_info.spi->ops->spi_send_data; 
	spi_read_t spi_read = self->info.rgb->bus_info.spi->ops->spi_read; 

	// NT35516 + AUO 4.29'
	// VCC=IOVCC=3.3V  RGB_24Bit
	NT35516_SpiWriteCmd(0xDB00);
	spi_read(&data);

	//TEST Commands
	NT35516_SpiWriteCmd(0xFF00); NT35516_SpiWriteData(0xAA);//AA

	NT35516_SpiWriteCmd(0xFF01); NT35516_SpiWriteData(0x55);//55
	NT35516_SpiWriteCmd(0xFF02); NT35516_SpiWriteData(0x25);//08
	NT35516_SpiWriteCmd(0xFF03); NT35516_SpiWriteData(0x01);//01

	//NT35516_SpiWriteCmd(0xFA0F); NT35516_SpiWriteData(0x20);

	NT35516_SpiWriteCmd(0xF300); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xF303); NT35516_SpiWriteData(0x15);

	//ENABLE PAGE 0    
	NT35516_SpiWriteCmd(0xF000); NT35516_SpiWriteData(0x55); //Manufacture Command Set Control   
	NT35516_SpiWriteCmd(0xF001); NT35516_SpiWriteData(0xAA);
	NT35516_SpiWriteCmd(0xF002); NT35516_SpiWriteData(0x52);
	NT35516_SpiWriteCmd(0xF003); NT35516_SpiWriteData(0x08);
	NT35516_SpiWriteCmd(0xF004); NT35516_SpiWriteData(0x00);

	NT35516_SpiWriteCmd(0xB800); NT35516_SpiWriteData(0x01); 
	NT35516_SpiWriteCmd(0xB801); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xB802); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xB803); NT35516_SpiWriteData(0x02);

	NT35516_SpiWriteCmd(0xBC00); NT35516_SpiWriteData(0x05); //Zig-Zag Inversion  
	NT35516_SpiWriteCmd(0xBC01); NT35516_SpiWriteData(0x05);
	NT35516_SpiWriteCmd(0xBC02); NT35516_SpiWriteData(0x05);

	NT35516_SpiWriteCmd(0x4C00); NT35516_SpiWriteData(0x11); //DB4=1,Enable Vivid Color,DB4=0 Disable Vivid Color

	// ENABLE PAGE 1   
	NT35516_SpiWriteCmd(0xF000); NT35516_SpiWriteData(0x55); //Manufacture Command Set Control      
	NT35516_SpiWriteCmd(0xF001); NT35516_SpiWriteData(0xAA);
	NT35516_SpiWriteCmd(0xF002); NT35516_SpiWriteData(0x52);
	NT35516_SpiWriteCmd(0xF003); NT35516_SpiWriteData(0x08);
	NT35516_SpiWriteCmd(0xF004); NT35516_SpiWriteData(0x01);//Page1

	NT35516_SpiWriteCmd(0xB000); NT35516_SpiWriteData(0x05); // Setting AVDD Voltage 6V
	NT35516_SpiWriteCmd(0xB001); NT35516_SpiWriteData(0x05);
	NT35516_SpiWriteCmd(0xB002); NT35516_SpiWriteData(0x05);

	NT35516_SpiWriteCmd(0xB600); NT35516_SpiWriteData(0x44); // Setting AVEE boosting time 2.5*vpnl 
	NT35516_SpiWriteCmd(0xB601); NT35516_SpiWriteData(0x44);
	NT35516_SpiWriteCmd(0xB602); NT35516_SpiWriteData(0x44);

	NT35516_SpiWriteCmd(0xB100); NT35516_SpiWriteData(0x05); // Setting AVEE Voltage -6V
	NT35516_SpiWriteCmd(0xB101); NT35516_SpiWriteData(0x05);
	NT35516_SpiWriteCmd(0xB102); NT35516_SpiWriteData(0x05);

	//Setting AVEE boosting time -2.5xVPNL
	NT35516_SpiWriteCmd(0xB700); NT35516_SpiWriteData(0x34); 
	NT35516_SpiWriteCmd(0xB701); NT35516_SpiWriteData(0x34);
	NT35516_SpiWriteCmd(0xB702); NT35516_SpiWriteData(0x34);

	//Setting VGLX boosting time  AVEE-AVDD
	NT35516_SpiWriteCmd(0xBA00); NT35516_SpiWriteData(0x14); //0x24 --> 0x14
	NT35516_SpiWriteCmd(0xBA01); NT35516_SpiWriteData(0x14);
	NT35516_SpiWriteCmd(0xBA02); NT35516_SpiWriteData(0x14);

	//Gamma Voltage
	NT35516_SpiWriteCmd(0xBC00); NT35516_SpiWriteData(0x00); 
	NT35516_SpiWriteCmd(0xBC01); NT35516_SpiWriteData(0xA0);//VGMP 0x88=4.7V  0x78=4.5V   0xA0=5.0V  
	NT35516_SpiWriteCmd(0xBC02); NT35516_SpiWriteData(0x00);//VGSP 

	//Gamma Voltage
	NT35516_SpiWriteCmd(0xBD00); NT35516_SpiWriteData(0x00); 
	NT35516_SpiWriteCmd(0xBD01); NT35516_SpiWriteData(0xA0);//VGMN 0x88=-4.7V 0x78=-4.5V   0xA0=-5.0V
	NT35516_SpiWriteCmd(0xBD02); NT35516_SpiWriteData(0x00);//VGSN  

	NT35516_SpiWriteCmd(0xBE00); NT35516_SpiWriteData(0x57); // Setting VCOM Offset Voltage  0x4E¸ÄÎª0x57  20111019 LIYAN

	//GAMMA RED Positive       
	NT35516_SpiWriteCmd(0xD100); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xD101); NT35516_SpiWriteData(0x32);
	NT35516_SpiWriteCmd(0xD102); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xD103); NT35516_SpiWriteData(0x41);
	NT35516_SpiWriteCmd(0xD104); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xD105); NT35516_SpiWriteData(0x54);
	NT35516_SpiWriteCmd(0xD106); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xD107); NT35516_SpiWriteData(0x67);
	NT35516_SpiWriteCmd(0xD108); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xD109); NT35516_SpiWriteData(0x7A);
	NT35516_SpiWriteCmd(0xD10A); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xD10B); NT35516_SpiWriteData(0x98);
	NT35516_SpiWriteCmd(0xD10C); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xD10D); NT35516_SpiWriteData(0xB0);
	NT35516_SpiWriteCmd(0xD10E); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xD10F); NT35516_SpiWriteData(0xDB);
	NT35516_SpiWriteCmd(0xD200); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xD201); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xD202); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xD203); NT35516_SpiWriteData(0x3F);
	NT35516_SpiWriteCmd(0xD204); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xD205); NT35516_SpiWriteData(0x70);
	NT35516_SpiWriteCmd(0xD206); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xD207); NT35516_SpiWriteData(0xB4);
	NT35516_SpiWriteCmd(0xD208); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xD209); NT35516_SpiWriteData(0xEC);
	NT35516_SpiWriteCmd(0xD20A); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xD20B); NT35516_SpiWriteData(0xED);
	NT35516_SpiWriteCmd(0xD20C); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xD20D); NT35516_SpiWriteData(0x1E);
	NT35516_SpiWriteCmd(0xD20E); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xD20F); NT35516_SpiWriteData(0x51);
	NT35516_SpiWriteCmd(0xD300); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xD301); NT35516_SpiWriteData(0x6C);
	NT35516_SpiWriteCmd(0xD302); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xD303); NT35516_SpiWriteData(0x8D);
	NT35516_SpiWriteCmd(0xD304); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xD305); NT35516_SpiWriteData(0xA5);
	NT35516_SpiWriteCmd(0xD306); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xD307); NT35516_SpiWriteData(0xC9);
	NT35516_SpiWriteCmd(0xD308); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xD309); NT35516_SpiWriteData(0xEA);
	NT35516_SpiWriteCmd(0xD30A); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xD30B); NT35516_SpiWriteData(0x19);
	NT35516_SpiWriteCmd(0xD30C); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xD30D); NT35516_SpiWriteData(0x45);
	NT35516_SpiWriteCmd(0xD30E); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xD30F); NT35516_SpiWriteData(0x7A);
	NT35516_SpiWriteCmd(0xD400); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xD401); NT35516_SpiWriteData(0xB0);
	NT35516_SpiWriteCmd(0xD402); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xD403); NT35516_SpiWriteData(0xF4);

	//Positive Gamma for GREEN
	NT35516_SpiWriteCmd(0xD500); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xD501); NT35516_SpiWriteData(0x37);
	NT35516_SpiWriteCmd(0xD502); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xD503); NT35516_SpiWriteData(0x41);
	NT35516_SpiWriteCmd(0xD504); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xD505); NT35516_SpiWriteData(0x54);
	NT35516_SpiWriteCmd(0xD506); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xD507); NT35516_SpiWriteData(0x67);
	NT35516_SpiWriteCmd(0xD508); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xD509); NT35516_SpiWriteData(0x7A);
	NT35516_SpiWriteCmd(0xD50A); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xD50B); NT35516_SpiWriteData(0x98);
	NT35516_SpiWriteCmd(0xD50C); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xD50D); NT35516_SpiWriteData(0xB0);
	NT35516_SpiWriteCmd(0xD50E); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xD50F); NT35516_SpiWriteData(0xDB);
	NT35516_SpiWriteCmd(0xD600); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xD601); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xD602); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xD603); NT35516_SpiWriteData(0x3F);
	NT35516_SpiWriteCmd(0xD604); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xD605); NT35516_SpiWriteData(0x70);
	NT35516_SpiWriteCmd(0xD606); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xD607); NT35516_SpiWriteData(0xB4);
	NT35516_SpiWriteCmd(0xD608); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xD609); NT35516_SpiWriteData(0xEC);
	NT35516_SpiWriteCmd(0xD60A); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xD60B); NT35516_SpiWriteData(0xED);
	NT35516_SpiWriteCmd(0xD60C); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xD60D); NT35516_SpiWriteData(0x1E);
	NT35516_SpiWriteCmd(0xD60E); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xD60F); NT35516_SpiWriteData(0x51);
	NT35516_SpiWriteCmd(0xD700); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xD701); NT35516_SpiWriteData(0x6C);
	NT35516_SpiWriteCmd(0xD702); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xD703); NT35516_SpiWriteData(0x8D);
	NT35516_SpiWriteCmd(0xD704); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xD705); NT35516_SpiWriteData(0xA5);
	NT35516_SpiWriteCmd(0xD706); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xD707); NT35516_SpiWriteData(0xC9);
	NT35516_SpiWriteCmd(0xD708); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xD709); NT35516_SpiWriteData(0xEA);
	NT35516_SpiWriteCmd(0xD70A); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xD70B); NT35516_SpiWriteData(0x19);
	NT35516_SpiWriteCmd(0xD70C); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xD70D); NT35516_SpiWriteData(0x45);
	NT35516_SpiWriteCmd(0xD70E); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xD70F); NT35516_SpiWriteData(0x7A);
	NT35516_SpiWriteCmd(0xD800); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xD801); NT35516_SpiWriteData(0xA0);
	NT35516_SpiWriteCmd(0xD802); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xD803); NT35516_SpiWriteData(0xF4);

	//Positive Gamma for BLUE
	NT35516_SpiWriteCmd(0xD900); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xD901); NT35516_SpiWriteData(0x32);
	NT35516_SpiWriteCmd(0xD902); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xD903); NT35516_SpiWriteData(0x41);
	NT35516_SpiWriteCmd(0xD904); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xD905); NT35516_SpiWriteData(0x54);
	NT35516_SpiWriteCmd(0xD906); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xD907); NT35516_SpiWriteData(0x67);
	NT35516_SpiWriteCmd(0xD908); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xD909); NT35516_SpiWriteData(0x7A);
	NT35516_SpiWriteCmd(0xD90A); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xD90B); NT35516_SpiWriteData(0x98);
	NT35516_SpiWriteCmd(0xD90C); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xD90D); NT35516_SpiWriteData(0xB0);
	NT35516_SpiWriteCmd(0xD90E); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xD90F); NT35516_SpiWriteData(0xDB);
	NT35516_SpiWriteCmd(0xDD00); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xDD01); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xDD02); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xDD03); NT35516_SpiWriteData(0x3F);
	NT35516_SpiWriteCmd(0xDD04); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xDD05); NT35516_SpiWriteData(0x70);
	NT35516_SpiWriteCmd(0xDD06); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xDD07); NT35516_SpiWriteData(0xB4);
	NT35516_SpiWriteCmd(0xDD08); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xDD09); NT35516_SpiWriteData(0xEC);
	NT35516_SpiWriteCmd(0xDD0A); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xDD0B); NT35516_SpiWriteData(0xED);
	NT35516_SpiWriteCmd(0xDD0C); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xDD0D); NT35516_SpiWriteData(0x1E);
	NT35516_SpiWriteCmd(0xDD0E); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xDD0F); NT35516_SpiWriteData(0x51);
	NT35516_SpiWriteCmd(0xDE00); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xDE01); NT35516_SpiWriteData(0x6C);
	NT35516_SpiWriteCmd(0xDE02); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xDE03); NT35516_SpiWriteData(0x8D);
	NT35516_SpiWriteCmd(0xDE04); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xDE05); NT35516_SpiWriteData(0xA5);
	NT35516_SpiWriteCmd(0xDE06); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xDE07); NT35516_SpiWriteData(0xC9);
	NT35516_SpiWriteCmd(0xDE08); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xDE09); NT35516_SpiWriteData(0xEA);
	NT35516_SpiWriteCmd(0xDE0A); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xDE0B); NT35516_SpiWriteData(0x19);
	NT35516_SpiWriteCmd(0xDE0C); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xDE0D); NT35516_SpiWriteData(0x45);
	NT35516_SpiWriteCmd(0xDE0E); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xDE0F); NT35516_SpiWriteData(0x7A);
	NT35516_SpiWriteCmd(0xDF00); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xDF01); NT35516_SpiWriteData(0xA0);
	NT35516_SpiWriteCmd(0xDF02); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xDF03); NT35516_SpiWriteData(0xF4);

	//Negative Gamma for RED
	NT35516_SpiWriteCmd(0xE000); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xE001); NT35516_SpiWriteData(0x32);
	NT35516_SpiWriteCmd(0xE002); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xE003); NT35516_SpiWriteData(0x41);
	NT35516_SpiWriteCmd(0xE004); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xE005); NT35516_SpiWriteData(0x54);
	NT35516_SpiWriteCmd(0xE006); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xE007); NT35516_SpiWriteData(0x67);
	NT35516_SpiWriteCmd(0xE008); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xE009); NT35516_SpiWriteData(0x7A);
	NT35516_SpiWriteCmd(0xE00A); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xE00B); NT35516_SpiWriteData(0x98);
	NT35516_SpiWriteCmd(0xE00C); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xE00D); NT35516_SpiWriteData(0xB0);
	NT35516_SpiWriteCmd(0xE00E); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xE00F); NT35516_SpiWriteData(0xDB);
	NT35516_SpiWriteCmd(0xE100); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xE101); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xE102); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xE103); NT35516_SpiWriteData(0x3F);
	NT35516_SpiWriteCmd(0xE104); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xE105); NT35516_SpiWriteData(0x70);
	NT35516_SpiWriteCmd(0xE106); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xE107); NT35516_SpiWriteData(0xB4);
	NT35516_SpiWriteCmd(0xE108); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xE109); NT35516_SpiWriteData(0xEC);
	NT35516_SpiWriteCmd(0xE10A); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xE10B); NT35516_SpiWriteData(0xED);
	NT35516_SpiWriteCmd(0xE10C); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xE10D); NT35516_SpiWriteData(0x1E);
	NT35516_SpiWriteCmd(0xE10E); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xE10F); NT35516_SpiWriteData(0x51);
	NT35516_SpiWriteCmd(0xE200); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xE201); NT35516_SpiWriteData(0x6C);
	NT35516_SpiWriteCmd(0xE202); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xE203); NT35516_SpiWriteData(0x8D);
	NT35516_SpiWriteCmd(0xE204); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xE205); NT35516_SpiWriteData(0xA5);
	NT35516_SpiWriteCmd(0xE206); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xE207); NT35516_SpiWriteData(0xC9);
	NT35516_SpiWriteCmd(0xE208); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xE209); NT35516_SpiWriteData(0xEA);
	NT35516_SpiWriteCmd(0xE20A); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xE20B); NT35516_SpiWriteData(0x19);
	NT35516_SpiWriteCmd(0xE20C); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xE20D); NT35516_SpiWriteData(0x45);
	NT35516_SpiWriteCmd(0xE20E); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xE20F); NT35516_SpiWriteData(0x7A);
	NT35516_SpiWriteCmd(0xE300); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xE301); NT35516_SpiWriteData(0xA0);
	NT35516_SpiWriteCmd(0xE302); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xE303); NT35516_SpiWriteData(0xF4);

	//Negative Gamma for GERREN
	NT35516_SpiWriteCmd(0xE400); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xE401); NT35516_SpiWriteData(0x32);
	NT35516_SpiWriteCmd(0xE402); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xE403); NT35516_SpiWriteData(0x41);
	NT35516_SpiWriteCmd(0xE404); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xE405); NT35516_SpiWriteData(0x54);
	NT35516_SpiWriteCmd(0xE406); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xE407); NT35516_SpiWriteData(0x67);
	NT35516_SpiWriteCmd(0xE408); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xE409); NT35516_SpiWriteData(0x7A);
	NT35516_SpiWriteCmd(0xE40A); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xE40B); NT35516_SpiWriteData(0x98);
	NT35516_SpiWriteCmd(0xE40C); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xE40D); NT35516_SpiWriteData(0xB0);
	NT35516_SpiWriteCmd(0xE40E); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xE40F); NT35516_SpiWriteData(0xDB);
	NT35516_SpiWriteCmd(0xE500); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xE501); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xE502); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xE503); NT35516_SpiWriteData(0x3F);
	NT35516_SpiWriteCmd(0xE504); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xE505); NT35516_SpiWriteData(0x70);
	NT35516_SpiWriteCmd(0xE506); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xE507); NT35516_SpiWriteData(0xB4);
	NT35516_SpiWriteCmd(0xE508); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xE509); NT35516_SpiWriteData(0xEC);
	NT35516_SpiWriteCmd(0xE50A); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xE50B); NT35516_SpiWriteData(0xED);
	NT35516_SpiWriteCmd(0xE50C); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xE50D); NT35516_SpiWriteData(0x1E);
	NT35516_SpiWriteCmd(0xE50E); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xE50F); NT35516_SpiWriteData(0x51);
	NT35516_SpiWriteCmd(0xE600); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xE601); NT35516_SpiWriteData(0x6C);
	NT35516_SpiWriteCmd(0xE602); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xE603); NT35516_SpiWriteData(0x8D);
	NT35516_SpiWriteCmd(0xE604); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xE605); NT35516_SpiWriteData(0xA5);
	NT35516_SpiWriteCmd(0xE606); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xE607); NT35516_SpiWriteData(0xC9);
	NT35516_SpiWriteCmd(0xE608); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xE609); NT35516_SpiWriteData(0xEA);
	NT35516_SpiWriteCmd(0xE60A); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xE60B); NT35516_SpiWriteData(0x19);
	NT35516_SpiWriteCmd(0xE60C); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xE60D); NT35516_SpiWriteData(0x45);
	NT35516_SpiWriteCmd(0xE60E); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xE60F); NT35516_SpiWriteData(0x7A);
	NT35516_SpiWriteCmd(0xE700); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xE701); NT35516_SpiWriteData(0xA0);
	NT35516_SpiWriteCmd(0xE702); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xE703); NT35516_SpiWriteData(0xF4);

	//Negative Gamma for BLUE
	NT35516_SpiWriteCmd(0xE800); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xE801); NT35516_SpiWriteData(0x32);
	NT35516_SpiWriteCmd(0xE802); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xE803); NT35516_SpiWriteData(0x41);
	NT35516_SpiWriteCmd(0xE804); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xE805); NT35516_SpiWriteData(0x54);
	NT35516_SpiWriteCmd(0xE806); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xE807); NT35516_SpiWriteData(0x67);
	NT35516_SpiWriteCmd(0xE808); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xE809); NT35516_SpiWriteData(0x7A);
	NT35516_SpiWriteCmd(0xE80A); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xE80B); NT35516_SpiWriteData(0x98);
	NT35516_SpiWriteCmd(0xE80C); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xE80D); NT35516_SpiWriteData(0xB0);
	NT35516_SpiWriteCmd(0xE80E); NT35516_SpiWriteData(0x00);
	NT35516_SpiWriteCmd(0xE80F); NT35516_SpiWriteData(0xDB);
	NT35516_SpiWriteCmd(0xE900); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xE901); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xE902); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xE903); NT35516_SpiWriteData(0x3F);
	NT35516_SpiWriteCmd(0xE904); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xE905); NT35516_SpiWriteData(0x70);
	NT35516_SpiWriteCmd(0xE906); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xE907); NT35516_SpiWriteData(0xB4);
	NT35516_SpiWriteCmd(0xE908); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xE909); NT35516_SpiWriteData(0xEC);
	NT35516_SpiWriteCmd(0xE90A); NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteCmd(0xE90B); NT35516_SpiWriteData(0xED);
	NT35516_SpiWriteCmd(0xE90C); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xE90D); NT35516_SpiWriteData(0x1E);
	NT35516_SpiWriteCmd(0xE90E); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xE90F); NT35516_SpiWriteData(0x51);
	NT35516_SpiWriteCmd(0xEA00); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xEA01); NT35516_SpiWriteData(0x6C);
	NT35516_SpiWriteCmd(0xEA02); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xEA03); NT35516_SpiWriteData(0x8D);
	NT35516_SpiWriteCmd(0xEA04); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xEA05); NT35516_SpiWriteData(0xA5);
	NT35516_SpiWriteCmd(0xEA06); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xEA07); NT35516_SpiWriteData(0xC9);
	NT35516_SpiWriteCmd(0xEA08); NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteCmd(0xEA09); NT35516_SpiWriteData(0xEA);
	NT35516_SpiWriteCmd(0xEA0A); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xEA0B); NT35516_SpiWriteData(0x19);
	NT35516_SpiWriteCmd(0xEA0C); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xEA0D); NT35516_SpiWriteData(0x45);
	NT35516_SpiWriteCmd(0xEA0E); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xEA0F); NT35516_SpiWriteData(0x7A);
	NT35516_SpiWriteCmd(0xEB00); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xEB01); NT35516_SpiWriteData(0xA0);
	NT35516_SpiWriteCmd(0xEB02); NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteCmd(0xEB03); NT35516_SpiWriteData(0xF4);

	NT35516_SpiWriteCmd(0x3A00); NT35516_SpiWriteData(0x77);

	NT35516_SpiWriteCmd(0x3500); NT35516_SpiWriteData(0x00);

	NT35516_SpiWriteCmd(0x1100); // Sleep out
	udelay(120);

	NT35516_SpiWriteCmd(0x2900); // Display On
}

static uint32_t nt35516_rgb_spi_readid(struct panel_spec *self)
{
	/*Jessica TODO: need read id*/
	return 0x16;
}

#if 0
void NT35516_RGB_SPI_set_display_window(
    uint16 left,     // start Horizon address
    uint16 right,     // end Horizon address
    uint16 top,         // start Vertical address
    uint16 bottom    // end Vertical address
    )
{
    NT35516_SpiWriteCmd(0x2A00); NT35516_SpiWriteData((left>>8));// set left address
    NT35516_SpiWriteCmd(0x2A01); NT35516_SpiWriteData((left&0xff));
    NT35516_SpiWriteCmd(0x2A02); NT35516_SpiWriteData((right>>8));// set right address
    NT35516_SpiWriteCmd(0x2A03); NT35516_SpiWriteData((right&0xff));

    NT35516_SpiWriteCmd(0x2B00); NT35516_SpiWriteData((top>>8));// set left address
    NT35516_SpiWriteCmd(0x2B01); NT35516_SpiWriteData((top&0xff));
    NT35516_SpiWriteCmd(0x2B02); NT35516_SpiWriteData((bottom>>8));// set bottom address
    NT35516_SpiWriteCmd(0x2B03); NT35516_SpiWriteData((bottom&0xff));
}

LCD_ERR_E NT35516_RGB_SPI_EnterSleep(BOOLEAN is_sleep)
{
    if(is_sleep==1)
    {
        NT35516_SpiWriteCmd(0x2800);
        LCD_Delay(200);
        NT35516_SpiWriteCmd(0x1000);
        LCD_Delay(200);
        //Lcd_EnvidOnOff(0);//RGB TIMENG OFF
        //LCD_Delay(200);        

    }
    else
    {
        //Lcd_EnvidOnOff(1);//RGB TIMENG ON 
        //LCD_Delay(200);
        //LCDinit_TFT();
        //LCD_Delay(200);

    }

    return 0;
}

LCD_ERR_E NT35516_RGB_SPI_SetDisplayWindow(
    uint16 left,         //left of the window
    uint16 top,            //top of the window
    uint16 right,        //right of the window
    uint16 bottom        //bottom of the window
    )
{         
    //NT35516_RGB_SPI_set_display_window(left, right, top, bottom);

    NT35516_SpiWriteCmd(0x2C00);
    return TRUE;
}
#endif


static struct panel_operations lcd_nt35516_rgb_spi_operations = {
	.panel_init = nt35516_rgb_spi_init,
	.panel_readid = nt35516_rgb_spi_readid,
};

static struct timing_rgb lcd_nt35516_rgb_timing = {
	.hfp = 16,  /* unit: pixel */
	.hbp = 16,
	.hsync = 1,
	.vfp = 16, /*unit: line*/
	.vbp = 16,
	.vsync = 1,
};

static struct spi_info lcd_nt35516_rgb_spi_info = {
	.ops = NULL,
};

static struct info_rgb lcd_nt35516_rgb_info = {
	.cmd_bus_mode  = SPRDFB_RGB_BUS_TYPE_SPI,
	.video_bus_width = 24, /*18,16*/
	.h_sync_pol = SPRDFB_POLARITY_POS,
	.v_sync_pol = SPRDFB_POLARITY_POS,
	.de_pol = SPRDFB_POLARITY_POS,
	.timing = &lcd_nt35516_rgb_timing,
	.bus_info = {
		.spi = &lcd_nt35516_rgb_spi_info,
	}
};

struct panel_spec lcd_nt35516_rgb_spi_spec = {
	.width = 540,
	.height = 960,
	.type = LCD_MODE_RGB,
	.direction = LCD_DIRECT_NORMAL,
	.info = {
		.rgb = &lcd_nt35516_rgb_info
	},
	.ops = &lcd_nt35516_rgb_spi_operations,
};

struct panel_cfg lcd_nt35516_rgb_spi = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x16,
	.lcd_name = "lcd_nt35516_rgb_spi",
	.panel = &lcd_nt35516_rgb_spi_spec,
};
static int __init lcd_nt35516_rgb_spi_init(void)
{
	return sprdfb_panel_register(&lcd_nt35516_rgb_spi);
}

subsys_initcall(lcd_nt35516_rgb_spi_init);
