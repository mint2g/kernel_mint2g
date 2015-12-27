#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/device.h>

#include "sprd_busmonitor_core.h"

#define SPRD_BM_CORE "sprd_busmonitor_core"

static struct class *sprd_bm_cls;
static dev_t sprd_bm_major;

static struct sprd_bm_chip *sprd_bm_set[MAX_DEV_NUM];

static int sprd_bm_core_open(struct inode *inode, struct file *fp)
{
	u8 minor;
	struct sprd_bm_chip *chip;

	minor = MINOR(inode->i_rdev);
	chip = sprd_bm_set[minor];
	fp->private_data = chip;

	init_waitqueue_head(&chip->wait_queue);

	if (chip->open) {
		return chip->open(chip);
	}

	return 0;
}

static int sprd_bm_core_release(struct inode *inode, struct file *fp)
{
	struct sprd_bm_chip *chip;

	chip = (struct sprd_bm_chip *)fp->private_data;

	if (chip->release)
		return chip->release(chip);

	return 0;
}

static ssize_t sprd_bm_core_read(struct file *fp, char __user *buf, size_t size, loff_t *offset)
{
	struct sprd_bm_chip *chip;

	chip = (struct sprd_bm_chip *)fp->private_data;
	if (chip->read) {
	}

	return 0;
}

static ssize_t sprd_bm_core_write(struct file *fp, const char __user *buf, size_t size, loff_t *offset)
{
	struct sprd_bm_chip *chip;

	chip = (struct sprd_bm_chip *)fp->private_data;
	if (chip->write) {
	}

	return 0;
}

static long sprd_bm_core_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	struct sprd_bm_chip *chip;
	volatile struct sprd_bm_reg *reg;

	chip = (struct sprd_bm_chip *)fp->private_data;
	reg = (struct sprd_bm_reg *)(chip->reg_base);

	switch (cmd) {
	case SPRD_READ_CONFIG:
		break;

	case SPRD_POINT_CONFIG:
		if (arg)
			chip->point_config(chip,  (struct sprd_bm_setting *)arg);
		break;

	case SPRD_POINT_ENABLE:
		chip->point_enable(chip);
		break;

	case SPRD_POINT_DISABLE:
		chip->point_disable(chip);
		break;

	/* non blocking function */
	case SPRD_READ_MATCH_DATA:
		if (arg && chip->is_irq_occur) {
			memcpy((void *)arg, &chip->match_data, sizeof(struct sprd_bm_match_data));
		} else {
			memset((void *)arg, 0x0, sizeof(struct sprd_bm_match_data));
		}
		break;

	/* blocking function */
	case SPRD_WAIT_INTERRUPT:
		wait_event_interruptible(chip->wait_queue, chip->is_irq_occur);
		break;

	case SPRD_REGISTER_CALLBACK:
		chip->call_back = (int (*)(struct sprd_bm_chip *))arg;
		break;

	default:
		break;
	}

	return 0;
}

int sprd_bm_register(struct sprd_bm_chip *chip)
{
	u8 minor;
	dev_t devt;
	struct device *dev;

	/* fixme, need to check */
	if (!chip->open) {
	}

	for (minor = 0; minor < MAX_DEV_NUM; minor++) {
		if (!sprd_bm_set[minor])
			break;
	}

	if (MAX_DEV_NUM == minor)
		return -EBUSY;

	devt = MKDEV(sprd_bm_major, minor);

	dev = device_create(sprd_bm_cls, NULL, devt, NULL, "%s-%d", chip->name, chip->bm_id);
	if (IS_ERR(dev)) {
		printk("device create failed!\n");
		return PTR_ERR(dev);
	}

	printk("create device file %s-%d\n", chip->name, chip->bm_id);

	chip->dev = dev;
	sprd_bm_set[minor] = chip;

	return 0;
}
EXPORT_SYMBOL_GPL(sprd_bm_register);

int sprd_bm_unregister(struct sprd_bm_chip *chip)
{
	device_destroy(sprd_bm_cls, chip->dev->devt);

	sprd_bm_set[MINOR(chip->dev->devt)] = NULL;

	return 0;
}
EXPORT_SYMBOL_GPL(sprd_bm_unregister);

static const struct file_operations sprd_bm_ops = {
	.open    = sprd_bm_core_open,
	.release = sprd_bm_core_release,
	.read    = sprd_bm_core_read,
	.write   = sprd_bm_core_write,
	.unlocked_ioctl = sprd_bm_core_ioctl,
};

static int __init sprd_bm_core_init(void)
{
	int ret, major;

	sprd_bm_cls = class_create(THIS_MODULE, SPRD_BM_CORE);
	if (IS_ERR(sprd_bm_cls)) {
		return -EBUSY;
	}

	major = register_chrdev(0, SPRD_BM_CORE, &sprd_bm_ops);
	if (major < 0) {
		ret = -EBUSY;
		goto err;
	}

	sprd_bm_major = (u8)major;

	return 0;

err:
	class_destroy(sprd_bm_cls);
	return ret;
}

static void __exit sprd_bm_core_exit(void)
{
	unregister_chrdev(sprd_bm_major, SPRD_BM_CORE);

	class_destroy(sprd_bm_cls);
}

module_init(sprd_bm_core_init);
module_exit(sprd_bm_core_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("jack.jiang<jack.jiang@apreadtrum.com>");
MODULE_DESCRIPTION("spreadtrum platform busmonitor core");
