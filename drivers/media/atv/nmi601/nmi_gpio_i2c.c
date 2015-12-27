/******************************************************************************
**
**	Copyright (c) Newport Media Inc.  All rights reserved.
**
** 	Module Name:  nmidrv_i2c.c
**	
**		This module implements the i2c interface NMI ATV   
**
** 
*******************************************************************************/
#include <linux/kernel.h>
#include <linux/init.h>
//#include <linux/android_pmem.h>
#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <mach/board.h>
#include <mach/hardware.h>
#include <asm/io.h>
#include <asm/delay.h>
#include <mach/gpio.h>
#include <linux/clk.h>
#include <mach/mfp.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include "nmi_gpio_i2c.h"
#include "nm5625_kernel.h"
#include <linux/delay.h>



#define NMI_SDA_PIN	81
#define NMI_SCL_PIN	80

static unsigned long I2C_func_cfg[] /*__initdata*/ = {
	MFP_CFG_X(SDA, GPIO, DS3, F_PULL_UP, F_PULL_UP, IO_OE),
        MFP_CFG_X(SCL, GPIO, DS3, F_PULL_UP, F_PULL_UP, IO_OE), 
};

static unsigned long I2C_std_func_cfg[] = {
	MFP_CFG_X(SDA, AF0, DS3, F_PULL_UP, F_PULL_UP, IO_NONE),
        MFP_CFG_X(SCL, AF0, DS3, F_PULL_UP, F_PULL_UP, IO_NONE), 
};

/******************************************************************************
**
**	I2c Defines
**
*******************************************************************************/
//#undef _DRIVE_   //this mode is nmi600 Recommend mode. please don't change it. 
#define  _DRIVE_
#define  I2C_DELAY_UNIT        2

/*#define set_sda_output()               mt_set_gpio_dir(NMI_SDA_PIN,1); \
                                                       mt_set_gpio_out(NMI_SDA_PIN,0)
#define set_sda_input()                  mt_set_gpio_dir(NMI_SDA_PIN,0) 
#define set_scl_output()                 mt_set_gpio_dir(NMI_SCL_PIN,1);  \
                                                        mt_set_gpio_out(NMI_SCL_PIN,0)
#define set_scl_input()                    mt_set_gpio_dir(NMI_SCL_PIN,0)*/

#define set_sda_output()				gpio_direction_output(NMI_SDA_PIN,0)
#define set_sda_input()					gpio_direction_input(NMI_SDA_PIN)		
#define set_scl_output()				gpio_direction_output(NMI_SCL_PIN,0)
#define set_scl_input()					gpio_direction_input(NMI_SCL_PIN)

#ifdef _DRIVE_
#define set_i2c_scl_PIN  			gpio_direction_output(NMI_SCL_PIN,1)	
#define clr_i2c_scl_PIN   			gpio_direction_output(NMI_SCL_PIN,0)
#define set_i2c_sda_PIN  			gpio_direction_output(NMI_SDA_PIN,1)
#define clr_i2c_sda_PIN    		    gpio_direction_output(NMI_SDA_PIN,0)
#else
/*#define set_i2c_scl_PIN  			mt_set_gpio_out(NMI_SCL_PIN,1)	
#define clr_i2c_scl_PIN   			mt_set_gpio_out(NMI_SCL_PIN,0)
#define set_i2c_sda_PIN  			mt_set_gpio_out(NMI_SDA_PIN,0)
#define clr_i2c_sda_PIN    		    mt_set_gpio_out(NMI_SDA_PIN,1)*/
#endif

//#define get_i2c_sda_PIN   			gpio_direction_input(NMI_SDA_PIN);gpio_get_value(NMI_SDA_PIN)
//#define get_i2c_scl_PIN   		    gpio_direction_input(NMI_SCL_PIN);gpio_get_value(NMI_SCL_PIN)
#define get_i2c_sda_PIN				gpio_get_value(NMI_SDA_PIN)
#define get_i2c_scl_PIN				gpio_get_value(NMI_SCL_PIN)


/******************************************************************************
**
**	I2c Platform Functions
**
*******************************************************************************/

static void i2c_delay(unsigned int time)
{
#if 0
	while(time--) {	
        ;
	}
#else
	udelay(time);
#endif
}

