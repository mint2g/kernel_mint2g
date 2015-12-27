#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/irq.h>

#include <asm/gpio.h>
#include <asm/io.h>
#include "MMS100S_ISC_Updater_Customize.h"
#include "MMS100S_ISC_Updater.h"

#ifdef CONFIG_MACH_KYLETD
#include "KYLE_I_R03_VA20.c"
#elif defined(CONFIG_MACH_VASTOI)
#include <VASTO_I_ME_R02_V03.c>
#else
#include <KYLE_CORE28_PR_31_VC05.c>
#endif
//#include <MTH_KYLE_I_R00_VA06.c>
//#include <KYLE_I_R02_VA10.c>

#define MFS_HEADER_		5
#define MFS_DATA_		20480
#define PACKET_			(MFS_HEADER_ + MFS_DATA_)

unsigned char g_write_buffer[PACKET_];
unsigned char IC_type;
bool module_type_check;
bool exception_condition = false;

#define ISC_CMD_ENTER_ISC						0x5F
#define ISC_CMD_ENTER_ISC_PARA1					0x01
#define ISC_CMD_ISC_ADDR						0xD5
#define ISC_CMD_ISC_STATUS_ADDR					0xD9

#ifdef CONFIG_MACH_KYLETD
#define MODULE_COMPATIBILITY_ADDR	0x1C
#define FIRMWARE_VERSION_ADDR	0x1D
#elif defined(CONFIG_MACH_VASTOI)
#define MODULE_COMPATIBILITY_ADDR	0xC2
#define FIRMWARE_VERSION_ADDR	0xE2
#endif

#define SET_COMPATIBILITY_ADDR	0x4C09
#define SET_VERSION_ADDR	0x4C08
#define READ_RETRY_CNT 3

/*
 * ISC Status Value
 */
#define ISC_STATUS_RET_MASS_ERASE_DONE			0X0C
#define ISC_STATUS_RET_MASS_ERASE_MODE			0X08

#define MFS_DEFAULT_SLAVE_ADDR	0x48

static eMFSRet_t enter_ISC_mode(void);
static eMFSRet_t check_module_compatibility(const unsigned char *_pBinary_Data);
static eMFSRet_t check_firmware_version(const unsigned char *_pBinary_Data);
eMFSRet_t check_firmware_version_func(void);
#if !defined(CONFIG_MACH_KYLETD) && !defined(CONFIG_MACH_VASTOI)
static eMFSRet_t check_module_type(void);
#endif
eMFSRet_t MFS_ISC_force_update(void);
eMFSRet_t check_firmware_version_resume(void);

