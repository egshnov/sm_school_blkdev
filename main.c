// SPDX-License-Identifier: GPL-2.0-only

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/blkdev.h>

#define BLKDEV_NAME "bdevm"
#define BLKDEV_MINORS 1
#define K_SECTOR_SIZE 512

static int major;
static fmode_t mode = FMODE_READ | FMODE_WRITE | FMODE_EXCL;

static struct device_maintainer
{
    char *last_bdev_path;
    struct block_device *bdev;
    struct gendisk *gd;

} maintainer; // make local pointer?

static void my_submit_bio(struct bio *bio)
{
    pr_info("do nothing\n");
    bio_endio(bio);
}

static const struct block_device_operations bio_ops = {
    .owner = THIS_MODULE,
    .submit_bio = my_submit_bio,
};

static int set_maintainer_gendisk(void)
{
    maintainer.gd = blk_alloc_disk(BLKDEV_MINORS);

    if (!maintainer.gd)
    {
        pr_err("Couldn't alloc gendisk\n");
        return -ENOMEM;
    }

    maintainer.gd->major = major;
    maintainer.gd->first_minor = 1;
    maintainer.gd->minors = 1;
    maintainer.gd->fops = &bio_ops;
    maintainer.gd->private_data = &maintainer;

    strcpy(maintainer.gd->disk_name, "bdevm0");
    set_capacity(maintainer.gd, get_capacity(maintainer.bdev->bd_disk));
    int err = add_disk(maintainer.gd);
    if (err)
    {
        pr_err("Couldn't add gendisk %d\n", err);
        put_disk(maintainer.gd);
    }

    return err;
}

static int __init blkm_init(void)
{
    pr_info("blkm init\n");
    major = register_blkdev(0, BLKDEV_NAME);
    if (major < 0)
    {
        pr_err("Unable to register block device\n");
        return -EBUSY;
    }

    return 0;
}

static void free_maintainer(void)
{
    if (maintainer.last_bdev_path)
    {
        del_gendisk(maintainer.gd);
        put_disk(maintainer.gd);
        blkdev_put(maintainer.bdev, mode);
    }

    kfree(maintainer.last_bdev_path);
}

static void __exit blkm_exit(void)
{
    free_maintainer();
    unregister_blkdev(major, BLKDEV_NAME);
    pr_info("blkm exit\n");
}

static int blkm_pipe_add(const char *arg, const struct kernel_param *kp)
{

    ssize_t len = strlen(arg) + 1;
    if (maintainer.last_bdev_path)
    {
        pr_err("Another device is already opened, please remove previous before opening new one.\n");
        return -EBUSY;
    }

    maintainer.last_bdev_path = kzalloc(sizeof(char) * len, GFP_KERNEL); // NULLPTR by default, set to NULLPTR when device is removed

    if (!maintainer.last_bdev_path)
    {
        pr_err("Cannot allocate space to read device name\n");
        return -ENOMEM;
    }

    strcpy(maintainer.last_bdev_path, arg);

    // trying to deal with \n produced by echo TODO: remove or switch to echo -n (deal in more adequate manner)
    char *actual_name = kzalloc(sizeof(char) * (len - 1), GFP_KERNEL);

    if (!actual_name)
    {
        pr_err("Cannot allocate space to save actual device name.\n");
        return -ENOMEM;
    }

    for (int i = 0; i < len - 1; i++)
    {
        actual_name[i] = maintainer.last_bdev_path[i];
    }
    actual_name[len - 2] = '\0';

    maintainer.bdev = blkdev_get_by_path(actual_name, mode, THIS_MODULE);
    kfree(actual_name);

    if (IS_ERR(maintainer.bdev))
    {
        pr_err("Opening device failed, err: %d\n", PTR_ERR(maintainer.bdev));
        goto free_path;
    }

    pr_info("opened %s\n", actual_name); // actual_name

    int err = set_maintainer_gendisk();
    // TODO: refactor to goto
    if (err)
    {
        pr_info("Couldn't create disk");
        put_disk(maintainer.gd);
        blkdev_put(maintainer.bdev, mode);
        kfree(maintainer.last_bdev_path);
        maintainer.last_bdev_path = NULL;
    }
    else
        pr_info("blkm init\n");
    return 0;

free_path:
    kfree(maintainer.last_bdev_path);
    maintainer.last_bdev_path = NULL;
    return -ENODEV;
}

static int blkm_pipe_get_name(char *buf, const struct kernel_param *kp)
{
    ssize_t len;

    if (!maintainer.last_bdev_path)
    {
        pr_err("No opened device\n");
        return -ENODEV;
    }

    len = strlen(maintainer.last_bdev_path);
    strcpy(buf, maintainer.last_bdev_path);
    return len;
}

static const struct kernel_param_ops blkm_name_ops = {
    .set = blkm_pipe_add,
    .get = blkm_pipe_get_name,
};

static int blkm_pipe_rm(const char *arg, const struct kernel_param *kp)
{
    if (!maintainer.last_bdev_path)
    {
        pr_err("No device to remove\n");
        return -ENODEV;
    }

    blkdev_put(maintainer.bdev, mode);
    kfree(maintainer.last_bdev_path);
    maintainer.last_bdev_path = NULL;
    del_gendisk(maintainer.gd);
    put_disk(maintainer.gd);
    pr_info("device removed\n");
    return 0;
}

static const struct kernel_param_ops blkm_rm = {
    .set = blkm_pipe_rm,
    .get = NULL,
};

MODULE_PARM_DESC(device_pipe, "Device pipe");
module_param_cb(device_pipe, &blkm_name_ops, NULL, S_IRUGO | S_IWUSR);

MODULE_PARM_DESC(rm_device, "Remove device");
module_param_cb(rm_device, &blkm_rm, NULL, S_IWUSR);

module_init(blkm_init);
module_exit(blkm_exit);

MODULE_AUTHOR("Egor Shalashnov <shalasheg@gmail.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SPBU MM summer school 2024 HW1");