static void i2c_begin(void)
{
#ifdef _DRIVE_

	/* set SDA to high */
	set_i2c_sda_PIN;
	i2c_delay(I2C_DELAY_UNIT << 0);
	
	/* set SCL to high */
	set_i2c_scl_PIN;
	i2c_delay(I2C_DELAY_UNIT << 0);

	/* set SDA to low */
	clr_i2c_sda_PIN;
	i2c_delay(I2C_DELAY_UNIT << 0);

	/* set SCL to low */
	clr_i2c_scl_PIN;		
	i2c_delay(I2C_DELAY_UNIT << 0);

#else

	/* set SDA to high */
	set_sda_input();
	i2c_delay(I2C_DELAY_UNIT << 0);

	/* set SCL to high */
	set_scl_input();
	i2c_delay(I2C_DELAY_UNIT << 0);

	/* set SDA to low */
	set_sda_output();
	i2c_delay(I2C_DELAY_UNIT << 0);

	/* set SCL to low */
	set_scl_output();		
	i2c_delay(I2C_DELAY_UNIT << 0);
#endif

}

static void i2c_end(void)
{
#ifdef _DRIVE_
	/* set SDA to low */
	clr_i2c_sda_PIN;	
	i2c_delay(I2C_DELAY_UNIT << 2);

	/* set SCL to high */ 
	set_i2c_scl_PIN;
	i2c_delay(I2C_DELAY_UNIT << 0);

	/* set SDA to high */
	set_i2c_sda_PIN;		
	i2c_delay(I2C_DELAY_UNIT << 0);

#else
	set_sda_output();
	i2c_delay(I2C_DELAY_UNIT << 2);
	set_scl_input();
	i2c_delay(I2C_DELAY_UNIT << 3);
	set_sda_input();
	i2c_delay(I2C_DELAY_UNIT << 4);
#endif
}

static void i2c_write_ask(unsigned char flag)
{
#ifdef _DRIVE_
	/* set SDA to high to ack */
	if(flag)
		set_i2c_sda_PIN;
	else
		clr_i2c_sda_PIN;
	i2c_delay(I2C_DELAY_UNIT << 0);

	/* toggle clock */
	set_i2c_scl_PIN;
	i2c_delay(I2C_DELAY_UNIT << 0);
	clr_i2c_scl_PIN;
	i2c_delay(I2C_DELAY_UNIT << 0);

	/* set SDA to 1 */
	set_i2c_sda_PIN;
	i2c_delay(I2C_DELAY_UNIT << 0);

#else

   	//set_sda_output();

	if(flag)
		set_sda_input();
	//else
		//set_sda_output();
	
	i2c_delay(I2C_DELAY_UNIT << 0);
	set_scl_input();
	i2c_delay(I2C_DELAY_UNIT << 0);
	set_scl_output();
	i2c_delay(I2C_DELAY_UNIT << 0);
	set_sda_input();
	i2c_delay(I2C_DELAY_UNIT << 0);
#endif
}

static unsigned char i2c_read_ack(void)
{
	unsigned char ret;

#ifdef _DRIVE_
	/* set SDA to input */
	set_sda_input();
	/* delay */
	i2c_delay(I2C_DELAY_UNIT << 0);
	
	/* read */
	//gpio_direction_input(NMI_SDA_PIN);
	if (!get_i2c_sda_PIN) {
		ret = 1;
	} else {
		ret = 0;
		dPrint(N_ERR,"[MTKI2C] 1. i2c_read_ack (Error.. No Ack received)\n");
		i2c_delay(I2C_DELAY_UNIT << 0);
		if (!get_i2c_sda_PIN) {
			ret = 1;
			dPrint(N_ERR,"[MTKI2C] 2.i2c_read_ack (Correct after additional delay.)\n");	
		} else {
			ret = 0;
			dPrint(N_ERR,"[MTKI2C] 2.i2c_read_ack (Error.. No Ack received)\n");
		}
	}

	/* set SCL high */
	set_i2c_scl_PIN;
	i2c_delay(I2C_DELAY_UNIT << 0);

	/* set SCL low */
	clr_i2c_scl_PIN;
	i2c_delay(I2C_DELAY_UNIT << 0);

	/* set SDA back to output */
	set_sda_output();

#else

	set_sda_input();
	i2c_delay(I2C_DELAY_UNIT << 0);
	
	//gpio_direction_input(NMI_SDA_PIN);
	if (!get_i2c_sda_PIN) {
		ret = 1;
	} else {
		ret = 0;
		dPrint(N_ERR,"[MTKI2C] 1. i2c_read_ack (Error.. No Ack received)\n");
		i2c_delay(I2C_DELAY_UNIT << 0);
		if (!get_i2c_sda_PIN) {
			ret = 1;
			dPrint(N_ERR,"[MTKI2C] 2.i2c_read_ack (Correct after additional delay.)\n");	
		} else {
			ret = 0;
			dPrint(N_ERR,"[MTKI2C] 2.i2c_read_ack (Error.. No Ack received)\n");
		}
	}
	
	set_scl_input();
	i2c_delay(I2C_DELAY_UNIT << 0);
	set_scl_output();
	i2c_delay(I2C_DELAY_UNIT << 0);
	   
#endif
	return ret;
}

