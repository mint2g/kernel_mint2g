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

#include <asm/io.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <mach/dma.h>
#include <mach/globalregs.h>

/* 0X00 */
#define DMA_CFG                         (DMA_REG_BASE + 0x0000)
#define DMA_CHN_EN_STATUS               (DMA_REG_BASE + 0x0004)
#define DMA_LINKLIST_EN                 (DMA_REG_BASE + 0x0008)
#define DMA_SOFTLINK_EN                 (DMA_REG_BASE + 0x000C)

/* 0X10 */
#define DMA_SOFTLIST_SIZE               (DMA_REG_BASE + 0x0010)
#define DMA_SOFTLIST_CMD                (DMA_REG_BASE + 0x0014)
#define DMA_SOFTLIST_STS                (DMA_REG_BASE + 0x0018)
#define DMA_SOFTLIST_BASEADDR           (DMA_REG_BASE + 0x001C)

/* 0X20 */
#define DMA_PRI_REG0                    (DMA_REG_BASE + 0x0020)
#define DMA_PRI_REG1                    (DMA_REG_BASE + 0x0024)

/* 0X30 */
#define DMA_INT_STS                     (DMA_REG_BASE + 0x0030)
#define DMA_INT_RAW                     (DMA_REG_BASE + 0x0034)

/* 0X40 */
#define DMA_LISTDONE_INT_EN             (DMA_REG_BASE + 0x0040)
#define DMA_BURST_INT_EN                (DMA_REG_BASE + 0x0044)
#define DMA_TRANSF_INT_EN               (DMA_REG_BASE + 0x0048)

/* 0X50 */
#define DMA_LISTDONE_INT_STS            (DMA_REG_BASE + 0x0050)
#define DMA_BURST_INT_STS               (DMA_REG_BASE + 0x0054)
#define DMA_TRANSF_INT_STS              (DMA_REG_BASE + 0x0058)

/* 0X60 */
#define DMA_LISTDONE_INT_RAW            (DMA_REG_BASE + 0x0060)
#define DMA_BURST_INT_RAW               (DMA_REG_BASE + 0x0064)
#define DMA_TRANSF_INT_RAW              (DMA_REG_BASE + 0x0068)

/* 0X70 */
#define DMA_LISTDONE_INT_CLR            (DMA_REG_BASE + 0x0070)
#define DMA_BURST_INT_CLR               (DMA_REG_BASE + 0x0074)
#define DMA_TRANSF_INT_CLR              (DMA_REG_BASE + 0x0078)

/* 0X80 */
#define DMA_SOFT_REQ                    (DMA_REG_BASE + 0x0080)
#define DMA_TRANS_STS                   (DMA_REG_BASE + 0x0084)
#define DMA_REQ_PEND                    (DMA_REG_BASE + 0x0088)

/* 0X90 */
#define DMA_WRAP_START                  (DMA_REG_BASE + 0x0090)
#define DMA_WRAP_END                    (DMA_REG_BASE + 0x0094)

#define DMA_CHN_UID_BASE                (DMA_REG_BASE + 0x0098)
#define DMA_CHN_UID0                    (DMA_REG_BASE + 0x0098)
#define DMA_CHN_UID1                    (DMA_REG_BASE + 0x009C)
#define DMA_CHN_UID2                    (DMA_REG_BASE + 0x00A0)
#define DMA_CHN_UID3                    (DMA_REG_BASE + 0x00A4)
#define DMA_CHN_UID4                    (DMA_REG_BASE + 0x00A8)
#define DMA_CHN_UID5                    (DMA_REG_BASE + 0x00AC)
#define DMA_CHN_UID6                    (DMA_REG_BASE + 0x00B0)
#define DMA_CHN_UID7                    (DMA_REG_BASE + 0x00B4)


#define SPRD_DMA_DPRINTF(mask, format, args...) \
	do { \
		if ((mask) & sprd_dma_print_mask) \
			printk(KERN_ERR format, args); \
	} while (0)

#define PRINT_ERROR(format, args...) \
	SPRD_DMA_DPRINTF(SPRD_DMA_PRINT_ERRORS, format, args);

