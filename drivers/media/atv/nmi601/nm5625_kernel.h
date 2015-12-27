#define NM5625_PWR_2P8_CTL 0x10000000
#define NM5625_PWR_1P2_CTL 0x20000000
#define NM5625_ATV_RESET_CTL 0x30000000
#define NM5625_ATV_I2C_READ 0x40000000
#define NM5625_ATV_I2C_WRITE 0x50000000



/**************************************************************
	
	Debug:

**************************************************************/

#define N_INIT		0x00000001
#define N_ERR		0x00000002
#define N_FUNC	0x00000004
#define N_TRACE	0x00000008
#define N_INFO		0x00000010

static u32 dflag = N_INIT|N_ERR|N_FUNC|N_INFO|N_TRACE;

#define DEBUG

#ifdef DEBUG
#define dPrint(f, str...) if (dflag & f) printk (str)
#else
#define dPrint(f, str...) /* nothing */
#endif

#define func_enter() dPrint (N_TRACE, "nmi: %s...enter\n", __func__)
#define func_exit()  dPrint(N_TRACE, "nmi: %s...exit\n", __func__)

