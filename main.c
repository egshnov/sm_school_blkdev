// SPDX-License-Identifier: GPL-2.0-only
/*
 * some information here
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/blkdev.h>
static char *blk_name;

static int __init blk_init(void)
{
	pr_info("blkm init\n");

	return 0;
}

static void __exit blk_exit(void)
{
	kfree(blk_name);

	pr_info("blkm exit\n");
}

static int blkm_name_set(const char *arg, const struct kernel_param *kp)
{
	ssize_t len = strlen(arg) + 1;

	if (blk_name)
	{
		kfree(blk_name);
		blk_name = NULL;
	}

	blk_name = kzalloc(sizeof(char) * len, GFP_KERNEL);
	if (!blk_name)
		return -ENOMEM;
	strcpy(blk_name, arg);

	return 0;
}

// redundant?
static int blkm_name_get(char *buf, const struct kernel_param *kp)
{
	ssize_t len;

	if (!blk_name)
		return -EINVAL;
	len = strlen(blk_name);

	strcpy(buf, blk_name);

	return len;
}

static const struct kernel_param_ops blkm_name_ops = {
	.set = blkm_name_set,
	.get = blkm_name_get, // redunanat???
};

static const struct kernel_param_ops blkm_rm = {
	.set = hwm_say_hello,
	.get = NULL,
};

//операция нахождения устройства по имени и его открытия
MODULE_PARM_DESC(device_name, "Device name");
module_param_cb(device_name, &blkm_name_ops, NULL, S_IRUGO | S_IWUSR);

//удаление устройства
MODULE_PARM_DESC(rm_device, "Remove device");
module_param_cb(rm_device, hwm_hello_times, NULL, S_IRUGO | S_IWUSR);

module_init(blkm_init);
module_exit(blkm_exit);

MODULE_AUTHOR("Egor Shalashnov <shalalsheg@gmail.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("blkm opening module");
