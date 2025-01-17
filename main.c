// SPDX-License-Identifier: GPL-2.0-only

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/blkdev.h>
#include <linux/blk_types.h>
#include "memtable.h"

#define BLKDEV_NAME "bdevm"
#define GD_NAME "blkm1"
#define mode (FMODE_READ | FMODE_WRITE | FMODE_EXCL)
#define CONST_REQ_SIZE 4096

static struct blkmr_device_maintainer {
	struct block_device *bdev;
	struct gendisk *gd;
	struct bio_set *pool;
	char *last_bdev_path;
	int major;
	struct lsm_memtable *memtable;
	sector_t head;

} maintainer;

static int sequential_write(struct bvec_iter *iter, unsigned int step)
{
	int overwritten_bytes;

	overwritten_bytes = lsm_memtable_add(maintainer.memtable,
					     iter->bi_sector, maintainer.head);
	if (overwritten_bytes < 0)
		goto no_mem;

	iter->bi_sector = maintainer.head;
	maintainer.head += step;
	maintainer.head %= maintainer.bdev->bd_nr_sectors;
	return overwritten_bytes < 0;

no_mem:
	pr_err("COULDN'T ALLOCATE MTB_NODE\n");
	return -ENOMEM;
}

static int sequential_read(struct bvec_iter *iter, unsigned int step)
{
	struct mtb_node *target;

	target = lsm_memtable_get(maintainer.memtable, iter->bi_sector);
	if (target) {
		iter->bi_sector = target->physical_addr;
	} else {
		iter->bi_sector = maintainer.head;
		maintainer.head += step;
		maintainer.head %= maintainer.bdev->bd_nr_sectors;
	}
	return 0;
}

static void blkmr_bio_end_io(struct bio *bio)
{
	bio_endio(bio->bi_private);
	bio_put(bio);
}

static void blkmr_submit_bio(struct bio *bio)
{
	struct bio *new_bio;
	int dir, err;
	unsigned int len;

	new_bio = bio_alloc_clone(maintainer.bdev, bio, GFP_KERNEL,
				  maintainer.pool);
	if (!new_bio)
		goto interrupt;

	new_bio->bi_private = bio;
	new_bio->bi_end_io = blkmr_bio_end_io;
	dir = bio_data_dir(bio);
	err = 0;
	len = CONST_REQ_SIZE >> SECTOR_SHIFT;
	if (dir == WRITE)
		err = sequential_write(&(new_bio->bi_iter), len);
	else
		err = sequential_read(&(new_bio->bi_iter), len);

	if (err)
		goto interrupt;

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
	maintainer.gd->flags |= GENHD_FL_NO_PART;

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
		goto interrupt;

	maintainer.major = register_blkdev(0, BLKDEV_NAME);
	if (maintainer.major < 0) {
		err = -EBUSY;
		goto interrupt;
	}
	pr_info("blkm init\n");
	return 0;

interrupt:
	kfree(maintainer.pool);
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

	if (maintainer.memtable) {
		lsm_free_memtable(maintainer.memtable);
		maintainer.memtable = NULL;
	}
	maintainer.head = 0;
}

static void __exit blkmr_exit(void)
{
	blkmr_clear_maintainer();
	bioset_exit(maintainer.pool);
	unregister_blkdev(maintainer.major, BLKDEV_NAME);
	pr_info("blkm exit\n");
}

static int blkmr_parse_device_name(char *input, char **path_for_maintainer,
				   char **path_for_search)
{
	ssize_t len;
	char *iter;

	len = strlen(input) + 1;
	if (len == 1)
		return -EINVAL;

	iter = strchr(input, '\n');

	if (iter) {
		*path_for_maintainer = kzalloc(sizeof(char) * len, GFP_KERNEL);
		*path_for_search =
			kzalloc(sizeof(char) * (len - 1), GFP_KERNEL);

		if (!(*path_for_maintainer) || !(path_for_search))
			goto dealloc;

		strcpy(*path_for_maintainer, input);
		strncpy(*path_for_search, *path_for_maintainer, len - 2);

	} else {
		*path_for_maintainer =
			kzalloc(sizeof(char) * len + 1, GFP_KERNEL);
		*path_for_search = kzalloc(sizeof(char) * (len), GFP_KERNEL);

		if (!(*path_for_maintainer) || !(path_for_search))
			goto dealloc;

		strcpy(*path_for_search, input);
		snprintf(*path_for_maintainer, len + 1, "%s\n", input);
	}

	return 0;

dealloc:
	kfree(*path_for_maintainer);
	kfree(*path_for_search);
	*path_for_search = NULL;
	*path_for_maintainer = NULL;
	return -ENOMEM;
}
static int blkmr_pipe_add(const char *arg, const struct kernel_param *kp)
{
	char *actual_name;
	int err;

	if (maintainer.last_bdev_path) {
		pr_err("Another device is already opened, please remove previous before opening new one.\n");
		return -EBUSY;
	}

	if (blkmr_parse_device_name(arg, &maintainer.last_bdev_path,
				    &actual_name))
		goto no_mem_for_name;

	maintainer.bdev = blkdev_get_by_path(actual_name, mode, THIS_MODULE);
	kfree(actual_name);
	maintainer.head = get_start_sect(maintainer.bdev);

	maintainer.memtable = lsm_create_memtable();
	if (IS_ERR(maintainer.bdev))
		goto incorrect_path;

	err = blkmr_set_maintainer_gendisk();
	if (err)
		goto gendisk_fail;

	pr_info("device instantiated successfully\n");
	return 0;

no_mem_for_name:
	pr_err("Cannot allocate space to read device name\n");
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