static int firmware_write(const unsigned char *_pBinary_Data);
static int firmware_verify(const unsigned char *_pBinary_Data);
static int mass_erase(void);
extern int mms_fw_i2c_write(char *buf, int length);
extern int mms_fw_i2c_read(u16 addr, u8 *value, u16 length);
extern int mms_fw_i2c_read_without_addr(u8 *value, u16 length);
extern int mms_fw_i2c_busrt_write(u8 *value, u16 length);
extern unsigned char TSP_PanelVersion, TSP_PhoneVersion;
unsigned char IC_type;
static int cal_mode;
static int get_boot_mode(char *str)
{

	get_option(&str, &cal_mode);
	printk("get_boot_mode, uart_mode : %d\n", cal_mode);
	return 1;
}
__setup("calmode=",get_boot_mode);
eMFSRet_t MFS_ISC_update(void)
{
	eMFSRet_t ret;
#if !defined(CONFIG_MACH_KYLETD) && !defined(CONFIG_MACH_VASTOI)	
//	unsigned char moduleComp;
	unsigned char read_buffer;
	int i;
#endif
	MFS_I2C_set_slave_addr(mfs_i2c_slave_addr);

#if defined(CONFIG_MACH_KYLETD) || defined(CONFIG_MACH_VASTOI)
	ret = check_firmware_version(MELFAS_binary);
	printk(KERN_ERR "<MELFAS> TSP_PanelVersion=%x\n", TSP_PanelVersion);
	printk(KERN_ERR "<MELFAS> TSP_PhoneVersion=%x\n", TSP_PhoneVersion);
#endif

	if(cal_mode)
	{
		printk("didn't update TSP F/W!! in CAL MODE\n");
		return 1;
	}
/*
	mms_fw_i2c_read(MODULE_COMPATIBILITY_ADDR, &read_buffer, 1);
	IC_type = read_buffer;
	printk(KERN_ERR "<MELFAS> Module Compatibility IC_type=%x\n",
		IC_type);
*/
#if !defined(CONFIG_MACH_KYLETD) && !defined(CONFIG_MACH_VASTOI)
	for (i = 0; i < READ_RETRY_CNT; i++) {
		ret = check_module_type();
		if (ret != MRET_SUCCESS) 
			MFS_ms_delay(50);
		else
			break;
	}
	printk(KERN_ERR "<MELFAS> IC_type is =%x!!!\n\n", IC_type);
	if (ret != MRET_SUCCESS) {
		MFS_ISC_force_update();
		ret = check_module_type();
		}
#endif		
	if (IC_type == 0x0F || IC_type == 0x52) {
		ret = check_module_compatibility(MELFAS_binary);
		if (ret != MRET_SUCCESS)
			goto MFS_ISC_UPDATE_FINISH;

		ret = check_firmware_version(MELFAS_binary);
		if (ret != MRET_SUCCESS)
			goto MFS_ISC_UPDATE_FINISH;

		MFS_TSP_reboot();

		ret = mass_erase();
		if (ret != MRET_SUCCESS)
			goto MFS_ISC_UPDATE_FINISH;

		ret = firmware_write(MELFAS_binary);
		if (ret != MRET_SUCCESS)
			goto MFS_ISC_UPDATE_FINISH;

		MFS_TSP_reboot();
		printk(KERN_ERR "<MELFAS> TOUCH IC REBOOT!!!\n");

		ret = enter_ISC_mode();
		if (ret != MRET_SUCCESS)
			goto MFS_ISC_UPDATE_FINISH;

		ret = firmware_verify(MELFAS_binary);
		if (ret != MRET_SUCCESS)
			goto MFS_ISC_UPDATE_FINISH;

		MFS_TSP_reboot();
		printk(KERN_ERR "<MELFAS> FIRMWARE_UPDATE_FINISHED!!!\n\n");
		ret = check_firmware_version(MELFAS_binary);

	}

	else {
		ret = check_module_compatibility(MELFAS_binary);
		if (ret != MRET_SUCCESS)
			goto MFS_ISC_UPDATE_FINISH;

		ret = check_firmware_version(MELFAS_binary);
		if (ret != MRET_SUCCESS)
			goto MFS_ISC_UPDATE_FINISH;

		MFS_TSP_reboot();

		ret = mass_erase();
		if (ret != MRET_SUCCESS)
			goto MFS_ISC_UPDATE_FINISH;

		ret = firmware_write(MELFAS_binary);
		if (ret != MRET_SUCCESS)
			goto MFS_ISC_UPDATE_FINISH;

		MFS_TSP_reboot();
		printk(KERN_ERR "<MELFAS> TOUCH IC REBOOT!!!\n");

		ret = enter_ISC_mode();
		if (ret != MRET_SUCCESS)
			goto MFS_ISC_UPDATE_FINISH;

		ret = firmware_verify(MELFAS_binary);
		if (ret != MRET_SUCCESS)
			goto MFS_ISC_UPDATE_FINISH;

		MFS_TSP_reboot();
		printk(KERN_ERR "<MELFAS> FIRMWARE_UPDATE_FINISHED!!!\n\n");
		ret = check_firmware_version(MELFAS_binary);
	}


	printk(KERN_ERR "<MELFAS> TSP_PanelVersion=%2x\n", TSP_PanelVersion);
	printk(KERN_ERR "<MELFAS> TSP_PhoneVersion=%2x\n", TSP_PhoneVersion);

	printk(KERN_ERR "<MELFAS> FIRMWARE_UPDATE_FINISHED!!!\n\n");

	MFS_ISC_UPDATE_FINISH:

	MFS_I2C_set_slave_addr(mfs_i2c_slave_addr);
	return ret;
}

