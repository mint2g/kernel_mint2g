#ifndef __SPRD_BUSMONITOR_H__
#define __SPRD_BUSMONITOR_H__

#include <linux/sched.h>


#define MAX_DEV_NUM 16
#define NAME_SIZE	32

enum {
	AHB_BM,
	AXI_BM,
};

enum SPRD_BM_COMMAND {
	/* fix me */
	SPRD_READ_CONFIG = 10,
	SPRD_POINT_CONFIG,
	SPRD_POINT_ENABLE,
	SPRD_POINT_DISABLE,
	SPRD_READ_MATCH_DATA,
	SPRD_WAIT_INTERRUPT,
	SPRD_REGISTER_CALLBACK,
};

struct sprd_bm_setting {
	int bm_id;
	int bm_chn;
	u32 bm_cfg;
	u32 addr_min;
	u32 addr_max;
	u32 addr_mask;
	u32 data_min;
	u32 data_min_h32;
	u32 data_max;
	u32 data_max_h32;
	u32 data_mask;
	u32 data_mask_h32;
};

struct sprd_bm_match_data {
	u32 match_addr;
	u32 match_data;
	u32 match_data_h32;
	u32 count;
};

struct sprd_bm_chip {
	/* property */
	const char *name;
	int bm_id;
	int bm_type;
	void *__iomem reg_base;
	unsigned int irq_num;
	struct device *dev;
	int is_irq_occur;
	spinlock_t lock;
	struct sprd_bm_match_data match_data;
	wait_queue_head_t wait_queue;
	/*  function */
	int (*open)(const struct sprd_bm_chip *);
	int(*release)(const struct sprd_bm_chip *);
	int (*read)(const struct sprd_bm_chip *);
	int (*write)(const struct sprd_bm_chip *);
	int (*point_config)(const struct sprd_bm_chip *,  const struct sprd_bm_setting *);
	void (*point_enable)(const struct sprd_bm_chip *);
	void (*point_disable)(const struct sprd_bm_chip *);
	int (*point_get_cnt)(const struct sprd_bm_chip *);
	/* when the irq occur, you can do what you want to do */
	int (*call_back)(struct sprd_bm_chip *);
	/* everything you want to do */
	void *prvdata;
};

int sprd_bm_register(struct sprd_bm_chip *);
int sprd_bm_unregister(struct sprd_bm_chip *);

#endif