#define PRINT_IO(format, args...) \
	SPRD_DMA_DPRINTF(SPRD_DMA_PRINT_IO, format, args);

#define PRINT_FLOW(format, args...) \
	SPRD_DMA_DPRINTF(SPRD_DMA_PRINT_FLOW, format, args);

spinlock_t dma_lock;
struct sprd_irq_handler {
	void (*handler)(int dma, void *dev_id);
	void *dev_id;
	u32 dma_uid;
	u32 used;/* mark the channel used before new API done */
};
enum {
	SPRD_DMA_PRINT_ERRORS = 1,
	SPRD_DMA_PRINT_IO = 2,
	SPRD_DMA_PRINT_FLOW = 4
};
unsigned int sprd_dma_print_mask = SPRD_DMA_PRINT_FLOW;
static struct sprd_irq_handler sprd_irq_handlers[DMA_CHN_NUM];

static void dma_channel_start(int dma_chn, int on_off);
static void dma_channel_set_software_req(int dma_chn, int on_off);

static irqreturn_t sprd_dma_irq(int irq, void *dev_id)
{
	u32 irq_status = __raw_readl(DMA_INT_STS);
	while (irq_status) {
		int i = DMA_CHN_MAX - __builtin_clz(irq_status);
		irq_status &= ~(1<<i);

		dma_reg_write(DMA_TRANSF_INT_CLR, i, 1, 1);
		dma_reg_write(DMA_BURST_INT_CLR, i, 1, 1);
		dma_reg_write(DMA_LISTDONE_INT_CLR, i, 1, 1);

		if (sprd_irq_handlers[i].handler){
			sprd_irq_handlers[i].handler(i, sprd_irq_handlers[i].dev_id);
		}else {
			printk(KERN_ERR "DMA channel %d needs handler!\n", i);
		}
	}
	return IRQ_HANDLED;
}

/*
* sprd_dma_channel_int_clr: clear intterrupts of a dma channel
* @chn: dma channle
*/
void sprd_dma_channel_int_clr(u32 chn)
{
	dma_reg_bits_or(1<<chn, DMA_BURST_INT_CLR);
	dma_reg_bits_or(1<<chn, DMA_TRANSF_INT_CLR);
	dma_reg_bits_or(1<<chn, DMA_LISTDONE_INT_CLR);
}
EXPORT_SYMBOL_GPL(sprd_dma_channel_int_clr);

void dma_channel_handlers_init(void)
{
	int i;
	for(i=DMA_CHN_MIN; i<DMA_CHN_NUM; i++){
		sprd_irq_handlers[i].handler = NULL;
		sprd_irq_handlers[i].dev_id = NULL;
		sprd_irq_handlers[i].dma_uid = 0;
		sprd_irq_handlers[i].used = 0;
	}
}

/*
 * check all the dma channels for DEBUG
 *
 */
void sprd_dma_check_channel(void)
{
	int i;
	for(i=DMA_CHN_MIN; i<DMA_CHN_NUM; i++){
		if(sprd_irq_handlers[i].handler == NULL){
			pr_debug("=== dma channel:%d is not occupied ====\n", i);
		}
	}
	for(i=DMA_CHN_MIN; i<DMA_CHN_NUM; i++){
		pr_debug("=== sprd_irq_handlers[%d].handler:%p ====\n",
						i, sprd_irq_handlers[i].handler);
		pr_debug("=== sprd_irq_handlers[%d].dma_uid:%u ====\n",
						i, sprd_irq_handlers[i].dma_uid);
		pr_debug("=== sprd_irq_handlers[%d].used:%u ====\n",
						i, sprd_irq_handlers[i].used);
	}
}
EXPORT_SYMBOL_GPL(sprd_dma_check_channel);


/*
 * dump main dma regs for DEBUG
 *
 */