static unsigned char i2c_read_byte(void)
{
	unsigned char i;
	unsigned char ret = 0;

#ifdef _DRIVE_

	/* set SDA input */
	set_sda_input();
	
	/* loop */
	for (i = 0; i < 8; i++) {			
		/* delay */
		i2c_delay(I2C_DELAY_UNIT << 0);

		/* set SCL high */
		set_i2c_scl_PIN;
		/* delay */
		i2c_delay(I2C_DELAY_UNIT << 0);

		/* read SDA */
		ret	= ret<<1;
		//gpio_direction_input(NMI_SDA_PIN);
		if (get_i2c_sda_PIN)
			ret |= 1;
		/* delay */
		i2c_delay(I2C_DELAY_UNIT << 0);

		/* set SCL low */
		clr_i2c_scl_PIN;
		/* delay */
		i2c_delay(I2C_DELAY_UNIT << 0);

		/* if end, set SDA output */
		if (i == 7) {
			set_sda_output();
      	}
		/* delay */
     	i2c_delay(I2C_DELAY_UNIT << 0);
	}	

#else
	int retry,retry_val = 10000000;

	ret	= 0;

	set_sda_input();
	for (i = 0; i < 8; i++) {			
		i2c_delay(I2C_DELAY_UNIT << 0);

		set_scl_input();	

		i2c_delay(I2C_DELAY_UNIT << 0);
		ret	= ret<<1;
		if (get_i2c_sda_PIN)
			ret |= 1;
		i2c_delay(I2C_DELAY_UNIT << 0);

		retry = retry_val;
		while (retry >= 0) 
		{
			if (get_i2c_scl_PIN)
				break;
			else
			{
				i2c_delay(I2C_DELAY_UNIT << 0);
				retry--;
			}
		}

		//if (retry != retry_val)
		if (retry < (retry_val-10000))
		{
			//NMI_ERROR("[MTKI2C] i2c_read_byte: retry = %d\n",retry);
		}

		set_scl_output();
      		i2c_delay(I2C_DELAY_UNIT << 0);

		if (i==7){
		set_sda_output();
      		}
     		i2c_delay(I2C_DELAY_UNIT << 0);
	}		

#endif	

	return ret;
}

static unsigned char i2c_write_byte(unsigned char data)
{
	unsigned char i;

#ifdef _DRIVE_
	/* loop */
	for	(i = 0; i < 8; i++) {
		/* set SDA high or low depend on the data bit */
		if (data & 0x80)
			set_i2c_sda_PIN;
		else
			clr_i2c_sda_PIN;
		/* delay */
		i2c_delay(I2C_DELAY_UNIT << 0);

		data <<= 1;

		/* set SCL high */
		set_i2c_scl_PIN;
		/* delay */
		i2c_delay(I2C_DELAY_UNIT << 0);

		/* set SCL low */
		clr_i2c_scl_PIN;
		/* delay */
		i2c_delay(I2C_DELAY_UNIT << 0);
	}
#else
	int retry, retry_val = 10000000;

	//set_sda_output();

	for	(i = 0; i < 8; i++) {
		if (data & 0x80)
			set_sda_input();
		else
			set_sda_output();

		data <<= 1;

		i2c_delay(I2C_DELAY_UNIT << 0);
		set_scl_input();
		i2c_delay(I2C_DELAY_UNIT << 0);
		retry = retry_val;
		while (retry >= 0) 
		{
			if (get_i2c_scl_PIN)
				break;
			else
			{
				i2c_delay(I2C_DELAY_UNIT << 0);
				retry--;
			}
		}

		//if (retry != retry_val)
		if (retry < (retry_val-10000))
		{
			dPrint(N_TRACE,"i2c write_byte: retry = %d\n",retry);
		}
		set_scl_output();		
		i2c_delay(I2C_DELAY_UNIT << 0);
	}
	
#endif
	
	return i2c_read_ack();
}

