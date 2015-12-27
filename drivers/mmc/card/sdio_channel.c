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

#include <linux/mmc/sdio_channel.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/mmc.h>
#include "../core/sdio_ops.h"
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/slab.h>
#include <linux/delay.h>

#define DRV_NAME "sprd-sdio-channel"
#define SPRD_SDIO_SLAVE_BLOCK_SIZE 512

struct sdio_channel {
    unsigned int open_count;
    struct sdio_func *func;
    struct mutex func_lock;
};

static struct sdio_channel sprd_sdio_channel_body;
static struct sdio_channel *sprd_sdio_channel = &sprd_sdio_channel_body;

static const struct sdio_device_id sprd_sdio_channel_table[] = {
    { SDIO_DEVICE_CLASS(SDIO_CLASS_NONE) },
    { 0 },
};

static int sprd_sdio_channel_do_tx(const char *buf, unsigned int len) {
    int retval;
    struct sdio_func *func;
    if(buf == NULL)
        return -EINVAL;
    if( len == 0)
        return 0;
    func = sprd_sdio_channel->func;
    sdio_claim_host(func);
    retval =  sdio_memcpy_toio(func, 0, buf, len);
    sdio_release_host(func);
    pr_debug("%s retval: %d\n", __func__, retval);
    return retval;
}

static int sprd_sdio_channel_do_rx(char *buf, unsigned int len) {
    int retval;
    struct sdio_func *func;
    if(buf == NULL)
        return -EINVAL;
    if( len == 0)
        return 0;
    func = sprd_sdio_channel->func;
    sdio_claim_host(func);
    retval = sdio_memcpy_fromio(func, buf, 0, len);
    sdio_release_host(func);
    pr_debug("%s retval: %d\n", __func__, retval);
    return retval;
}

int sprd_sdio_channel_open(void) {
    int retry = 100;
    while(!sprd_sdio_channel->func && retry--) {
	     msleep (100);
           }
    if (retry <= 0) {
        printk(KERN_ERR "failed to %s()\n", __func__);
        return -ENODEV;
    }
    pr_debug("%s done\n", __func__);
    return 0;
}
EXPORT_SYMBOL(sprd_sdio_channel_open);

void sprd_sdio_channel_close(void) {
}
EXPORT_SYMBOL(sprd_sdio_channel_close);

int sprd_sdio_channel_tx(const char *buf, unsigned int len) {
    int retval = -ENODEV;
    if(sprd_sdio_channel->func) {
        mutex_lock(&sprd_sdio_channel->func_lock);
        if(sprd_sdio_channel->func) {
            retval = sprd_sdio_channel_do_tx(buf, len);
        }
        mutex_unlock(&sprd_sdio_channel->func_lock);
    }
    return retval;
}
EXPORT_SYMBOL(sprd_sdio_channel_tx);

int sprd_sdio_channel_rx(char *buf, unsigned int len) {
    int retval = -ENODEV;
    if(sprd_sdio_channel->func) {
        mutex_lock(&sprd_sdio_channel->func_lock);
        if(sprd_sdio_channel->func) {
            retval = sprd_sdio_channel_do_rx(buf, len);
        }
        mutex_unlock(&sprd_sdio_channel->func_lock);
    }
    return retval;
}
EXPORT_SYMBOL(sprd_sdio_channel_rx);

static int sprd_sdio_channel_probe(struct sdio_func *func, const struct sdio_device_id *id) {
    int retval;
    struct sdio_channel *channel;
    printk(KERN_INFO "%s enter\n", __func__);
    channel = sprd_sdio_channel;
    mutex_init(&channel->func_lock);
    sdio_claim_host(func);
    if((retval = sdio_set_block_size(func, SPRD_SDIO_SLAVE_BLOCK_SIZE))) {
        goto ERR_HOST;
    }
    if((retval = sdio_enable_func(func))) {
        goto ERR_HOST;
    }
    channel->func = func;
    printk(KERN_INFO "%s done\n", __func__);
ERR_HOST:
    sdio_release_host(func);
    return retval;
}

static void sprd_sdio_channel_remove(struct sdio_func *func) {
    printk(KERN_INFO "%s enter\n", __func__);
    mutex_lock(&sprd_sdio_channel->func_lock);
    sprd_sdio_channel->func = 0;
    sdio_claim_host(func);
    sdio_disable_func(func);
    sdio_release_host(func);
    mutex_unlock(&sprd_sdio_channel->func_lock);
    printk(KERN_INFO "%s done\n", __func__);
}

static int sprd_sdio_channel_suspend(struct device *dev) {
    mutex_lock(&sprd_sdio_channel->func_lock);
    return 0;
}

static int sprd_sdio_channel_resume(struct device *dev) {
    mutex_unlock(&sprd_sdio_channel->func_lock);
    return 0;
}

static const struct dev_pm_ops sprd_sdio_channel_dev_pm_ops = {
    .suspend = sprd_sdio_channel_suspend,
    .resume  = sprd_sdio_channel_resume,
};

static struct sdio_driver sprd_sdio_channel_driver = {
    .name     = DRV_NAME,
    .probe     = sprd_sdio_channel_probe,
    .remove  = sprd_sdio_channel_remove,
    .id_table  = sprd_sdio_channel_table,
    .drv         = {
        .pm = &sprd_sdio_channel_dev_pm_ops,
    },
};

static int __init sprd_sdio_channel_init(void) {
    return sdio_register_driver(&sprd_sdio_channel_driver);
}

static void __exit sprd_sdio_channel_exit(void) {
    sdio_unregister_driver(&sprd_sdio_channel_driver);
}

module_init(sprd_sdio_channel_init);
module_exit(sprd_sdio_channel_exit);

MODULE_DESCRIPTION("Spredtrum SDHCI Function Slave");
MODULE_LICENSE("GPL");