void sprd_dma_dump_regs(u32 chn_id){
	printk("==== DMA_CFG:0x%x ===\n", __raw_readl(DMA_CFG) );
	printk("==== DMA_CHx_EN_STATUS:0x%x ===\n", __raw_readl(DMA_CHN_EN_STATUS) );
	printk("==== DMA_LINKLIST_EN:0x%x ===\n", __raw_readl(DMA_LINKLIST_EN) );
	printk("==== DMA_SOFTLINK_EN:0x%x ===\n", __raw_readl(DMA_SOFTLINK_EN) );

	printk("==== DMA_CHN_UID0:0x%x ===\n", __raw_readl(DMA_CHN_UID0) );
	printk("==== DMA_CHN_UID1:0x%x ===\n", __raw_readl(DMA_CHN_UID1) );
	printk("==== DMA_CHN_UID2:0x%x ===\n", __raw_readl(DMA_CHN_UID2) );
	printk("==== DMA_CHN_UID3:0x%x ===\n", __raw_readl(DMA_CHN_UID3) );
	printk("==== DMA_CHN_UID4:0x%x ===\n", __raw_readl(DMA_CHN_UID4) );
	printk("==== DMA_CHN_UID5:0x%x ===\n", __raw_readl(DMA_CHN_UID5) );
	printk("==== DMA_CHN_UID6:0x%x ===\n", __raw_readl(DMA_CHN_UID6) );
	printk("==== DMA_CHN_UID7:0x%x ===\n", __raw_readl(DMA_CHN_UID7) );


	printk("==== DMA_INT_STS:0x%x ===\n", __raw_readl(DMA_INT_STS) );
	printk("==== DMA_INT_RAW:0x%x ===\n", __raw_readl(DMA_INT_RAW) );

	printk("==== DMA_LISTDONE_INT_EN:0x%x ===\n", __raw_readl(DMA_LISTDONE_INT_EN) );
	printk("==== DMA_BURST_INT_EN:0x%x ===\n", __raw_readl(DMA_BURST_INT_EN) );
	printk("==== DMA_TRANSF_INT_EN:0x%x ===\n", __raw_readl(DMA_TRANSF_INT_EN) );

	printk("==== DMA_LISTDONE_INT_STS:0x%x ===\n", __raw_readl(DMA_LISTDONE_INT_STS) );
	printk("==== DMA_BURST_INT_STS:0x%x ===\n", __raw_readl(DMA_BURST_INT_STS) );
	printk("==== DMA_TRANSF_INT_STS:0x%x ===\n", __raw_readl(DMA_TRANSF_INT_STS) );

	printk("==== DMA_LISTDONE_INT_RAW:0x%x ===\n", __raw_readl(DMA_LISTDONE_INT_RAW) );
	printk("==== DMA_BURST_INT_RAW:0x%x ===\n", __raw_readl(DMA_BURST_INT_RAW) );
	printk("==== DMA_TRANSF_INT_RAW:0x%x ===\n", __raw_readl(DMA_TRANSF_INT_RAW) );

	printk("==== DMA_LISTDONE_INT_CLR:0x%x ===\n", __raw_readl(DMA_LISTDONE_INT_CLR) );
	printk("==== DMA_BURST_INT_CLR:0x%x ===\n", __raw_readl(DMA_BURST_INT_CLR) );
	printk("==== DMA_TRANSF_INT_CLR:0x%x ===\n", __raw_readl(DMA_TRANSF_INT_CLR) );


	printk("==== DMA_TRANS_STS:0x%x ===\n", __raw_readl(DMA_TRANS_STS) );
	printk("==== DMA_REQ_PEND:0x%x ===\n", __raw_readl(DMA_REQ_PEND) );

	printk("==== DMA_CH%d_CFG0:0x%x ===\n", chn_id, __raw_readl(DMA_CHx_CFG0(chn_id)) );
	printk("==== DMA_CH%d_CFG1:0x%x ===\n", chn_id, __raw_readl(DMA_CHx_CFG1(chn_id)) );
	printk("==== DMA_CH%d_SRC_ADDR:0x%x ===\n", chn_id, __raw_readl(DMA_CHx_SRC_ADDR(chn_id)) );
	printk("==== DMA_CH%d_DEST_ADDR:0x%x ===\n", chn_id, __raw_readl(DMA_CHx_DEST_ADDR(chn_id)) );
	printk("==== DMA_CH%d_LLPTR:0x%x ===\n", chn_id, __raw_readl(DMA_CHx_LLPTR(chn_id)) );
	printk("==== DMA_CH%d_SDEP:0x%x ===\n", chn_id, __raw_readl(DMA_CHx_SDEP(chn_id)) );
	printk("==== DMA_CH%d_SBP:0x%x ===\n", chn_id, __raw_readl(DMA_CHx_SBP(chn_id)) );
	printk("==== DMA_CH%d_DBP:0x%x ===\n", chn_id, __raw_readl(DMA_CHx_DBP(chn_id)) );

}
EXPORT_SYMBOL_GPL(sprd_dma_dump_regs);


