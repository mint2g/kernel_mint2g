#ifndef  _SC8810_NFC_H
#define	 _SC8810_NFC_H

#include <mach/hardware.h>

#define NFC_REG_BASE				(SPRD_NAND_BASE)
#define NFC_MBUF_ADDR				(NFC_REG_BASE + 0x2000)
#define NFC_SBUF_ADDR				(NFC_REG_BASE + 0x4000)

#define NFC_CMD					    (NFC_REG_BASE + 0x0000)
#define NFC_CFG0				    (NFC_REG_BASE + 0x0004)
#define NFC_CFG1				    (NFC_REG_BASE + 0x0008)
#define NFC_TIMING				    (NFC_REG_BASE + 0x0010)
#define NFC_TIEOUT				    (NFC_REG_BASE + 0x0014)
#define NFC_ID_STS				    (NFC_REG_BASE + 0x0018)
#define NFC_STS_EN				    (NFC_REG_BASE + 0x0020)
#define NFC_CLR_RAW				    (NFC_REG_BASE + 0x0024)

#define NFC_ECC_CFG0				(NFC_REG_BASE + 0x0030)
#define NFC_ECC_CFG1				(NFC_REG_BASE + 0x0034)

#define NFC_ECC_STS0				(NFC_REG_BASE + 0x0040)
#define NFC_ECC_STS1				(NFC_REG_BASE + 0x0044)
#define NFC_ECC_STS2				(NFC_REG_BASE + 0x0048)
#define NFC_ECC_STS3				(NFC_REG_BASE + 0x004C)

#define NFC_START_ADDR0				(NFC_REG_BASE + 0x0060)
#define NFC_START_ADDR1				(NFC_REG_BASE + 0x0064)
#define NFC_START_ADDR2				(NFC_REG_BASE + 0x0068)
#define NFC_START_ADDR3				(NFC_REG_BASE + 0x006C)
#define NFC_START_ADDR4				(NFC_REG_BASE + 0x0070)
#define NFC_START_ADDR5				(NFC_REG_BASE + 0x0074)
#define NFC_START_ADDR6				(NFC_REG_BASE + 0x0078)
#define NFC_START_ADDR7				(NFC_REG_BASE + 0x007C)

#define NFC_END_ADDR0				(NFC_REG_BASE + 0x0080)
#define NFC_END_ADDR1				(NFC_REG_BASE + 0x0084)
#define NFC_END_ADDR2				(NFC_REG_BASE + 0x0088)
#define NFC_END_ADDR3				(NFC_REG_BASE + 0x008c)
#define NFC_END_ADDR4				(NFC_REG_BASE + 0x0090)
#define NFC_END_ADDR5				(NFC_REG_BASE + 0x0094)
#define NFC_END_ADDR6				(NFC_REG_BASE + 0x0098)
#define NFC_END_ADDR7				(NFC_REG_BASE + 0x009C)

#define NFC_CMD_VALID				(1 << 31)
#define NFC_BLKNUM_OFFSET			(16)

#define NFC_SP_SIZE_OFFSET			(16)
#define NFC_CMD_SET_OFFSET			(15)
#define NFC_ADVANCE				    (1 << 14)
#define NFC_ADDR_4CYCLES			(1 << 12)
#define NFC_ADDR_5CYCLES			(2 << 12)
#define NFC_PAGE_TYPE_2K			(2 << 9)
#define NFC_PAGE_TYPE_4K			(3 << 9)
#define NFC_PAGE_TYPE_8K			(4 << 9)

#define NFC_CS1_SEL				    (1 << 6)
#define NFC_BUS_WIDTH_16			(1 << 5)
#define NFC_NFC_MEM_SWITCH			(1 << 4)
#define NFC_MEM_NFC_SWITCH			(1 << 3)
#define NFC_WPN					    (1 << 2)
#define NFC_CMD_CLR				    (1 << 1)
#define NFC_RBN					    (1 << 0)

#define NFC_ACS_OFFSET				(0)
#define NFC_RWH_OFFSET				(5)
#define NFC_RWL_OFFSET				(10)
#define NFC_ACR_OFFSET				(16)
#define NFC_RR_OFFSET				(21)
#define NFC_CEH_OFFSET				(26)

#define NFC_DONE_EN				    (1 << 0)
#define NFC_ECC_DONE_EN				(1 << 1)
#define NFC_ERR_EN				    (1 << 2)
#define NFC_WP_EN				    (1 << 3)
#define NFC_TO_EN				    (1 << 4)