eMFSRet_t MFS_ISC_force_update(void)
{
	eMFSRet_t ret;

	if(cal_mode)
	{
		printk("didn't update TSP F/W!! in CAL MODE\n");
		return 1;
	}
	
	MFS_I2C_set_slave_addr(mfs_i2c_slave_addr);

	MFS_TSP_reboot();

	if (IC_type == 0x0F ) {
		ret = mass_erase();
		if (ret != MRET_SUCCESS)
			goto MFS_ISC_force_update_FINISH;

		ret = firmware_write(MELFAS_binary);
		if (ret != MRET_SUCCESS)
			goto MFS_ISC_force_update_FINISH;

		MFS_TSP_reboot();
		printk(KERN_ERR "<MELFAS> TOUCH IC REBOOT!!!\n");

		ret = enter_ISC_mode();
		if (ret != MRET_SUCCESS)
			goto MFS_ISC_force_update_FINISH;

		ret = firmware_verify(MELFAS_binary);
		if (ret != MRET_SUCCESS)
			goto MFS_ISC_force_update_FINISH;

		MFS_TSP_reboot();
		printk(KERN_ERR "<MELFAS> FIRMWARE_UPDATE_FINISHED!!!\n\n");
		ret = check_firmware_version(MELFAS_binary);
	}
	
	else {	
	#ifdef CONFIG_MACH_KYLETD	
		ret = check_module_compatibility(MELFAS_binary);
		if (ret != MRET_SUCCESS)
			goto MFS_ISC_force_update_FINISH;		
	#endif	
		ret = mass_erase();
		if (ret != MRET_SUCCESS)
			goto MFS_ISC_force_update_FINISH;

		ret = firmware_write(MELFAS_binary);
		if (ret != MRET_SUCCESS)
			goto MFS_ISC_force_update_FINISH;

		MFS_TSP_reboot();
		printk(KERN_ERR "<MELFAS> TOUCH IC REBOOT!!!\n");

		ret = enter_ISC_mode();
		if (ret != MRET_SUCCESS)
			goto MFS_ISC_force_update_FINISH;

		ret = firmware_verify(MELFAS_binary);
		if (ret != MRET_SUCCESS)
			goto MFS_ISC_force_update_FINISH;

		MFS_TSP_reboot();

		ret = check_firmware_version(MELFAS_binary);
	}
	printk(KERN_ERR "<MELFAS> TSP_PanelVersion=%2x\n", TSP_PanelVersion);
	printk(KERN_ERR "<MELFAS> TSP_PhoneVersion=%2x\n", TSP_PhoneVersion);

	printk(KERN_ERR "<MELFAS> FIRMWARE_UPDATE_FINISHED!!!\n\n");

MFS_ISC_force_update_FINISH:

	MFS_I2C_set_slave_addr(mfs_i2c_slave_addr);
	return ret;
}