/**
 * dma_set_uid: dedicate uid to a dma channel
 * @dma_chn: dma channel id, in range 0~31
 * @dma_uid: dma uid
 **/
void dma_set_uid(u32 dma_chn, u32 dma_uid)
{
	int chn_uid_shift = 0;
	u32 dma_chn_uid_reg = 0;

	if(dma_chn > DMA_CHN_MAX){
		pr_warning("!!!! Invalid DMA Channel: %d !!!!\n", dma_chn);
		return;
	}

	dma_chn_uid_reg = DMA_CHN_UID_BASE + DMA_UID_UNIT*(dma_chn/DMA_UID_UNIT);
	chn_uid_shift = DMA_UID_SHIFT_STP*(dma_chn%DMA_UID_UNIT);

	__raw_writel( (~(DMA_UID_MASK<<chn_uid_shift))&__raw_readl(dma_chn_uid_reg),
									dma_chn_uid_reg);
	dma_uid = dma_uid << chn_uid_shift;
	dma_uid |= __raw_readl(dma_chn_uid_reg);
	__raw_writel(dma_uid, dma_chn_uid_reg);
	pr_debug("**** dma_chn_uid_reg:0x%x, 0x%x ****\n",
					dma_chn_uid_reg, __raw_readl(dma_chn_uid_reg) );

	return;
}


/**
 *    dma_check_channel: check the whole dma channels according to uid, return the one
 * not occupied
 * @uid: dma uid
 * @return: dma channel number
 **/
u32 dma_check_channel(u32 uid)
{
	u32 chn;

	/* we should set uid 0 when memory to memory(software request) */
	if( (uid<DMA_UID_MIN)||(uid>DMA_UID_MAX) ){
		printk("!!!! DMA UID:%u IS Beyond Valid Range %d~%d !!!!\n",
							uid, DMA_UID_MIN, DMA_UID_MAX);
		WARN_ON(1);
		return -1;
	}

	if(uid == DMA_UID_SOFTWARE){
		for(chn=DMA_CHN_MIN; chn<DMA_CHN_NUM; chn++){
			if((sprd_irq_handlers[chn].handler==NULL) &&
				(sprd_irq_handlers[chn].used!=1)) {
				return chn;
			}
		}
	}else{
		/* return the same channel if not freed */
		for(chn=DMA_CHN_MIN+1; chn<DMA_CHN_NUM; chn++){
			if(sprd_irq_handlers[chn].dma_uid==uid)
				return chn;
		}
		for(chn=DMA_CHN_MIN+1; chn<DMA_CHN_NUM; chn++){
			if((sprd_irq_handlers[chn].handler==NULL) &&
				(sprd_irq_handlers[chn].used!=1)) {
				return chn;
			}
		}
	}

	printk(" !!! %s, no more channels left\n", __func__ );
	return -1;
}

/*
* dma_channel_workmode_clr: clear work mode of a dma channel
* @chn: dma channle
*/
void dma_channel_workmode_clr(u32 chn_id)
{
	dma_reg_bits_and(~(1<<chn_id), DMA_LINKLIST_EN);
	dma_reg_bits_and(~(1<<chn_id), DMA_SOFTLINK_EN);
}

