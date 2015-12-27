/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <mach/globalregs.h>
#include "sc8825_nand.h"
struct sprd_sc8825_nand_param {
	uint8_t id[5];
	uint8_t bus_width;
	uint8_t a_cycles;
	uint8_t sct_pg; //sector per page
	uint8_t oob_size; /* oob size per sector*/
	uint8_t ecc_pos; /* oob size per sector*/
	uint8_t info_pos; /* oob size per sector*/
	uint8_t info_size; /* oob size per sector*/
	uint8_t eccbit; /* ecc level per eccsize */
	uint16_t eccsize; /*bytes per sector for ecc calcuate once time */
};
struct sprd_sc8825_nand_info {
	struct mtd_info *mtd;
	struct nand_chip *nand;
	struct platform_device *pdev;
	//struct sprd_sc8825_nand_param *param;
	u32 chip; //chip index
	u32 v_mbuf; //virtual main buffer address
	u32 p_mbuf; //phy main buffer address
	u32 v_oob; // virtual oob buffer address
	u32 p_oob; //phy oob buffer address
	u32 page; //page address
	u16 column; //column address
	u16 oob_size;
	u16 m_size; //main part size per sector
	u16 s_size; //oob size per sector
	u8 a_cycles;//address cycles, 3, 4, 5
	u8 sct_pg; //sector per page
	u8 info_pos;
	u8 info_size;
	u8 ecc_mode;//0-1bit, 1-2bit, 2-4bit, 3-8bit,4-12bit,5-16bit,6-24bit
	u8 ecc_pos; // ecc postion
	u8 wp_en; //write protect enable
	u16 write_size;
	u16 page_per_bl;//page per block
	u16  buf_head;
	u16 _buf_tail;
	u8 ins_num;//instruction number
	u32 ins[NAND_MC_BUFFER_SIZE >> 1];
};
#define mtd_to_sc8825(m) (&g_sc8825_nand_info)
struct sprd_sc8825_nand_info g_sc8825_nand_info = {0};
static __attribute__((aligned(4))) u8  s_id_status[8];
//gloable variable
static struct nand_ecclayout sprd_sc8825_nand_oob_default = {
	.eccbytes = 0,
	.eccpos = {0},
	.oobfree = {
		{.offset = 2,
		 .length = 46}}
};
static u32 sprd_sc8825_reg_read(u32 addr)
{
	return readl(addr);
}
static void sprd_sc8825_reg_write(u32 addr, u32 val)
{
	writel(val, addr);
}
#if 0
static void sprd_sc8825_reg_or(u32 addr, u32 val)
{
	sprd_sc8825_reg_write(addr, sprd_sc8825_reg_read(addr) | val);
}
static void sprd_sc8825_reg_and(u32 addr, u32 mask)
{
	sprd_sc8825_reg_write(addr, sprd_sc8825_reg_read(addr) & mask);
}
#endif
static void sprd_sc8825_nand_int_clr(u32 bit_clear)
{
	sprd_sc8825_reg_write(NFC_INT_REG, bit_clear);
}
unsigned int sc8825_ecc_mode_convert(u32 mode)
{
	u32 mode_m;
	switch(mode)
	{
	case 1:
		mode_m = 0;
		break;
	case 2:
		mode_m = 1;
		break;
	case 4:
		mode_m = 2;
		break;
	case 8:
		mode_m = 3;
		break;
	case 12:
		mode_m = 4;
		break;
	case 16:
		mode_m = 5;
		break;
	case 24:
		mode_m = 6;
		break;
	default:
		mode_m = 0;
		break;
	}
	return mode_m;
}
/*spare info must be align to ecc pos, info_pos + info_size = ecc_pos,
 *the hardware must be config info_size and info_pos when ecc enable,and the ecc_info size can't be zero,
 *to simplify the nand_param_tb, the info is align with ecc and ecc at the last postion in one sector
*/
static struct sprd_sc8825_nand_param sprd_sc8825_nand_param_tb[] = {
	{{0xec, 0xbc, 0x00,0x55, 0x54}, 	1, 	5, 	4, 	16, 	12, 	11, 	1, 	2, 	512},
	{{0xec, 0xbc, 0x00,0x6A, 0x56}, 	1, 	5, 	8, 	16, 	9, 	8, 	1, 	4, 	512},
};
static void sprd_sc8825_nand_ecc_layout_gen(struct sprd_sc8825_nand_info *sc8825, int ecc_mode, int oob_size, int info_size, int info_pos,int scts, struct nand_ecclayout *layout)
{
	uint32_t sct = 0;
	uint32_t i = 0;
	uint32_t offset;
	uint32_t used_len ; //one sector ecc data size(byte)
	uint32_t eccbytes = 0; //one page ecc data size(byte)
	uint32_t oobfree_len = 0;
	used_len = (14 * ecc_mode + 7) / 8 + info_size;
	if(scts > ARRAY_SIZE(layout->oobfree))
	{
		while(1);
	}
	for(sct = 0; sct < scts; sct++)
	{
		//offset = (oob_size * sct) + ecc_pos;
		//for(i = 0; i < ecc_len; i++)
		offset = (oob_size * sct) + info_pos;
		for(i = 0; i < used_len; i++)
		{
			layout->eccpos[eccbytes++] = offset + i;
		}
		layout->oobfree[sct].offset = oob_size * sct;
		layout->oobfree[sct].length = oob_size - used_len ;
		oobfree_len += oob_size - used_len;
	}
	//for bad mark postion
	layout->oobfree[0].offset = 2;
	layout->oobfree[0].length = oob_size - used_len - 2;
	oobfree_len -= 2;
	layout->eccbytes = used_len * scts;
}
void nand_hardware_config(struct mtd_info *mtd, struct nand_chip *chip, u8 id[5])
{
	int index;
	int array;
	struct sprd_sc8825_nand_param * param;
	struct sprd_sc8825_nand_info *sc8825 = mtd_to_sc8825(mtd);
	int steps;
	array = ARRAY_SIZE(sprd_sc8825_nand_param_tb);
	for (index = 0; index < array; index ++) {
		param = sprd_sc8825_nand_param_tb + index;
		if ((param->id[0] == id[0])
			&& (param->id[1] == id[1])
			&& (param->id[2] == id[2])
			&& (param->id[3] == id[3])
			&& (param->id[4] == id[4]))
			break;
	}
	if (index < array) {
		//save the param config
		sc8825->ecc_mode = sc8825_ecc_mode_convert(param->eccbit);
		sc8825->m_size = param->eccsize;
		sc8825->s_size = param->oob_size;
		sc8825->a_cycles = param->a_cycles;
		sc8825->sct_pg = param->sct_pg;
		sc8825->info_pos = param->info_pos;
		sc8825->info_size = param->info_size;
		sc8825->write_size = sc8825->m_size * sc8825->sct_pg;
		sc8825->ecc_pos = param->ecc_pos;
		//sc8825->bus_width = param->bus_width;
		if(param->bus_width)
		{
			chip->options |= NAND_BUSWIDTH_16;
		}
		else
		{
			chip->options &= ~NAND_BUSWIDTH_16;
		}
//		sc8825->param = param;
		//update the mtd and nand default param after nand scan
		mtd->writesize = sc8825->write_size;
		mtd->oobsize = sc8825->s_size * sc8825->sct_pg;
		sc8825->oob_size = mtd->oobsize;
		/* Calculate the address shift from the page size */
		chip->page_shift = ffs(mtd->writesize) - 1;
		/* Convert chipsize to number of pages per chip -1. */
		chip->pagemask = (chip->chipsize >> chip->page_shift) - 1;

		sprd_sc8825_nand_ecc_layout_gen(sc8825, param->eccbit, param->oob_size, param->info_size,param->info_pos, param->sct_pg, &sprd_sc8825_nand_oob_default);
		chip->ecc.layout = &sprd_sc8825_nand_oob_default;
		sc8825->mtd = mtd;
	}
	else {
		//save the param config
		steps = mtd->writesize / CONFIG_SYS_NAND_ECCSIZE;
		sc8825->ecc_mode = sc8825_ecc_mode_convert(CONFIG_SYS_NAND_ECC_MODE);
		sc8825->m_size = CONFIG_SYS_NAND_ECCSIZE;
		sc8825->s_size = mtd->oobsize / steps;
		sc8825->a_cycles = mtd->writesize / CONFIG_SYS_NAND_ECCSIZE;
		sc8825->sct_pg = steps;
		sc8825->info_pos = sc8825->s_size - CONFIG_SYS_NAND_ECCBYTES - 1;
		sc8825->info_size = 1;
		sc8825->write_size = mtd->writesize;
		sc8825->oob_size = mtd->oobsize;
		sc8825->ecc_pos = sc8825->s_size - CONFIG_SYS_NAND_ECCBYTES;
		if(chip->chipsize > (128 << 20)) {
			sc8825->a_cycles = 5;
		}
		else {
			sc8825->a_cycles = 4;
		}

		sprd_sc8825_nand_ecc_layout_gen(sc8825, CONFIG_SYS_NAND_ECC_MODE, sc8825->s_size, sc8825->info_size, sc8825->info_pos, sc8825->sct_pg, &sprd_sc8825_nand_oob_default);
		chip->ecc.layout = &sprd_sc8825_nand_oob_default;
		sc8825->mtd = mtd;
	}
	sc8825->mtd = mtd;
}
//add one macro instruction to nand controller
static void sprd_sc8825_nand_ins_init(struct sprd_sc8825_nand_info *sc8825)
{
	sc8825->ins_num = 0;
}
static void sprd_sc8825_nand_ins_add(u16 ins, struct sprd_sc8825_nand_info *sc8825)
{
	u16 *buf = (u16 *)sc8825->ins;
	if(sc8825->ins_num >= NAND_MC_BUFFER_SIZE)
	{
		while(1);
	}
	*(buf + sc8825->ins_num) = ins;
	sc8825->ins_num++;
}
static void sprd_sc8825_nand_ins_exec(struct sprd_sc8825_nand_info *sc8825)
{
	u32 i;
	u32 cfg0;

	for(i = 0; i < ((sc8825->ins_num + 1) >> 1); i++)
	{
		sprd_sc8825_reg_write(NFC_INST0_REG + (i << 2), sc8825->ins[i]);
	}
	cfg0 = sprd_sc8825_reg_read(NFC_CFG0_REG);
	if(sc8825->wp_en)
	{
		cfg0 &= ~NFC_WPN;
	}
	else
	{
		cfg0 |= NFC_WPN;
	}
	if(sc8825->chip)
	{
		cfg0 |= CS_SEL;
	}
	else
	{
		cfg0 &= ~CS_SEL;
	}
	sprd_sc8825_nand_int_clr(INT_STSMCH_CLR | INT_WP_CLR | INT_TO_CLR | INT_DONE_CLR);//clear all interrupt
	sprd_sc8825_reg_write(NFC_CFG0_REG, cfg0);
	sprd_sc8825_reg_write(NFC_START_REG, NFC_START);
}
static int sprd_sc8825_nand_wait_finish(struct sprd_sc8825_nand_info *sc8825)
{
	unsigned int value;
	unsigned int counter = 0;
	while((counter < NFC_TIMEOUT_VAL/*time out*/))
	{
		value = sprd_sc8825_reg_read(NFC_INT_REG);
		if(value & INT_DONE_RAW)
		{
			break;
		}
		counter ++;
	}
	sprd_sc8825_reg_write(NFC_INT_REG, 0xf00); //clear all interrupt status
	if(counter > NFC_TIMEOUT_VAL)
	{
		while (1);
		return -1;
	}
	return 0;
}
static void sprd_sc8825_nand_wp_en(struct sprd_sc8825_nand_info *sc8825, int en)
{
	if(en)	{
		sc8825->wp_en = 1;
	}
	else
	{
		sc8825->wp_en = 0;
	}
}
static void sprd_sc8825_select_chip(struct mtd_info *mtd, int chip)
{
	struct sprd_sc8825_nand_info *sc8825 = mtd_to_sc8825(mtd);
	if(chip < 0) { //for release caller
		return;
	}
	sc8825->chip = chip;
}
static void sprd_sc8825_nand_read_status(struct sprd_sc8825_nand_info *sc8825)
{
	u32 *buf;

	sprd_sc8825_nand_ins_init(sc8825);
	sprd_sc8825_nand_ins_add(NAND_MC_CMD(NAND_CMD_STATUS), sc8825);
	sprd_sc8825_nand_ins_add(NAND_MC_IDST(1), sc8825);
	sprd_sc8825_nand_ins_add(NFC_MC_DONE_ID, sc8825);
	sprd_sc8825_reg_write(NFC_CFG0_REG, NFC_ONLY_NAND_MODE);
	sprd_sc8825_nand_ins_exec(sc8825);
	sprd_sc8825_nand_wait_finish(sc8825);
	buf = (u32 *)s_id_status;
	*buf = sprd_sc8825_reg_read(NFC_STATUS0_REG);
	sc8825->buf_head = 0;
	sc8825->_buf_tail = 1;
}
static void sprd_sc8825_nand_read_id(struct sprd_sc8825_nand_info *sc8825, u32 *buf)
{

	sprd_sc8825_nand_ins_init(sc8825);
	sprd_sc8825_nand_ins_add(NAND_MC_CMD(NAND_CMD_READID), sc8825);
	sprd_sc8825_nand_ins_add(NAND_MC_ADDR(0), sc8825);
	sprd_sc8825_nand_ins_add(NAND_MC_IDST(8), sc8825);
	sprd_sc8825_nand_ins_add(NFC_MC_DONE_ID, sc8825);

	sprd_sc8825_reg_write(NFC_CFG0_REG, NFC_ONLY_NAND_MODE);
	sprd_sc8825_nand_ins_exec(sc8825);
	sprd_sc8825_nand_wait_finish(sc8825);
	*buf = sprd_sc8825_reg_read(NFC_STATUS0_REG);
	*(buf + 1) = sprd_sc8825_reg_read(NFC_STATUS1_REG);
	sc8825->buf_head = 0;
	sc8825->_buf_tail = 8;

}
static void sprd_sc8825_nand_reset(struct sprd_sc8825_nand_info *sc8825)
{
	sprd_sc8825_nand_ins_init(sc8825);
	sprd_sc8825_nand_ins_add(NAND_MC_CMD(NAND_CMD_RESET), sc8825);
	sprd_sc8825_nand_ins_add(NFC_MC_WRB0_ID, sc8825); //wait rb
	sprd_sc8825_nand_ins_add(NFC_MC_DONE_ID, sc8825);
	//config register
	sprd_sc8825_reg_write(NFC_CFG0_REG, NFC_ONLY_NAND_MODE);
	sprd_sc8825_nand_ins_exec(sc8825);
	sprd_sc8825_nand_wait_finish(sc8825);
}
static u32 sprd_sc8825_get_decode_sts(u32 index)
{
	u32 err;
	u32 shift;
	u32 reg_addr;
	reg_addr = NFC_STATUS0_REG + (index & 0xfffffffc);
	shift = (index & 0x3) << 3;
	err = sprd_sc8825_reg_read(reg_addr);
	err >>= shift;
	if((err & ECC_ALL_FF))
	{
		err = 0;
	}
	else
	{
		err &= ERR_ERR_NUM0_MASK;
	}
	return err;
}
//read large page
static int sprd_sc8825_nand_read_lp(struct mtd_info *mtd,u8 *mbuf, u8 *sbuf,u32 raw)
{
	struct sprd_sc8825_nand_info *sc8825 = mtd_to_sc8825(mtd);
	struct nand_chip *chip = sc8825->nand;
	u32 column;
	u32 page_addr;
	u32 cfg0;
	u32 cfg1;
	u32 cfg2;
	u32 i;
	u32 err;
	page_addr = sc8825->page;

	if(sbuf) {
		column = mtd->writesize;
	}
	else
	{
		column = 0;
	}
	if(chip->options & NAND_BUSWIDTH_16)
	{
		column >>= 1;
	}

	sprd_sc8825_nand_ins_init(sc8825);
	sprd_sc8825_nand_ins_add(NAND_MC_CMD(NAND_CMD_READ0), sc8825);
	sprd_sc8825_nand_ins_add(NAND_MC_ADDR(column & 0xff), sc8825);
	column >>= 8;
	sprd_sc8825_nand_ins_add(NAND_MC_ADDR(column & 0xff), sc8825);

	sprd_sc8825_nand_ins_add(NAND_MC_ADDR(page_addr & 0xff), sc8825);
	page_addr >>= 8;
	sprd_sc8825_nand_ins_add(NAND_MC_ADDR(page_addr & 0xff), sc8825);

	if (5 == sc8825->a_cycles)// five address cycles
	{
		page_addr >>= 8;
		sprd_sc8825_nand_ins_add(NAND_MC_ADDR(page_addr & 0xff), sc8825);
	}
	sprd_sc8825_nand_ins_add(NAND_MC_CMD(NAND_CMD_READSTART), sc8825);

	sprd_sc8825_nand_ins_add(NFC_MC_WRB0_ID, sc8825); //wait rb
	if(mbuf && sbuf)
	{
		sprd_sc8825_nand_ins_add(NAND_MC_SRDT, sc8825);
		//switch to main part
		sprd_sc8825_nand_ins_add(NAND_MC_CMD(NAND_CMD_RNDOUT), sc8825);
		sprd_sc8825_nand_ins_add(NAND_MC_ADDR(0), sc8825);
		sprd_sc8825_nand_ins_add(NAND_MC_ADDR(0), sc8825);
		sprd_sc8825_nand_ins_add(NAND_MC_CMD(NAND_CMD_RNDOUTSTART), sc8825);
		sprd_sc8825_nand_ins_add(NAND_MC_MRDT, sc8825);
	}
	else
	{
		sprd_sc8825_nand_ins_add(NAND_MC_MRDT, sc8825);
	}
	sprd_sc8825_nand_ins_add(NFC_MC_DONE_ID, sc8825);
	//config registers
	cfg0 = NFC_AUTO_MODE | MAIN_SPAR_APT | ((sc8825->sct_pg - 1)<< SECTOR_NUM_OFFSET);
	if((!raw) && mbuf && sbuf)
	{
		cfg0 |= ECC_EN | DETECT_ALL_FF;
	}
	if(chip->options & NAND_BUSWIDTH_16)
	{
		cfg0 |= BUS_WIDTH;
	}
	cfg1 = (sc8825->info_size - 1) << SPAR_INFO_SIZE_OFFSET;
	cfg2 = (sc8825->ecc_mode << ECC_MODE_OFFSET) | (sc8825->info_pos << SPAR_INFO_POS_OFFSET) | ((sc8825->sct_pg - 1) << SPAR_SECTOR_NUM_OFFSET) | sc8825->ecc_pos;

	if(mbuf && sbuf)
	{
		cfg1 |= (sc8825->m_size - 1) | ((sc8825->s_size  - 1)<< SPAR_SIZE_OFFSET);
		sprd_sc8825_reg_write(NFC_MAIN_ADDR_REG, sc8825->p_mbuf);
		sprd_sc8825_reg_write(NFC_SPAR_ADDR_REG, sc8825->p_oob);
		cfg0 |= MAIN_USE | SPAR_USE;
	}
	else
	{
		if(mbuf)
		{
			cfg1 |= (sc8825->m_size - 1);
			sprd_sc8825_reg_write(NFC_MAIN_ADDR_REG, sc8825->p_mbuf);
		}
		if(sbuf)
		{
			cfg1 |= (sc8825->s_size - 1);
			sprd_sc8825_reg_write(NFC_MAIN_ADDR_REG, sc8825->p_oob);
		}
		cfg0 |= MAIN_USE;
	}
	sprd_sc8825_reg_write(NFC_CFG0_REG, cfg0);
	sprd_sc8825_reg_write(NFC_CFG1_REG, cfg1);
	sprd_sc8825_reg_write(NFC_CFG2_REG, cfg2);

	sprd_sc8825_nand_ins_exec(sc8825);
	sprd_sc8825_nand_wait_finish(sc8825);
	if(!raw) {
		for(i = 0; i < sc8825->sct_pg; i++) {
			err = sprd_sc8825_get_decode_sts(i);
			if(err == ERR_ERR_NUM0_MASK) {
				mtd->ecc_stats.failed++;
			}
			else {
				mtd->ecc_stats.corrected += err;
			}
		}
	}
	if(mbuf) {
		memcpy(mbuf, (const void *)sc8825->v_mbuf, sc8825->write_size);
	}
	if(sbuf) {
		memcpy(sbuf, (const void *)sc8825->v_oob, sc8825->oob_size);
	}
	return 0;
}
static int sprd_sc8825_nand_read_sp(struct mtd_info *mtd,u8 *mbuf, u8 *sbuf,u32 raw)
{
	return 0;
}
static int sprd_sc8825_nand_write_lp(struct mtd_info *mtd,const u8 *mbuf, u8 *sbuf,u32 raw)
{
	struct sprd_sc8825_nand_info *sc8825 = mtd_to_sc8825(mtd);
	struct nand_chip *chip = sc8825->nand;
	u32 column;
	u32 page_addr;
	u32 cfg0;
	u32 cfg1;
	u32 cfg2;
	page_addr = sc8825->page;
	if(mbuf) {
		column = 0;
	}
	else {
		column = mtd->writesize;
	}
	if(chip->options & NAND_BUSWIDTH_16)
	{
		column >>= 1;
	}
	if(mbuf) {
		memcpy((void *)sc8825->v_mbuf, (const void *)mbuf, sc8825->write_size);
	}
	if(sbuf) {
		memcpy((void *)sc8825->v_oob, (const void *)sbuf, sc8825->oob_size);
	}
	sprd_sc8825_nand_ins_init(sc8825);
	sprd_sc8825_nand_ins_add(NAND_MC_CMD(NAND_CMD_SEQIN), sc8825);
	sprd_sc8825_nand_ins_add(NAND_MC_ADDR(column & 0xff), sc8825);
	column >>= 8;
	sprd_sc8825_nand_ins_add(NAND_MC_ADDR(column & 0xff), sc8825);
	sprd_sc8825_nand_ins_add(NAND_MC_ADDR(page_addr & 0xff), sc8825);
	page_addr >>= 8;
	sprd_sc8825_nand_ins_add(NAND_MC_ADDR(page_addr & 0xff), sc8825);

	if (5 == sc8825->a_cycles)// five address cycles
	{
		page_addr >>= 8;
		sprd_sc8825_nand_ins_add(NAND_MC_ADDR(page_addr & 0xff), sc8825);
	}

	sprd_sc8825_nand_ins_add(NAND_MC_MWDT, sc8825);
	if(mbuf && sbuf)
	{
		sprd_sc8825_nand_ins_add(NAND_MC_SWDT, sc8825);
	}
	sprd_sc8825_nand_ins_add(NAND_MC_CMD(NAND_CMD_PAGEPROG), sc8825);
	sprd_sc8825_nand_ins_add(NFC_MC_WRB0_ID, sc8825); //wait rb

	sprd_sc8825_nand_ins_add(NFC_MC_DONE_ID, sc8825);
	//config registers
	cfg0 = NFC_AUTO_MODE | NFC_RW |  NFC_WPN | MAIN_SPAR_APT | ((sc8825->sct_pg - 1)<< SECTOR_NUM_OFFSET);
	if((!raw) && mbuf && sbuf)
	{
		cfg0 |= ECC_EN;
	}
	if(chip->options & NAND_BUSWIDTH_16)
	{
		cfg0 |= BUS_WIDTH;
	}
	cfg1 = ((sc8825->info_size - 1) << SPAR_INFO_SIZE_OFFSET);
	cfg2 = (sc8825->ecc_mode << ECC_MODE_OFFSET) | (sc8825->info_pos << SPAR_INFO_POS_OFFSET) | ((sc8825->sct_pg - 1) << SPAR_SECTOR_NUM_OFFSET) | sc8825->ecc_pos;
	if(mbuf && sbuf)
	{
		cfg0 |= MAIN_USE | SPAR_USE;
		cfg1 = (sc8825->m_size - 1) | ((sc8825->s_size - 1) << SPAR_SIZE_OFFSET);
		sprd_sc8825_reg_write(NFC_MAIN_ADDR_REG, sc8825->p_mbuf);
		sprd_sc8825_reg_write(NFC_SPAR_ADDR_REG, sc8825->p_oob);
	}
	else
	{
		cfg0 |= MAIN_USE;
		if(mbuf)
		{
			cfg1 |= sc8825->m_size - 1;
			sprd_sc8825_reg_write(NFC_MAIN_ADDR_REG, sc8825->p_mbuf);
		}
		else
		{
			cfg1 |= sc8825->s_size - 1;
			sprd_sc8825_reg_write(NFC_MAIN_ADDR_REG, sc8825->p_oob);
		}
	}
	sprd_sc8825_reg_write(NFC_CFG0_REG, cfg0);
	sprd_sc8825_reg_write(NFC_CFG1_REG, cfg1);
	sprd_sc8825_reg_write(NFC_CFG2_REG, cfg2);
	sprd_sc8825_nand_ins_exec(sc8825);
	sprd_sc8825_nand_wait_finish(sc8825);

	return 0;
}
static int sprd_sc8825_nand_write_sp(struct mtd_info *mtd,const u8 *mbuf, u8 *sbuf,u32 raw)
{
	return 0;
}
static void sprd_sc8825_erase(struct mtd_info *mtd, int page_addr)
{
	struct sprd_sc8825_nand_info *sc8825 = mtd_to_sc8825(mtd);
	u32 cfg0 = 0;
	sprd_sc8825_nand_ins_init(sc8825);
	sprd_sc8825_nand_ins_add(NAND_MC_CMD(NAND_CMD_ERASE1), sc8825);
	sprd_sc8825_nand_ins_add(NAND_MC_ADDR(page_addr & 0xff), sc8825);
	page_addr >>= 8;
	sprd_sc8825_nand_ins_add(NAND_MC_ADDR(page_addr & 0xff), sc8825);
	if((5 == sc8825->a_cycles) || ((4 == sc8825->a_cycles) && (512 == sc8825->write_size)))
	{
		page_addr >>= 8;
		sprd_sc8825_nand_ins_add(NAND_MC_ADDR(page_addr & 0xff), sc8825);
	}
	sprd_sc8825_nand_ins_add(NAND_MC_CMD(NAND_CMD_ERASE2), sc8825);
	sprd_sc8825_nand_ins_add(NFC_MC_WRB0_ID, sc8825); //wait rb

	sprd_sc8825_nand_ins_add(NFC_MC_DONE_ID, sc8825);
	cfg0 = NFC_WPN | NFC_ONLY_NAND_MODE;
	sprd_sc8825_reg_write(NFC_CFG0_REG, cfg0);
	sprd_sc8825_nand_ins_exec(sc8825);
	sprd_sc8825_nand_wait_finish(sc8825);
}
static u8 sprd_sc8825_read_byte(struct mtd_info *mtd)
{
	u8 ch = 0xff;
	struct sprd_sc8825_nand_info *sc8825 = mtd_to_sc8825(mtd);
	if(sc8825->buf_head < sc8825->_buf_tail)
	{
		ch = s_id_status[sc8825->buf_head ++];
	}
	return ch;
}
static u16 sprd_sc8825_read_word(struct mtd_info *mtd)
{
	u16 data = 0xffff;
	struct sprd_sc8825_nand_info *sc8825 = mtd_to_sc8825(mtd);
	if(sc8825->buf_head < (sc8825->_buf_tail - 1))
	{
		data = s_id_status[sc8825->buf_head ++];
		data |= ((u16)s_id_status[sc8825->buf_head ++]) << 8;
	}
	return data;
}
static int sprd_sc8825_waitfunc(struct mtd_info *mtd, struct nand_chip *chip)
{
	return 0;
}
static int sprd_sc8825_ecc_calculate(struct mtd_info *mtd, const u8 *data,
				u8 *ecc_code)
{
	return 0;
}
static int sprd_sc8825_ecc_correct(struct mtd_info *mtd, u8 *data,
				u8 *read_ecc, u8 *calc_ecc)
{
	return 0;
}
static int sprd_sc8825_read_page(struct mtd_info *mtd, struct nand_chip *chip,
			u8 *buf, int page)
{
	struct sprd_sc8825_nand_info *sc8825 = mtd_to_sc8825(mtd);
	sc8825->page = page;
	if(512 == mtd->writesize)
	{
		sprd_sc8825_nand_read_sp(mtd, buf, chip->oob_poi, 0);
	}
	else
	{
		sprd_sc8825_nand_read_lp(mtd, buf, chip->oob_poi, 0);
	}
	return 0;
}
static int sprd_sc8825_read_page_raw(struct mtd_info *mtd, struct nand_chip *chip,
				u8 *buf, int page)
{
	struct sprd_sc8825_nand_info *sc8825 = mtd_to_sc8825(mtd);
	sc8825->page = page;
	if(512 == mtd->writesize)
	{
		sprd_sc8825_nand_read_sp(mtd, buf, chip->oob_poi, 1);
	}
	else
	{
		sprd_sc8825_nand_read_lp(mtd, buf, chip->oob_poi, 1);
	}
	return 0;
}
static int sprd_sc8825_read_oob(struct mtd_info *mtd, struct nand_chip *chip,
			   int page, int sndcmd)
{
	struct sprd_sc8825_nand_info *sc8825 = mtd_to_sc8825(mtd);
	sc8825->page = page;
	if(512 == mtd->writesize)
	{
		sprd_sc8825_nand_read_sp(mtd, 0, chip->oob_poi, 1);
	}
	else
	{
		sprd_sc8825_nand_read_lp(mtd, 0, chip->oob_poi, 1);
	}
	return 0;
}
static void sprd_sc8825_write_page(struct mtd_info *mtd, struct nand_chip *chip,
				const u8 *buf)
{
	if(512 == mtd->writesize)
	{
		sprd_sc8825_nand_write_sp(mtd, buf, chip->oob_poi, 0);
	}
	else
	{
		sprd_sc8825_nand_write_lp(mtd, buf, chip->oob_poi, 0);
	}

}
static void sprd_sc8825_write_page_raw(struct mtd_info *mtd, struct nand_chip *chip,
					const u8 *buf)
{
	if(512 == mtd->writesize)
	{
		sprd_sc8825_nand_write_sp(mtd, buf, chip->oob_poi, 1);
	}
	else
	{
		sprd_sc8825_nand_write_lp(mtd, buf, chip->oob_poi, 1);
	}
}
static int sprd_sc8825_write_oob(struct mtd_info *mtd, struct nand_chip *chip,
				int page)
{
	struct sprd_sc8825_nand_info *sc8825 = mtd_to_sc8825(mtd);
	sc8825->page = page;
	if(512 == mtd->writesize)
	{
		sprd_sc8825_nand_write_sp(mtd, 0, chip->oob_poi, 1);
	}
	else
	{
		sprd_sc8825_nand_write_lp(mtd, 0, chip->oob_poi, 1);
	}
	return 0;
}
static void sprd_sc8825_nand_cmdfunc(struct mtd_info *mtd, unsigned int command,
			    int column, int page_addr)
{
	struct sprd_sc8825_nand_info *sc8825 = mtd_to_sc8825(mtd);
	/* Emulate NAND_CMD_READOOB */
	if (command == NAND_CMD_READOOB) {
		column += mtd->writesize;
		command = NAND_CMD_READ0;
	}
	/*
	 * program and erase have their own busy handlers
	 * status, sequential in, and deplete1 need no delay
	 */
	switch (command) {
	case NAND_CMD_STATUS:
		sprd_sc8825_nand_read_status(sc8825);
		break;
	case NAND_CMD_READID:
		sprd_sc8825_nand_read_id(sc8825, (u32 *)s_id_status);
		break;
	case NAND_CMD_RESET:
		sprd_sc8825_nand_reset(sc8825);
		break;
	case NAND_CMD_ERASE1:
		sprd_sc8825_erase(mtd, page_addr);
		break;
	case NAND_CMD_READ0:
	case NAND_CMD_SEQIN:
		sc8825->column = column;
		sc8825->page = page_addr;
	default:
		break;
	}
}
static void sprd_sc8825_nand_hwecc_ctl(struct mtd_info *mtd, int mode)
{
	return; //do nothing
}
static void sprd_sc8825_nand_hw_init(struct sprd_sc8825_nand_info *sc8825)
{

	sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL, AHB_CTL0_NFC_EN, AHB_CTL0);
	sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL, AHB_CTL0_NFC_EN, AHB_SOFT_NFC_RST);
	mdelay(1);
	sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL, AHB_CTL0_NFC_EN, AHB_SOFT_NFC_RST);

	sprd_sc8825_reg_write(NFC_TIMING_REG, NFC_DEFAULT_TIMING);
	sprd_sc8825_reg_write(NFC_TIMEOUT_REG, 0x80400000);
	//close write protect
	sprd_sc8825_nand_wp_en(sc8825, 0);
}
int board_nand_init(struct nand_chip *chip)
{
	g_sc8825_nand_info.nand = chip;
	g_sc8825_nand_info.ecc_mode = CONFIG_SYS_NAND_ECC_MODE;

	sprd_sc8825_nand_hw_init(&g_sc8825_nand_info);
	chip->select_chip = sprd_sc8825_select_chip;
	chip->cmdfunc = sprd_sc8825_nand_cmdfunc;
	chip->read_byte = sprd_sc8825_read_byte;
	chip->read_word = sprd_sc8825_read_word;
	chip->waitfunc = sprd_sc8825_waitfunc;
	chip->ecc.mode = NAND_ECC_HW;
	chip->ecc.calculate = sprd_sc8825_ecc_calculate;
	chip->ecc.hwctl = sprd_sc8825_nand_hwecc_ctl;

	chip->ecc.correct = sprd_sc8825_ecc_correct;
	chip->ecc.read_page = sprd_sc8825_read_page;
	chip->ecc.read_page_raw = sprd_sc8825_read_page_raw;
	chip->ecc.write_page = sprd_sc8825_write_page;
	chip->ecc.write_page_raw = sprd_sc8825_write_page_raw;
	chip->ecc.read_oob = sprd_sc8825_read_oob;
	chip->ecc.write_oob = sprd_sc8825_write_oob;
	chip->erase_cmd = sprd_sc8825_erase;

	chip->ecc.bytes = CONFIG_SYS_NAND_ECCBYTES;
	g_sc8825_nand_info.ecc_mode = CONFIG_SYS_NAND_ECC_MODE;
	g_sc8825_nand_info.nand = chip;
	//chip->eccbitmode = g_sc8825_nand_info.ecc_mode;
	chip->ecc.size = CONFIG_SYS_NAND_ECCSIZE;
	chip->chip_delay = 20;
	chip->priv = &g_sc8825_nand_info;

	chip->options |= NAND_BUSWIDTH_16;

	return 0;
}

