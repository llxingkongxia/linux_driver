#include <linux/module.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/device.h>

#define SECOND_MAJOR 0

static int second_major = SECOND_MAJOR;
module_param(second_major, int, S_IRUGO);

struct second_dev{
    struct cdev cdev;
    atomic_t counter;
    struct timer_list s_timer;
    struct class  *second_class;
    struct device *second_device; 
};

static struct second_dev *second_devp;

static void second_timer_handler(unsigned long arg)
{
    mod_timer(&second_devp->s_timer,jiffies + HZ);
    atomic_inc(&second_devp->counter);
    
    printk(KERN_INFO "Current jiffies is %ld\n",jiffies);
}

static int second_open(struct inode *inode, struct file *filp)
{
    init_timer(&second_devp->s_timer);
    second_devp->s_timer.function = &second_timer_handler;
    second_devp->s_timer.expires = jiffies + HZ;
    
    add_timer(&second_devp->s_timer);
    atomic_set(&second_devp->counter, 0);
    
    return 0;
}

static int second_release(struct inode *inode, struct file *filp)
{
    del_timer(&second_devp->s_timer);
    return 0;
}

static ssize_t second_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
    int counter;
    counter = atomic_read(&second_devp -> counter);
    if(put_user(counter, (int *)buf))
        return -EFAULT;
    else
        return sizeof(unsigned int);
}

static const struct file_operations second_fops = {
    .owner = THIS_MODULE,
    .open = second_open,
    .release = second_release,
    .read = second_read,
};

static void second_setup_cdev(struct second_dev *dev, int index)
{
    int err, devno = MKDEV(second_major, index);
    cdev_init(&dev->cdev, &second_fops);
    err = cdev_add(&dev->cdev,devno,1);
    if(err)
        printk(KERN_ERR "Failed to add second device\n");
}

static int __init second_init(void)
{
    int ret;
    dev_t devno;
    if(second_major){
	devno = MKDEV(second_major, 0);
        ret = register_chrdev_region(devno, 1, "second");
    }
    else {
        ret = alloc_chrdev_region(&devno, 0, 1, "second");
        second_major = MAJOR(devno);
    }
    
    if(ret < 0)
        return ret;
    
    second_devp = kzalloc(sizeof(*second_devp), GFP_KERNEL);
    if(!second_devp){
        ret = -ENOMEM;
        goto fail_malloc;
    }
    
    second_setup_cdev(second_devp, 0);
    second_devp->second_class = class_create(THIS_MODULE, "second_class");
    if(second_devp)
      second_devp->second_device = device_create(second_devp->second_class, NULL, devno, NULL, "second");
    else
	printk(KERN_ERR "Failed to create class in second device\n");
    return 0;
    
fail_malloc:
    unregister_chrdev_region(devno, 1);
    return ret;
}
module_init(second_init);

static void __exit second_exit(void)
{
    cdev_del(&second_devp->cdev);
    kfree(second_devp);
    unregister_chrdev_region(MKDEV(second_major, 0), 1);
}
module_exit(second_exit);

MODULE_AUTHOR("xxxxx");
MODULE_LICENSE("GPL v2");