#define NFC_DONE_STS				(1 << 16)
#define NFC_ECC_DONE_STS			(1 << 16)
#define NFC_ERR_STS				    (1 << 16)
#define NFC_WP_STS				    (1 << 16)
#define NFC_TO_STS				    (1 << 16)

#define NFC_DONE_RAW				(1 << 0)
#define NFC_ECC_DONE_RAW			(1 << 1)
#define NFC_ERR_RAW				    (1 << 2)
#define NFC_WP_RAW				    (1 << 3)
#define NFC_TO_RAW				    (1 << 4)

#define NFC_DONE_CLR				(1 << 16)
#define NFC_ECC_DONE_CLR			(1 << 17)
#define NFC_ERR_CLR				    (1 << 18)
#define NFC_WP_CLR				    (1 << 19)
#define NFC_TO_CLR				    (1 << 20)

#define NFC_ECC_SP_POS_OFFSET		(24)
#define NFC_ECC_SP_SIZE_OFFSET		(16)
#define NFC_ECC_NUM_OFFSET			(8)
#define NFC_ECC_MODE_OFFSET			(4)
#define NFC_ECC_MODE_1BIT			(0 << 4)
#define NFC_ECC_MODE_2BIT			(1 << 4)
#define NFC_ECC_MODE_4BIT			(2 << 4)
#define NFC_ECC_MODE_8BIT			(3 << 4)
#define NFC_ECC_MODE_12BIT			(4 << 4)
#define NFC_ECC_MODE_16BIT			(5 << 4)
#define NFC_ECC_MODE_24BIT			(6 << 4)
#define NFC_ECC_SP_ENDIAN			(1 << 3)
#define NFC_ECC_DECODE				(1 << 2)
#define NFC_ECC_AUTO_EN				(1 << 1)
#define NFC_ECC_ACTIVE				(1 << 0)

#define NFC_ECC_MAIN_ADDR_OFFSET	(16)
#define NFC_ECC_LOC_MAIN			(1 << 15)
#define NFC_ECC_ERR_NUM_MASK		(0x1f)

#define NFC_DEFAULT_TIMING			((12 << 0) | (7 << 5) | (10 << 10) | \
                                    (6 << 16) | (5 << 21) | (7 << 26))

 /* TODO : use global interface */
#define AHB_SOFT_NFC_RST			(1 << 5)

#define	NFC_ECC_EVENT  				(1)
#define	NFC_DONE_EVENT				(2)
#define	NFC_TX_DMA_EVENT			(4)
#define	NFC_RX_DMA_EVENT			(8)
#define	NFC_ERR_EVENT				(16)
#define	NFC_TIMEOUT_EVENT			(32)
/* #define NFC_TIMEOUT_VAL				(0x1000000) */
#define NFC_TIMEOUT_VAL 			(0xf0000)
#define NFC_ECCENCODE_TIMEOUT			(0xfff)
#define NFC_ECCDECODE_TIMEOUT			(0xfff)
#define NFC_RESET_TIMEOUT			(0x1ff)
#define NFC_STATUS_TIMEOUT			(0x1ff)
#define NFC_READID_TIMEOUT			(0x1ff)
#define NFC_ERASE_TIMEOUT			(0xc000)
#define NFC_READ_TIMEOUT			(0x2000)
#define NFC_WRITE_TIMEOUT			(0x4000)

#define NF_MC_CMD_ID				(0xFD)
#define NF_MC_ADDR_ID				(0xF1)
#define NF_MC_WAIT_ID				(0xF2)
#define NF_MC_RWORD_ID				(0xF3)
#define NF_MC_RBLK_ID				(0xF4)
#define NF_MC_WWORD_ID				(0xF6)
#define NF_MC_WBLK_ID				(0xF7)
#define NF_MC_NOP_ID				(0xFA)

#define CONFIG_SYS_NAND_ECCSIZE		(512)
#define CONFIG_SYS_NAND_ECCBYTES	(4)
#define CONFIG_SYS_NAND_ECC_MODE	(2)

#define NFC_CMD_ENCODE		(0x0000ffff)
#define NFC_CMD_DECODE		(NFC_CMD_ENCODE + 1)

struct sc8810_ecc_param {
	u8 mode;
	u8 ecc_num;
	u8 sp_size;
	u8 ecc_pos;
	u16 m_size;
	u8 *p_mbuf;
	u8 *p_sbuf;
	u8 *sts;
};

unsigned int sc8810_ecc_encode(struct sc8810_ecc_param *param);
unsigned int ecc_mode_convert(u32 mode);

#endif