/***
* sprd_dma_request: request dma channel resources, return the valid channel number
* @uid: dma uid
* @irq_handler: irq_handler of dma users
* @data: parameter pass to irq_handler
* @return: dma channle id
***/
int sprd_dma_request(u32 uid, void (*irq_handler)(int, void *), void *data)
{
	unsigned long flags;
	int ch_id;
	local_irq_save(flags);
	ch_id = dma_check_channel(uid);
	if(ch_id < DMA_CHN_MIN){
		printk("!!!! DMA UID:%u Is Already Requested !!!!\n", uid);
		local_irq_restore(flags);
		return -1;
	}

	/* init a dma channel handler */
	pr_debug("++++ requested dma channel:%u +++", ch_id);
	sprd_irq_handlers[ch_id].handler = irq_handler;
	sprd_irq_handlers[ch_id].dev_id = data;
	sprd_irq_handlers[ch_id].dma_uid = uid;
	sprd_irq_handlers[ch_id].used = 1;
	local_irq_restore(flags);

	/* init a dma channel configuration */
	dma_channel_start(ch_id, OFF);
	dma_channel_set_software_req(ch_id, OFF);
	sprd_dma_channel_int_clr(ch_id);
	dma_channel_workmode_clr(ch_id);
	dma_set_uid(ch_id, uid);

	return ch_id;
}
EXPORT_SYMBOL_GPL(sprd_dma_request);

/*
 * sprd_dma_free: free the occupied dma channel
 * @chn_id: dma channel occupied
 */
void sprd_dma_free(u32 chn_id)
{
	if(chn_id > DMA_CHN_MAX){
		printk("!!!! dma channel id is out of range:%u~%u !!!!\n",
							DMA_CHN_MIN, DMA_CHN_MAX);
		return;
	}

	/* disable channels*/
	dma_channel_start(chn_id, OFF);
	dma_channel_set_software_req(chn_id, OFF);

	/* clear channels' interrupt */
	sprd_dma_set_irq_type(chn_id, BLOCK_DONE, OFF);
	sprd_dma_set_irq_type(chn_id, TRANSACTION_DONE, OFF);
	sprd_dma_set_irq_type(chn_id, LINKLIST_DONE, OFF);

	/* clear channels' interrupt handler */
	sprd_irq_handlers[chn_id].handler = NULL;
	sprd_irq_handlers[chn_id].dev_id = NULL;
	sprd_irq_handlers[chn_id].dma_uid = DMA_UID_SOFTWARE;/* default UID value */
	sprd_irq_handlers[chn_id].used = 0;/* not occupied */

	/* clear channels' work mode */
	dma_channel_workmode_clr(chn_id);

	/* clear UID, default is DMA_UID_SOFTWARE */
	dma_set_uid(chn_id, DMA_UID_SOFTWARE);/* default UID value */

	return;
}
EXPORT_SYMBOL_GPL(sprd_dma_free);

/*
 * dma_channel_start: start dma transfer
 * @dma_chn: dma channel
 * @on_off:  on or off
 */
static void dma_channel_start(int dma_chn, int on_off)
{
	pr_debug("%s, dma_chn:%d, %d", __func__, dma_chn, on_off);
	switch(on_off){
	case ON:
		dma_reg_bits_and (~(1<<dma_chn), DMA_CHx_DIS);
		dma_reg_bits_or(1<<dma_chn, DMA_CHx_EN);
		break;
	case OFF:
		dma_reg_bits_and (~(1<<dma_chn), DMA_CHx_EN);
		dma_reg_bits_or((1<<dma_chn), DMA_CHx_DIS);
		break;
	default:
		printk("??? dma_channel_start??? what you mean?\n");
	}

	pr_debug("%s, DMA_CHN_EN_STATUS:0x%x", __func__, __raw_readl(DMA_CHN_EN_STATUS) );
	return;
}

static void dma_channel_set_software_req(int dma_chn, int on_off)
{

	if(dma_chn > DMA_CHN_MAX){
		printk("!!!! Invalid DMA Channel: %d !!!!\n", dma_chn);
		return;
	}

	switch(on_off){
	case ON:
		dma_reg_bits_or(1<<dma_chn, DMA_SOFT_REQ);
		break;
	case OFF:
		dma_reg_bits_and (~(1<<dma_chn), DMA_SOFT_REQ);
		break;
	default:
		printk("??? channel:%d, DMA_SOFT_REQ, ON or OFF \n", dma_chn);
	}

	return;
}

