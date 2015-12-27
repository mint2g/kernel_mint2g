#if CONFIG_BATTERY_STC3115
int null_fn(void)
{
        return 0;                // for discharging status
}

int Temperature_fn(void)
{
	return (25);
}

static struct stc311x_platform_data stc3115_data = {
                .battery_online = NULL,
                .charger_online = null_fn, 		// used in stc311x_get_status()
                .charger_enable = null_fn,		// used in stc311x_get_status()
                .power_supply_register = NULL,
                .power_supply_unregister = NULL,

		.Vmode= 0,       /*REG_MODE, BIT_VMODE 1=Voltage mode, 0=mixed mode */
  		.Alm_SOC = 10,      /* SOC alm level %*/
  		.Alm_Vbat = 3600,   /* Vbat alm level mV*/
  		.CC_cnf = 303,      /* nominal CC_cnf, coming from battery characterisation*/
  		.VM_cnf = 307,      /* nominal VM cnf , coming from battery characterisation*/
  		.Cnom = 1500,       /* nominal capacity in mAh, coming from battery characterisation*/
  		.Rsense = 10,       /* sense resistor mOhms*/
  		.RelaxCurrent = 75, /* current for relaxation in mA (< C/20) */
  		.Adaptive = 1,     /* 1=Adaptive mode enabled, 0=Adaptive mode disabled */

		.CapDerating[6] = 0,   /* capacity derating in 0.1%, for temp = -20°C */
  		.CapDerating[5] = 0,   /* capacity derating in 0.1%, for temp = -10°C */
		.CapDerating[4] = 0,   /* capacity derating in 0.1%, for temp = 0°C */
		.CapDerating[3] = 0,   /* capacity derating in 0.1%, for temp = 10°C */
		.CapDerating[2] = 0,   /* capacity derating in 0.1%, for temp = 25°C */
		.CapDerating[1] = 0,   /* capacity derating in 0.1%, for temp = 40°C */
		.CapDerating[0] = 0,   /* capacity derating in 0.1%, for temp = 60°C */

  		.OCVOffset[15] = -93,    /* OCV curve adjustment */
		.OCVOffset[14] = 5,   /* OCV curve adjustment */
		.OCVOffset[13] = 15,    /* OCV curve adjustment */
		.OCVOffset[12] = -6,    /* OCV curve adjustment */
		.OCVOffset[11] = 0,    /* OCV curve adjustment */
		.OCVOffset[10] = -6,    /* OCV curve adjustment */
		.OCVOffset[9] = 25,     /* OCV curve adjustment */
		.OCVOffset[8] = 10,      /* OCV curve adjustment */
		.OCVOffset[7] = 11,      /* OCV curve adjustment */
		.OCVOffset[6] = 11,    /* OCV curve adjustment */
		.OCVOffset[5] = 11,    /* OCV curve adjustment */
		.OCVOffset[4] = 19,     /* OCV curve adjustment */
		.OCVOffset[3] = 38,    /* OCV curve adjustment */
		.OCVOffset[2] = 37,     /* OCV curve adjustment */
		.OCVOffset[1] = 57,    /* OCV curve adjustment */
		.OCVOffset[0] = 1,     /* OCV curve adjustment */

			/*if the application temperature data is preferred than the STC3115 temperature*/
  		.ExternalTemperature = Temperature_fn, /*External temperature fonction, return °C*/
  		.ForceExternalTemperature = 0, /* 1=External temperature, 0=STC3115 temperature */
		
};
#endif


static struct i2c_board_info __initdata beagle_i2c2_boardinfo[] = {

	#if CONFIG_BATTERY_STC3115
	{
		I2C_BOARD_INFO("stc3115", 0x70),
		.platform_data = &stc3115_data, //MSH AM
	},
	#endif

};
