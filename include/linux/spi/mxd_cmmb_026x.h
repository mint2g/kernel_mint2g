#ifndef __MXD_CMMB_026X_H__
#define __MXD_CMMB_026X_H__

#include <linux/types.h>

typedef void (*CMMB_POWER_FUNC)(void);
typedef int  (*CMMB_INIT_FUNC)(void);

 typedef void (* CMMB_SET_SPI_PIN_INPUT)(void);
 typedef void (* CMMB_RES_SPI_PIN_CFG)(void);


struct mxd_cmmb_026x_platform_data {
	CMMB_POWER_FUNC poweron;
	CMMB_POWER_FUNC poweroff;
	CMMB_INIT_FUNC  init;
	CMMB_SET_SPI_PIN_INPUT	set_spi_pin_input;
  CMMB_RES_SPI_PIN_CFG   restore_spi_pin_cfg;
};

#endif

