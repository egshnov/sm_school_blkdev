// SPDX-License-Identifier: GPL-2.0-only
/*
 * SPBU MM summer school 2024 HW1
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/blkdev.h>

static char *last_bdev_path;
struct block_device *bdev; // TODO: some kind of dynamic list?
fmode_t mode = FMODE_READ | FMODE_WRITE;

static int __init blkm_init(void)
{
    pr_info("blkm init\n");
    return 0;
}

static void __exit blkm_exit(void)
{
    if (last_bdev_path)
    {
        blkdev_put(bdev, mode);
    }

    kfree(last_bdev_path);
    pr_info("blkm exit\n");
}

static int blkm_pipe_add(const char *arg, const struct kernel_param *kp)
{
    ssize_t len = strlen(arg) + 1;
    if (last_bdev_path)
    {
        pr_err("Another device is already opened, please remove previous one before opening new one.\n");
        return -EBUSY;
    }

    last_bdev_path = kzalloc(sizeof(char) * len, GFP_KERNEL);

    if (!last_bdev_path)
        return -ENOMEM;

    strcpy(last_bdev_path, arg);
    pr_info("recieved %s\n", last_bdev_path);
    bdev = blkdev_get_by_path(last_bdev_path, mode, THIS_MODULE);

    if (IS_ERR(bdev))
    {
        pr_err("Opening device '%s' failed, err: %d\n", last_bdev_path, PTR_ERR(bdev));
        goto free_path;
    }
    pr_info("opened %s\n", last_bdev_path);
    return 0;

free_path:
    kfree(last_bdev_path);
    last_bdev_path = NULL;
    return -ENODEV;
}

static int blkm_pipe_get_name(char *buf, const struct kernel_param *kp)
{
    ssize_t len;

    if (!last_bdev_path)
        return -EINVAL;
    len = strlen(last_bdev_path);

    strcpy(buf, last_bdev_path);
    strcat(buf, "\n\0"); // TODO: redundant ?

    return len;
}

static const struct kernel_param_ops blkm_name_ops = {
    .set = blkm_pipe_add,
    .get = blkm_pipe_get_name,
};

static int blkm_pipe_rm(const char *arg, const struct kernel_param *kp)
{
    if (!last_bdev_path)
    {
        pr_err("No opened devices\n");
        return -ENODEV;
    }

    blkdev_put(bdev, mode);
    kfree(last_bdev_path);
    last_bdev_path = NULL;
    pr_info("device removed\n");
    return 0;
}
static const struct kernel_param_ops blkm_rm = {
    .set = blkm_pipe_rm,
    .get = NULL,
};

// операция нахождения устройства по имени и его открытия
MODULE_PARM_DESC(device_pipe, "Device pipe");
module_param_cb(device_pipe, &blkm_name_ops, NULL, S_IRUGO | S_IWUSR);

// удаление устройства
MODULE_PARM_DESC(rm_device, "Remove device");
module_param_cb(rm_device, &blkm_rm, NULL, S_IWUSR);

module_init(blkm_init);
module_exit(blkm_exit);

MODULE_AUTHOR("Egor Shalashnov <shalasheg@gmail.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("blkm opening module");