/**
 * sprd_dma_channel_start: start one dma channel transfer
 * @chn_id: dma channel id, in range 0 to 31
 **/
void sprd_dma_channel_start(u32 chn_id)
{
	u32 uid;

	if( chn_id > DMA_CHN_MAX ){
		printk("!!! channel id:%u out of range %u~%u !!!\n",
					chn_id, DMA_CHN_MIN, DMA_CHN_MAX);
		return;
	}

	dma_channel_start(chn_id, ON);
	uid = sprd_irq_handlers[chn_id].dma_uid;
	if(uid == DMA_UID_SOFTWARE){
		dma_channel_set_software_req(chn_id, ON);
	}

	return;
}
EXPORT_SYMBOL_GPL(sprd_dma_channel_start);

/**
 * sprd_dma_channel_stop: stop one dma channel transfer
 * @chn_id: dma channel id, in range 0 to 31
 **/
void sprd_dma_channel_stop(u32 chn_id)
{
	u32 uid;

	if( chn_id > DMA_CHN_MAX ){
		printk("!!! channel id:%u out of range %u~%u !!!\n",
					chn_id, DMA_CHN_MIN, DMA_CHN_MAX);
		return;
	}

	dma_channel_start(chn_id, OFF);
	uid = sprd_irq_handlers[chn_id].dma_uid;
	if(uid == DMA_UID_SOFTWARE){
		dma_channel_set_software_req(chn_id, OFF);
	}

	return;
}
EXPORT_SYMBOL_GPL(sprd_dma_channel_stop);

/**
 * sprd_dma_set_irq_type: enable or disable dma interrupt
 * @dma_chn: dma channel id, in range 0 to 31
 * @dma_done_type: BLOCK_DONE, TRANSACTION_DONE or LINKLIST_DONE
 * @on_off: ON:enable interrupt,
 *          OFF:disable interrupt
 **/
void sprd_dma_set_irq_type(u32 dma_chn, dma_done_type irq_type, u32 on_off)
{

	if(dma_chn > DMA_CHN_MAX){
		printk("!!!! Invalid DMA Channel: %d !!!!\n", dma_chn);
		return;
	}

	switch(irq_type){
	case LINKLIST_DONE:
		switch(on_off){
		case ON:
			dma_reg_bits_or(1<<dma_chn, DMA_LISTDONE_INT_EN);
			break;
		case OFF:
			dma_reg_bits_and (~(1<<dma_chn), DMA_LISTDONE_INT_EN);
			break;
		default:
			printk(" LLD_MODE, INT_EN ON OR OFF???\n");
		}
		break;

	case BLOCK_DONE:
		switch(on_off){
		case ON:
			dma_reg_bits_or(1<<dma_chn, DMA_BURST_INT_EN);
			break;
		case OFF:
			dma_reg_bits_and (~(1<<dma_chn), DMA_BURST_INT_EN);
			break;
		default:
			printk(" BURST_MODE, INT_EN ON OR OFF???\n");
		}
		break;

	case TRANSACTION_DONE:
		switch(on_off){
		case ON:
			dma_reg_bits_or(1<<dma_chn, DMA_TRANSF_INT_EN);
			break;
		case OFF:
			dma_reg_bits_and (~(1<<dma_chn), DMA_TRANSF_INT_EN);
			break;
		default:
			printk(" TRANSACTION_MODE, INT_EN ON OR OFF???\n");
		}
		break;

	default:
		printk("??? WHICH IRQ TYPE DID YOU SELECT ???\n");
	}
}
EXPORT_SYMBOL_GPL(sprd_dma_set_irq_type);

/**
 * sprd_dma_set_chn_pri: set dma channel priority
 * @chn: dma channel
 * @pri: channel priority, in range lowest 0 to highest 3,
 */
