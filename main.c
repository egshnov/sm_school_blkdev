// SPDX-License-Identifier: GPL-2.0-only

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/blkdev.h>

#define BLKDEV_NAME "bdevm"
#define GD_NAME "blkm1"
#define mode (FMODE_READ | FMODE_WRITE | FMODE_EXCL)

static struct blkmr_device_maintainer {
	struct block_device *bdev;
	struct gendisk *gd;
	struct bio_set *pool;
	char *last_bdev_path;
	int major;

} maintainer;

/* seems like having maintainer as a global variable makes having such a struct defined kind of redundant
   since we do not pass it to any function as a pointer but overhead is small and I believe that it makes code much more readable.
   Maybe making it a local pointer is a better option but i'm not sure (TODO: ask during code review) */

static void blkmr_submit_bio(struct bio *bio)
{
	struct bio *new_bio;

	new_bio = bio_alloc_clone(maintainer.bdev, bio, GFP_KERNEL,
				  maintainer.pool);
	if (!bio)
		goto interrupt;

	bio_chain(new_bio, bio);
	submit_bio(new_bio);
	return;

interrupt:
	bio_io_error(bio);
}

static const struct block_device_operations blkmr_bio_ops = {
	.owner = THIS_MODULE,
	.submit_bio = blkmr_submit_bio,
};

static int blkmr_set_maintainer_gendisk(void)
{
	int err;

	maintainer.gd = blk_alloc_disk(NUMA_NO_NODE);
	if (!maintainer.gd)
		goto no_mem;

	maintainer.gd->major = maintainer.major;
	maintainer.gd->first_minor = 1;
	maintainer.gd->minors = 1;
	maintainer.gd->fops = &blkmr_bio_ops;
	maintainer.gd->private_data = &maintainer;
	maintainer.gd->part0 = maintainer.bdev;
	//maintainer.gd->flags |= GENHD_FL_NO_PART;

	strcpy(maintainer.gd->disk_name, GD_NAME);
	set_capacity(maintainer.gd, get_capacity(maintainer.bdev->bd_disk));

	err = add_disk(maintainer.gd);
	if (err)
		goto disk_err;

	return 0;

no_mem:
	pr_err("Couldn't alloc gendisk\n");
	return -ENOMEM;

disk_err:
	pr_err("Couldn't add gendisk %d\n", err);
	put_disk(maintainer.gd);
	maintainer.gd = NULL;
	return err;
}

static int __init blkmr_init(void)
{
	int err;

	maintainer.pool = kzalloc(sizeof(struct bio_set), GFP_KERNEL);
	if (!maintainer.pool)
		return -ENOMEM;

	err = bioset_init(maintainer.pool, BIO_POOL_SIZE, 0, BIOSET_NEED_BVECS);
	if (err)
		goto interrupt_on_pool_init;

	maintainer.major = register_blkdev(0, BLKDEV_NAME);
	if (maintainer.major < 0)
		goto interrupt_on_major;

	pr_info("blkm init\n");
	return 0;

interrupt_on_pool_init:
	kfree(maintainer.pool);
	return err;
interrupt_on_major:
	pr_err("Unable to register block device\n");
	return -EBUSY;
}

static void blkmr_clear_maintainer(void)
{
	if (maintainer.last_bdev_path) {
		blkdev_put(maintainer.bdev, mode);
		maintainer.bdev = NULL;
		kfree(maintainer.last_bdev_path);
		maintainer.last_bdev_path = NULL;
	}

	if (maintainer.gd) {
		del_gendisk(maintainer.gd);
		put_disk(maintainer.gd);
		maintainer.gd = NULL;
	}
}

static void __exit blkmr_exit(void)
{
	blkmr_clear_maintainer();
	bioset_exit(maintainer.pool);
	unregister_blkdev(maintainer.major, BLKDEV_NAME);
	pr_info("blkm exit\n");
}
//TODO: parse with \n
static int blkmr_pipe_add(const char *arg, const struct kernel_param *kp)
{
	char *actual_name;
	ssize_t len;
	int err;

	if (maintainer.last_bdev_path) {
		pr_err("Another device is already opened, please remove previous before opening new one.\n");
		return -EBUSY;
	}

	len = strlen(arg) + 1;

	maintainer.last_bdev_path = kzalloc(sizeof(char) * len, GFP_KERNEL);

	if (!maintainer.last_bdev_path)
		goto no_mem_for_name;

	strcpy(maintainer.last_bdev_path, arg);

	actual_name = kzalloc(sizeof(char) * (len - 1), GFP_KERNEL);

	if (!actual_name)
		goto no_mem_for_name;

	strncpy(actual_name, maintainer.last_bdev_path, len - 2);
	maintainer.bdev = blkdev_get_by_path(actual_name, mode, THIS_MODULE);
	kfree(actual_name);

	if (IS_ERR(maintainer.bdev))
		goto incorrect_path;

	err = blkmr_set_maintainer_gendisk();
	if (err)
		goto gendisk_fail;

	pr_info("device instantiated successfully\n");
	return 0;

no_mem_for_name:
	pr_err("Cannot allocate space to read device name\n");
	kfree(maintainer.last_bdev_path);
	return -ENOMEM;

incorrect_path:
	pr_err("Opening device failed, err: %d\n", PTR_ERR(maintainer.bdev));

	kfree(maintainer.last_bdev_path);
	maintainer.last_bdev_path = NULL;
	maintainer.bdev = NULL;
	return -ENODEV;

gendisk_fail:
	pr_err("Couldn't create disk");
	blkmr_clear_maintainer();
	return err;
}

static int blkmr_pipe_get_name(char *buf, const struct kernel_param *kp)
{
	ssize_t len;

	if (!maintainer.last_bdev_path)
		goto no_dev;

	len = strlen(maintainer.last_bdev_path);
	strcpy(buf, maintainer.last_bdev_path);
	return len;

no_dev:
	pr_err("No opened device\n");
	return -ENODEV;
}

static const struct kernel_param_ops blkmr_name_ops = {
	.set = blkmr_pipe_add,
	.get = blkmr_pipe_get_name,
};

static int blkmr_pipe_rm(const char *arg, const struct kernel_param *kp)
{
	if (!maintainer.last_bdev_path)
		goto no_dev;

	blkmr_clear_maintainer();
	pr_info("device removed\n");
	return 0;

no_dev:
	pr_err("No device to remove\n");
	return -ENODEV;
}

static const struct kernel_param_ops blkmr_rm = {
	.set = blkmr_pipe_rm,
	.get = NULL,
};

MODULE_PARM_DESC(device_pipe, "Device pipe");
module_param_cb(device_pipe, &blkmr_name_ops, NULL, S_IRUGO | S_IWUSR);

MODULE_PARM_DESC(rm_device, "Remove device");
module_param_cb(rm_device, &blkmr_rm, NULL, S_IWUSR);

module_init(blkmr_init);
module_exit(blkmr_exit);

MODULE_AUTHOR("Egor Shalashnov <shalasheg@gmail.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simple I/O redirection driver");
