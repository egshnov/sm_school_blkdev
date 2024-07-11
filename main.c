// SPDX-License-Identifier: GPL-2.0-only
/*
 * SPBU MM summer school 2024 HW1
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/blkdev.h>

fmode_t mode = FMODE_READ | FMODE_WRITE | FMODE_EXCL;

struct device_maintainer
{   
    //TODO: add mutex
    char *last_bdev_path;
    struct block_device *bdev;
    // bio
    //...
};

static struct device_maintainer *maintainer;

static struct device_maintainer *alloc_maintainer(void)
{
    struct device_maintainer *target = kzalloc(sizeof(struct device_maintainer), GFP_KERNEL);
    return target;
}

static int __init blkm_init(void)
{
    pr_info("blkm init\n");
    maintainer = alloc_maintainer();
    if (!maintainer)
    {
        pr_err("Couldn't allocate maintainer\n");
        return -ENOMEM;
    }
    return 0;
}

static void __exit blkm_exit(void)
{
    if (maintainer->last_bdev_path)
    {
        blkdev_put(maintainer->bdev, mode);
    }

    kfree(maintainer->last_bdev_path); //dealloc maintainer
    pr_info("blkm exit\n");
}

static int blkm_pipe_add(const char *arg, const struct kernel_param *kp)
{
    ssize_t len = strlen(arg) + 1;
    if (maintainer->last_bdev_path)
    {
        pr_err("Another device is already opened, please remove previous before opening new one.\n");
        return -EBUSY;
    }

    maintainer->last_bdev_path = kzalloc(sizeof(char) * len, GFP_KERNEL);

    if (!maintainer->last_bdev_path)
    {
        pr_err("Cannot allocate space to read device name\n");
        return -ENOMEM;
    }

    strcpy(maintainer->last_bdev_path, arg);

    // trying to deal with \n produced by echo TODO: remove or switch to echo -n
    char *actual_name = kzalloc(sizeof(char) * (len - 1), GFP_KERNEL);

    if (!actual_name)
    {
        pr_err("Cannot allocate space to save actual device name.\n");
        return -ENOMEM;
    }

    for (int i = 0; i < len - 1; i++)
    {
        actual_name[i] = maintainer->last_bdev_path[i];
    }
    actual_name[len - 2] = '\0';

    maintainer->bdev = blkdev_get_by_path(actual_name, mode, THIS_MODULE);

    if (IS_ERR(maintainer->bdev))
    {
        pr_err("Opening device '%s' failed, err: %d\n", actual_name, PTR_ERR(maintainer->bdev));
        goto free_path;
    }

    pr_info("opened %s\n", actual_name);

    // TODO: REMOVE AS WELL AS PREVIOUS!!!!!!
    kfree(actual_name);
    return 0;

free_path:
    kfree(maintainer->last_bdev_path);
    maintainer->last_bdev_path = NULL;
    return -ENODEV;
}

static int blkm_pipe_get_name(char *buf, const struct kernel_param *kp)
{
    ssize_t len;

    if (!maintainer->last_bdev_path)
    {
        pr_err("No opened device\n");
        return -ENODEV;
    }

    len = strlen(maintainer->last_bdev_path);
    strcpy(buf, maintainer->last_bdev_path);
    return len;
}

static const struct kernel_param_ops blkm_name_ops = {
    .set = blkm_pipe_add,
    .get = blkm_pipe_get_name,
};

static int blkm_pipe_rm(const char *arg, const struct kernel_param *kp)
{
    if (!maintainer->last_bdev_path)
    {
        pr_err("No device to remove\n");
        return -ENODEV;
    }

    blkdev_put(maintainer->bdev, mode);
    kfree(maintainer->last_bdev_path);
    maintainer->last_bdev_path = NULL;
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
MODULE_DESCRIPTION("blkm opening module");