void sprd_dma_set_chn_pri(u32 chn,  u32 pri)
{
	u32 shift;
	u32 reg_val;
	if( chn > DMA_CHN_MAX ){
		printk("!!! channel id:%u out of range %u~%u !!!\n",
					chn, DMA_CHN_MIN, DMA_CHN_MAX);
		return;
	}
	if( pri > DMA_MAX_PRI ){
		printk("!!! channel priority:%u out of range %d~%d !!!!\n",
						pri, DMA_MIN_PRI, DMA_MAX_PRI);
		return;
	}

	shift = chn%16;
	switch(chn/16){
	case 0:
		reg_val = dma_get_reg(DMA_PRI_REG0);
		reg_val &= ~(DMA_MAX_PRI<<(2*shift));
		reg_val |= pri<<(2*shift);
		dma_set_reg(reg_val, DMA_PRI_REG0);
		break;
	case 1:
		reg_val = dma_get_reg(DMA_PRI_REG1);
		reg_val &= ~(DMA_MAX_PRI<<(2*shift));
		reg_val |= pri<<(2*shift);
		dma_set_reg(reg_val, DMA_PRI_REG1);
		break;
	default:
		printk("!!!! WOW, WOW, WOW, chn:%u, pri%u !!!\n", chn, pri);
	}
	return;
}
EXPORT_SYMBOL_GPL(sprd_dma_set_chn_pri);

/**
 * sprd_dma_chn_cfg_update: configurate a dma channel
 * @chn: dma channel id
 * @desc: dma channel configuration descriptor
 */
void sprd_dma_chn_cfg_update(u32 chn, struct sprd_dma_channel_desc *desc)
{
	u32 chn_cfg = 0;
	u32 chn_elem_postm = 0;
	u32 chn_src_blk_postm = 0;
	u32 chn_dst_blk_postm = 0;

	chn_cfg |= ( desc->cfg_swt_mode_sel      |
			desc->cfg_src_data_width |
			desc->cfg_dst_data_width |
			desc->cfg_req_mode_sel   |
			desc->cfg_src_wrap_en    |
			desc->cfg_dst_wrap_en    |
			desc->cfg_no_auto_close  |
			(desc->cfg_blk_len&CFG_BLK_LEN_MASK)
		);
	chn_elem_postm = ((desc->src_elem_postm & SRC_ELEM_POSTM_MASK)<<SRC_ELEM_POSTM_SHIFT) |
		(desc->dst_elem_postm & DST_ELEM_POSTM_MASK);
	chn_src_blk_postm = (desc->src_burst_mode)|(desc->src_blk_postm & SRC_BLK_POSTM_MASK);
	chn_dst_blk_postm = (desc->dst_burst_mode)|(desc->dst_blk_postm & DST_BLK_POSTM_MASK);

	dma_set_reg(chn_cfg, DMA_CHx_CFG0(chn) );
	dma_set_reg(desc->total_len, DMA_CHx_CFG1(chn) );
	dma_set_reg(desc->src_addr, DMA_CHx_SRC_ADDR(chn) );
	dma_set_reg(desc->dst_addr, DMA_CHx_DEST_ADDR(chn) );
	dma_set_reg(desc->llist_ptr, DMA_CHx_LLPTR(chn) );
	dma_set_reg(chn_elem_postm, DMA_CHx_SDEP(chn) );
	dma_set_reg(chn_src_blk_postm, DMA_CHx_SBP(chn) );
	dma_set_reg(chn_dst_blk_postm, DMA_CHx_DBP(chn) );
}
EXPORT_SYMBOL_GPL(sprd_dma_chn_cfg_update);

/**
 * sprd_dma_channel_config: configurate dma channel
 * @chn: dma channel
 * @work_mode: dma work mode, normal mode as default
 * @dma_cfg: dma channel configuration descriptor
 **/
void sprd_dma_channel_config(u32 chn, dma_work_mode work_mode,
				struct sprd_dma_channel_desc *dma_cfg)
{
	switch(work_mode){
	case DMA_NORMAL:
		break;
	case DMA_LINKLIST:
		dma_reg_bits_and(~(1<<chn), DMA_SOFTLINK_EN);
		dma_reg_bits_or(1<<chn, DMA_LINKLIST_EN);
		break;
	case DMA_SOFTLIST:
		dma_reg_bits_and(~(1<<chn), DMA_LINKLIST_EN);
		dma_reg_bits_or(1<<chn, DMA_SOFTLINK_EN);
		break;
	default:
		printk("???? Unsupported Work Mode You Seleced ????\n");
		return;
	}
	sprd_dma_chn_cfg_update(chn, dma_cfg);
}
EXPORT_SYMBOL_GPL(sprd_dma_channel_config);