static struct mtd_info *sprd_mtd = NULL;
#ifdef CONFIG_MTD_CMDLINE_PARTS
const char *part_probes[] = { "cmdlinepart", NULL };
#endif

static int sprd_nand_dma_init(struct sprd_sc8825_nand_info *sc8825)
{
	dma_addr_t phys_addr = 0;
	void *virt_ptr = 0;
	virt_ptr = dma_alloc_coherent(NULL, sc8825->write_size, &phys_addr, GFP_KERNEL);
	if (virt_ptr == NULL) {
		printk(KERN_ERR "NAND - Failed to allocate memory for DMA main buffer\n");
		return -ENOMEM;
	}
	sc8825->v_mbuf = (u32)virt_ptr;
	sc8825->p_mbuf = (u32)phys_addr;

	virt_ptr = dma_alloc_coherent(NULL, sc8825->oob_size, &phys_addr, GFP_KERNEL);
	if (virt_ptr == NULL) {
		printk(KERN_ERR "NAND - Failed to allocate memory for DMA oob buffer\n");
		dma_free_coherent(NULL, sc8825->write_size, (void *)sc8825->v_mbuf, (dma_addr_t)sc8825->p_mbuf);
		return -ENOMEM;
	}
	sc8825->v_oob = (u32)virt_ptr;
	sc8825->p_oob = (u32)phys_addr;
	return 0;
}
static void sprd_nand_dma_deinit(struct sprd_sc8825_nand_info *sc8825)
{
	dma_free_coherent(NULL, sc8825->write_size, (void *)sc8825->v_mbuf, (dma_addr_t)sc8825->p_mbuf);
	dma_free_coherent(NULL, sc8825->write_size, (void *)sc8825->v_oob, (dma_addr_t)sc8825->p_oob);
}
static int sprd_nand_probe(struct platform_device *pdev)
{
	struct nand_chip *this;
	struct resource *regs = NULL;
	struct mtd_partition *partitions = NULL;
	int num_partitions = 0;
	int ret = 0;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs) {
		dev_err(&pdev->dev,"resources unusable\n");
		goto prob_err;
	}

	memset(&g_sc8825_nand_info, 0 , sizeof(g_sc8825_nand_info));

	platform_set_drvdata(pdev, &g_sc8825_nand_info);
	g_sc8825_nand_info.pdev = pdev;

	sprd_mtd = kmalloc(sizeof(struct mtd_info) + sizeof(struct nand_chip), GFP_KERNEL);
	this = (struct nand_chip *)(&sprd_mtd[1]);
	memset((char *)sprd_mtd, 0, sizeof(struct mtd_info));
	memset((char *)this, 0, sizeof(struct nand_chip));

	sprd_mtd->priv = this;

	this->options |= NAND_BUSWIDTH_16;
	this->options |= NAND_NO_READRDY;

	board_nand_init(this);
	//nand_scan(sprd_mtd, 1);
	/* first scan to find the device and get the page size */
	if (nand_scan_ident(sprd_mtd, 1, NULL)) {
		ret = -ENXIO;
		goto prob_err;
	}
	sprd_sc8825_nand_read_id(&g_sc8825_nand_info, (u32 *)s_id_status);
	nand_hardware_config(sprd_mtd, this, s_id_status);
	if(sprd_nand_dma_init(&g_sc8825_nand_info) != 0) {
		return -ENOMEM;
	}

	/* second phase scan */
	if (nand_scan_tail(sprd_mtd)) {
		ret = -ENXIO;
		goto prob_err;
	}

	sprd_mtd->name = "sprd-nand";
	num_partitions = parse_mtd_partitions(sprd_mtd, part_probes, &partitions, 0);

	if ((!partitions) || (num_partitions == 0)) {
		printk("No parititions defined, or unsupported device.\n");
		goto release;
	}
	mtd_device_register(sprd_mtd, partitions, num_partitions);

	return 0;