#if !defined(CONFIG_MACH_KYLETD) && !defined(CONFIG_MACH_VASTOI)
eMFSRet_t check_module_type(void)
{
	unsigned char read_buffer;
	unsigned char moduleComp;
	printk(KERN_ERR "<MELFAS> enter_check_module_type_function!!!\n\n");

	mms_fw_i2c_read(0x1F, &read_buffer, 1);
	moduleComp = read_buffer;
	printk(KERN_ERR "<MELFAS> Module Type=%x\n",
		moduleComp);

	if (moduleComp == 0x46) {
		printk(KERN_ERR "<MELFAS> G1F type detected!!!\n\n");
		IC_type = 0x0F;
		return MRET_SUCCESS;
	}

	else if (moduleComp == 0x4D) {
		printk(KERN_ERR "<MELFAS> G1M type detected!!!\n\n");
		IC_type = 0x0D;
		return MRET_SUCCESS;
	} else
		return MRET_CHECK_IC_TYPE_ERROR;
}
#endif
eMFSRet_t check_module_compatibility(const unsigned char *_pBinary_Data)
{
	unsigned char read_buffer;
	unsigned char moduleComp, setComp;
	printk(KERN_ERR "<MELFAS> Check Module Compatibility\n");

	mms_fw_i2c_read(MODULE_COMPATIBILITY_ADDR, &read_buffer, 1);
	moduleComp = read_buffer;
	printk(KERN_ERR "<MELFAS> Module Compatibility moduleComp=%x\n",
		moduleComp);
	setComp = _pBinary_Data[SET_COMPATIBILITY_ADDR];

#ifndef CONFIG_MACH_VASTOI //vasto-i tsp firmware no need this step. if "setComp-55" , the value of setComp will be wrong.
	setComp = setComp - 55;
#endif

#ifdef CONFIG_MACH_KYLETD	
	setComp = 0x03;	/* HW rev Rxx? */
#endif

	printk(KERN_ERR "<MELFAS> Module Compatibility setComp=%x\n", setComp);

//	return MRET_SUCCESS; // temp minkang

	if (moduleComp == setComp  || moduleComp == 0xE0 || moduleComp == 0x52)
		return MRET_SUCCESS;
	else if (IC_type == 0x0F && moduleComp != setComp) {
		exception_condition = true;
		return MRET_SUCCESS;
	}
	else if (moduleComp > 0xAA)
		return MRET_SUCCESS;
	else
		return MRET_CHECK_COMPATIBILITY_ERROR;
}

eMFSRet_t check_firmware_version(const unsigned char *_pBinary_Data)
{
	unsigned char read_buffer;
	unsigned char moduleVersion, setVersion;
	printk(KERN_ERR "<MELFAS> Check Firmware Version\n");
	mms_fw_i2c_read(FIRMWARE_VERSION_ADDR, &read_buffer, 1);

	moduleVersion = read_buffer;
	setVersion = _pBinary_Data[SET_VERSION_ADDR];
#ifdef CONFIG_MACH_KYLETD
	setVersion = 0x20;		/* kylei white panel R03 */
#endif	
	TSP_PanelVersion = moduleVersion;
	TSP_PhoneVersion = setVersion;

//	return MRET_SUCCESS; // temp minkang

	if (moduleVersion < setVersion)
		return MRET_SUCCESS;
	else if (exception_condition == true) {
		exception_condition = false;
		return MRET_SUCCESS;
	}
	else if(moduleVersion > 0xAA)
		return MRET_SUCCESS;
	else {
		printk(KERN_ERR "Do not need to download\n\n");
		printk(KERN_ERR "Module has a latest or same version\n");
		return MRET_CHECK_VERSION_ERROR;
	}
}

eMFSRet_t check_firmware_version_func(void)
{
	return check_firmware_version(MELFAS_binary);
}
eMFSRet_t enter_ISC_mode(void)
{
	unsigned char write_buffer[2];
	printk(KERN_ERR "<MELFAS> ENTER_ISC_MODE\n\n");
	write_buffer[0] = ISC_CMD_ENTER_ISC;
	write_buffer[1] = ISC_CMD_ENTER_ISC_PARA1;
	if (!mms_fw_i2c_write(write_buffer, 2))
		printk(KERN_ERR "<MELFAS> MMS100S Firmare is not exist!!!\n\n");
	MFS_ms_delay(50);
	return MRET_SUCCESS;
}