/**
 * sprd_dma_softlist_config: configurate dma softlist mode
 * @softlist_desc: dma softlist mode  configuration descriptor
 **/
void sprd_dma_softlist_config(struct sprd_dma_softlist_desc *softlist_desc)
{
	u32 dma_softlist_sts;
	if(softlist_desc){
		dma_softlist_sts=((softlist_desc->softlist_req_ptr&SOFTLIST_REQ_PTR_MASK)<<SOFTLIST_REQ_PTR_SHIFT) |
			(softlist_desc->softlist_cnt & SOFTLIST_CNT_MASK);

		dma_set_reg(softlist_desc->softlist_base_addr, DMA_SOFTLIST_BASEADDR);
		dma_set_reg(softlist_desc->softlist_size, DMA_SOFTLIST_SIZE);
		dma_set_reg(softlist_desc->softlist_cnt_incr, DMA_SOFTLIST_CMD);
		dma_set_reg(dma_softlist_sts, DMA_SOFTLIST_STS);
		dma_set_reg(softlist_desc->softlist_base_addr, DMA_SOFTLIST_BASEADDR);
	}
	return;
}
EXPORT_SYMBOL_GPL(sprd_dma_softlist_config);

/**
 * sprd_dma_softlist_config: configurate dma wrap address
 * @wrap_addr: dma wrap address descriptor
 **/
void sprd_dma_wrap_addr_config(struct sprd_dma_wrap_addr *wrap_addr)
{
	if(wrap_addr){
		dma_set_reg(wrap_addr->wrap_start_addr, DMA_WRAP_START);
		dma_set_reg(wrap_addr->wrap_end_addr, DMA_WRAP_END);
	}
	return;
}
EXPORT_SYMBOL_GPL(sprd_dma_wrap_addr_config);

/**
 * sprd_dma_irq_handler_ready: check the irq handler
 * @ch_id: dma channel id
 * @return: irq handler ready(return 1)
 **/
int sprd_dma_irq_handler_ready(u32 ch_id)
{
	return sprd_irq_handlers[ch_id].handler != NULL;
}
EXPORT_SYMBOL_GPL(sprd_dma_irq_handler_ready);


static int sprd_dma_init(void)
{
	int ret;

	sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL, AHB_CTL0_DMA_EN, AHB_CTL0);/* DMA ENABLE */
	__raw_writel(0, DMA_CHx_EN);
	__raw_writel(0, DMA_LINKLIST_EN);
	__raw_writel(0, DMA_SOFT_REQ);
	__raw_writel(0, DMA_PRI_REG0);
	__raw_writel(0, DMA_PRI_REG1);
	__raw_writel(-1,DMA_LISTDONE_INT_CLR);
	__raw_writel(-1,DMA_TRANSF_INT_CLR);
	__raw_writel(-1,DMA_BURST_INT_CLR);
	__raw_writel(0, DMA_LISTDONE_INT_EN);
	__raw_writel(0, DMA_TRANSF_INT_EN);
	__raw_writel(0, DMA_BURST_INT_EN);

	/*set hard/soft burst wait time*/
	dma_reg_write(DMA_CFG, 0, DMA_HARD_WAITTIME, 0xff);
	dma_reg_write(DMA_CFG,16, DMA_SOFT_WAITTIME, 0xffff);

	/*register dma irq handle to host*/
	ret = request_irq(IRQ_DMA_INT, sprd_dma_irq, 0, "sprd-dma", NULL);
	if (ret == 0)
		printk(KERN_INFO "request dma irq ok\n");
	else
		printk(KERN_ERR "request dma irq failed %d\n", ret);

	/* initialize the sprd_irq_handlers */
	dma_channel_handlers_init( );

	return ret;
}

arch_initcall(sprd_dma_init);

MODULE_DESCRIPTION("SPRD DMA Module");
MODULE_LICENSE("GPL");