release:
	nand_release(sprd_mtd);
	sprd_nand_dma_deinit(&g_sc8825_nand_info);
prob_err:
	kfree(sprd_mtd);
	return ret;
}

static int sprd_nand_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);
	nand_release(sprd_mtd);
	sprd_nand_dma_deinit(&g_sc8825_nand_info);
	kfree(sprd_mtd);
	return 0;
}

#ifdef CONFIG_PM
static int sprd_nand_suspend(struct platform_device *dev, pm_message_t pm)
{
	//nothing to do
	return 0;
}

static int sprd_nand_resume(struct platform_device *dev)
{
	sprd_sc8825_reg_write(NFC_TIMING_REG, NFC_DEFAULT_TIMING);
	sprd_sc8825_reg_write(NFC_TIMEOUT_REG, 0x80400000);
	//close write protect
	sprd_sc8825_nand_wp_en(&g_sc8825_nand_info, 0);
	return 0;
}
#else
#define sprd_nand_suspend NULL
#define sprd_nand_resume NULL
#endif

static struct platform_driver sprd_nand_driver = {
	.probe		= sprd_nand_probe,
	.remove		= sprd_nand_remove,
	.suspend	= sprd_nand_suspend,
	.resume		= sprd_nand_resume,
	.driver		= {
		.name	= "sprd-nand",
		.owner	= THIS_MODULE,
	},
};

static int __init sprd_nand_init(void)
{
	return platform_driver_register(&sprd_nand_driver);
}

static void __exit sprd_nand_exit(void)
{
	platform_driver_unregister(&sprd_nand_driver);
}

module_init(sprd_nand_init);
module_exit(sprd_nand_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("spreadtrum.com");
MODULE_DESCRIPTION("SPRD sc8825 MTD NAND driver");
MODULE_ALIAS("platform:sprd-nand");