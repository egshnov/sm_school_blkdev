// SPDX-License-Identifier: GPL-2.0-only

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/blkdev.h>

#define BLKDEV_NAME "bdevm"
#define GD_NAME "blkm1"
#define BLKDEV_MINORS 1
#define mode (FMODE_READ | FMODE_WRITE | FMODE_EXCL)

static struct device_maintainer{
    char *last_bdev_path; // NULLPTR by default, set to NULLPTR when device is removed and memmory is freed
    struct block_device *bdev;
    struct gendisk *gd; // NULLPTR by default, set to NULLPTR when device is removed or adding disk fails
    int major;

} maintainer;

/* seems like having maintainer as a global variable makes having such a struct defined kind of redundant
   since we do not pass it to any function as a pointer but overhead is small and I believe that it makes code much more readable.
   Maybe making it a local pointer is a better option but i'm not sure (TODO: ask during code review) */

static void blkm_submit_bio(struct bio *bio)
{

    struct bio_set *pool = kzalloc(sizeof(struct bio_set), GFP_KERNEL);}
    
    bioset_init(pool, BIO_POOL_SIZE, 0, BIOSET_NEED_BVECS);
    
    struct bio *new_bio = bio_alloc_clone(maintainer.bdev, bio, GFP_KERNEL, pool);

    bio_chain(new_bio, bio);
    submit_bio(new_bio);
}

static const struct block_device_operations bio_ops = {
    .owner = THIS_MODULE,
    .submit_bio = blkm_submit_bio,
};

static int set_maintainer_gendisk(void)
{
    maintainer.gd = blk_alloc_disk(BLKDEV_MINORS);
    if (!maintainer.gd)
    {
        pr_err("Couldn't alloc gendisk\n");
        return -ENOMEM;
    }

    maintainer.gd->major = maintainer.major;
    maintainer.gd->first_minor = 1;
    maintainer.gd->minors = 1;
    maintainer.gd->fops = &bio_ops;
    maintainer.gd->private_data = &maintainer;
    maintainer.gd->part0 = maintainer.bdev;

    strcpy(maintainer.gd->disk_name, GD_NAME);
    set_capacity(maintainer.gd, get_capacity(maintainer.bdev->bd_disk));
    
    int err = add_disk(maintainer.gd);
    if (err){
        pr_err("Couldn't add gendisk %d\n", err);
        put_disk(maintainer.gd);
        maintainer.gd = NULL;
    }

    return err;
}

static int __init blkm_init(void)
{
    pr_info("blkm init\n");
    maintainer.major = register_blkdev(0, BLKDEV_NAME);
    if (maintainer.major < 0){
        pr_err("Unable to register block device\n");
        return -EBUSY;
    }

    return 0;
}

static void clear_maintainer(void)
{
    if (maintainer.last_bdev_path){
        if (maintainer.gd){
            del_gendisk(maintainer.gd);
            put_disk(maintainer.gd);
            invalidate_disk(maintainer.gd);
            maintainer.gd = NULL;
        }
        blkdev_put(maintainer.bdev, mode);
    }
    kfree(maintainer.last_bdev_path);
    maintainer.last_bdev_path = NULL;
}

static void __exit blkm_exit(void)
{
    clear_maintainer();
    unregister_blkdev(maintainer.major, BLKDEV_NAME);
    pr_info("blkm exit\n");
}

static int blkm_pipe_add(const char *arg, const struct kernel_param *kp)
{

    ssize_t len = strlen(arg) + 1;
    if (maintainer.last_bdev_path){
        pr_err("Another device is already opened, please remove previous before opening new one.\n");
        return -EBUSY;
    }

    maintainer.last_bdev_path = kzalloc(sizeof(char) * len, GFP_KERNEL);

    if (!maintainer.last_bdev_path){
        pr_err("Cannot allocate space to read device name\n");
        return -ENOMEM;
    }

    strcpy(maintainer.last_bdev_path, arg);

    {
        char *actual_name = kzalloc(sizeof(char) * (len - 1), GFP_KERNEL);

        if (!actual_name){
            pr_err("Cannot allocate space to save actual device name.\n");
            kfree(maintainer.last_bdev_path);
            return -ENOMEM;
        }
        strncpy(actual_name, maintainer.last_bdev_path, len - 2);
        maintainer.bdev = blkdev_get_by_path(actual_name, mode, THIS_MODULE);
        kfree(actual_name);
    }

    if (IS_ERR(maintainer.bdev)){
        pr_err("Opening device failed, err: %d\n", PTR_ERR(maintainer.bdev));

        kfree(maintainer.last_bdev_path);
        maintainer.last_bdev_path = NULL;
        return -ENODEV;
    }

    int err = set_maintainer_gendisk();
    if (err){
        pr_err("Couldn't create disk");
        clear_maintainer();
        return err;
    }
    pr_info("device opened\n");
    return 0;
}

static int blkm_pipe_get_name(char *buf, const struct kernel_param *kp)
{
    ssize_t len;

    if (!maintainer.last_bdev_path){
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
    if (!maintainer.last_bdev_path){
        pr_err("No device to remove\n");
        return -ENODEV;
    }

    clear_maintainer();
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
MODULE_DESCRIPTION("Simple I/O redirection driver");