/******************************************************************************
**
**	I2c Global Functions
**
*******************************************************************************/

int nmi_i2c_init(void)
{
	dPrint(N_TRACE,"nmi_i2c_init: enter...\n");

	sprd_mfp_config(I2C_func_cfg, ARRAY_SIZE(I2C_func_cfg));
	gpio_request(NMI_SCL_PIN,"scl");
	gpio_request(NMI_SDA_PIN,"sda");
	gpio_direction_output(NMI_SCL_PIN,1);
	gpio_direction_output(NMI_SDA_PIN,1);
/*#if 0
       //disable all inside pull( pullup & pulldown)
	NMI_SET_GPIO_PULL_DISABLE(NMI_SDA_PIN);
	NMI_SET_GPIO_PULL_DISABLE(NMI_SCL_PIN);
	//set gpio mode
	NMI_SET_GPIO_MODE_ENABLE(NMI_SDA_PIN);
	NMI_SET_GPIO_MODE_ENABLE(NMI_SCL_PIN);

#ifdef _DRIVE_
	//set output mode
	NMI_SET_GPIO_DIR( NMI_SDA_PIN,1);
	NMI_SET_GPIO_DIR( NMI_SCL_PIN,1);
      //set gpio high
	NMI_SET_GPIO_LEVEL( NMI_SDA_PIN,1);
	NMI_SET_GPIO_LEVEL( NMI_SCL_PIN,1);
#else
	//set input mode
	NMI_SET_GPIO_DIR( NMI_SDA_PIN,0);
	NMI_SET_GPIO_DIR( NMI_SCL_PIN,0);
#endif
#else
	//config scl
	mt_set_gpio_mode(NMI_SCL_PIN,GPIO_MODE_00);
	mt_set_gpio_dir(NMI_SCL_PIN, GPIO_DIR_OUT);
	mt_set_gpio_pull_enable(NMI_SCL_PIN,true);
	mt_set_gpio_pull_select(NMI_SCL_PIN, GPIO_PULL_UP);
	mt_set_gpio_out(NMI_SCL_PIN, GPIO_OUT_ONE);

	//config sda
	mt_set_gpio_mode(NMI_SDA_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(NMI_SDA_PIN, GPIO_DIR_OUT);
	mt_set_gpio_pull_enable(NMI_SDA_PIN,true);
	mt_set_gpio_pull_select(NMI_SDA_PIN, GPIO_PULL_UP);
	mt_set_gpio_out(NMI_SDA_PIN, GPIO_OUT_ONE);
#endif*/
	dPrint(N_TRACE,"nmi_i2c_init: exit...\n");

	return 1;
}

void nmi_i2c_deinit(void)
{
	dPrint(N_TRACE,"nmi_i2c_deinit: enter...\n");

	gpio_free(NMI_SDA_PIN);
	gpio_free(NMI_SCL_PIN);
	sprd_mfp_config(I2C_std_func_cfg, ARRAY_SIZE(I2C_std_func_cfg));
#if 0
	NMI_SET_GPIO_MODE_ENABLE(NMI_SDA_PIN);
	NMI_SET_GPIO_MODE_ENABLE(NMI_SCL_PIN);

	//set as input
	NMI_SET_GPIO_DIR( NMI_SDA_PIN,0);
	NMI_SET_GPIO_DIR( NMI_SCL_PIN,0);
#endif
	dPrint(N_TRACE,"nmi_i2c_deinit: exit...\n");

}

int nmi_i2c_read(unsigned char adr, unsigned char *b, unsigned long sz)
{
	int i;

	i2c_begin();
	i2c_write_byte((adr << 1)|0x1);

	for(i = 0; i < sz; i++) {
	    b[i] = i2c_read_byte();	    
		
	    if(i == sz-1) 
			i2c_write_ask(1);	
	    else  	           
			i2c_write_ask(0);
	}
 
	i2c_end();
	return 1;
}

int nmi_i2c_write(unsigned char adr, unsigned char *b, unsigned long sz)
{
	int i;

	i2c_begin();

	i2c_write_byte((adr << 1));
	for(i = 0; i < sz; i++) {
		i2c_write_byte(b[i]);
	}

	i2c_end();
	return 1;
}
 