int firmware_write(const unsigned char *_pBinary_Data)
{
#define DATA_SIZE 1024
#define CLENGTH 4
	int i, lStartAddr = 0, lCnt = 0;


	while (lStartAddr*CLENGTH < 20*1024) {
		g_write_buffer[0] = ISC_CMD_ISC_ADDR;
		g_write_buffer[1] = (char)(lStartAddr & 0xFF);
		g_write_buffer[2] = (char)((lStartAddr>>8) & 0xFF);
		g_write_buffer[3] = g_write_buffer[4] = 0;

		for (i = 0; i < DATA_SIZE; i++)
			g_write_buffer[MFS_HEADER_ + i] = \
			_pBinary_Data[lStartAddr*CLENGTH + i];

		MFS_ms_delay(5);
		mms_fw_i2c_busrt_write(g_write_buffer,
							MFS_HEADER_+DATA_SIZE);

		lCnt++;
		lStartAddr = DATA_SIZE*lCnt/CLENGTH;
	}

	MFS_ms_delay(5);
	return MRET_SUCCESS;
}

int firmware_verify(const unsigned char *_pBinary_Data)
{
#define DATA_SIZE 1024
#define CLENGTH 4

	int i, k = 0;
	unsigned char  write_buffer[MFS_HEADER_], read_buffer[DATA_SIZE];
	unsigned short int start_addr = 0;

	printk(KERN_ERR "<MELFAS> FIRMARE VERIFY...\n");
	MFS_ms_delay(5);
	while (start_addr * CLENGTH < MFS_DATA_) {
		write_buffer[0] = ISC_CMD_ISC_ADDR;
		write_buffer[1] = (char)((start_addr) & 0XFF);
		write_buffer[2] = 0x40 + (char)((start_addr>>8) & 0XFF);
		write_buffer[3] = write_buffer[4] = 0;


		if (!mms_fw_i2c_write(write_buffer, MFS_HEADER_))
			return MRET_I2C_ERROR;

		MFS_ms_delay(5);
		if (!mms_fw_i2c_read_without_addr(read_buffer, DATA_SIZE))
			return MRET_I2C_ERROR;

		for (i = 0; i < DATA_SIZE; i++)
			if (read_buffer[i] != _pBinary_Data[i + start_addr * CLENGTH]) {
				printk(KERN_ERR "<MELFAS> VERIFY Failed\n");
				printk(KERN_ERR \
						"<MELFAS> original : 0x%2x, buffer : 0x%2x, addr : %d \n",
						_pBinary_Data[i + start_addr*CLENGTH], read_buffer[i], i);
				return MRET_FIRMWARE_VERIFY_ERROR;
			}

		k++;
		start_addr = DATA_SIZE*k/CLENGTH;
		return MRET_SUCCESS;
	}
	return MRET_SUCCESS;
}

int mass_erase(void)
{
	int i = 0;
	unsigned char mass_erase_cmd[MFS_HEADER_] = {
						ISC_CMD_ISC_ADDR, 0, 0xC1, 0, 0};
	unsigned char read_buffer[4] = { 0, };

	printk(KERN_ERR "<MELFAS> mass erase start\n\n");

	MFS_ms_delay(5);
	if (!mms_fw_i2c_write(mass_erase_cmd, MFS_HEADER_))
		printk(KERN_ERR "<MELFAS> mass erase start write fail\n\n");
	MFS_ms_delay(5);
	while (read_buffer[2] != ISC_STATUS_RET_MASS_ERASE_DONE) {
		mms_fw_i2c_write(mass_erase_cmd, MFS_HEADER_);
		MFS_ms_delay(1000);
		if (!mms_fw_i2c_read(ISC_CMD_ISC_STATUS_ADDR, read_buffer, 4))

		MFS_ms_delay(1000);

		if (read_buffer[2] == ISC_STATUS_RET_MASS_ERASE_DONE) {
			printk(KERN_ERR "<MELFAS> Firmware Mass Erase done.\n");
			return MRET_SUCCESS;
		} else if (read_buffer[2] == ISC_STATUS_RET_MASS_ERASE_MODE)
			printk(KERN_ERR "<MELFAS> Firmware Mass Erase enter success!!!\n");

		MFS_ms_delay(1);
		if (i > 20)
			return MRET_MASS_ERASE_ERROR;
		i++;
	}
	return MRET_SUCCESS;
}
