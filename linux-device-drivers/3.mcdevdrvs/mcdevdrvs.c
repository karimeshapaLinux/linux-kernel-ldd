#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/kfifo.h>
#include <linux/fs.h>
#include <linux/cdev.h>

MODULE_AUTHOR("Linux Community");
MODULE_DESCRIPTION("mcdevdrvs ldd");
MODULE_LICENSE("GPL");

#define DEVICE_NAME			"mcdevdrvs"
#define DEVICE_CLASS		"mcdevdrvsclass"
#define BASE_MINORS			(0)
#define NR_MINOR_DEVCS		(2)
#define DEFAULT_FIFO_SIZE	(16)

static int fsize = DEFAULT_FIFO_SIZE;
module_param(fsize, int, S_IRUGO);


struct prvt_devc {
	struct cdev new_cdevice;
	struct kfifo myfifo;
	struct mutex r_f_lock;
	struct mutex w_f_lock;
};

struct prvt_drv {
	dev_t dev_nr;
	struct class *new_class;
	struct prvt_devc devcs[NR_MINOR_DEVCS];
};

struct prvt_drv prv_drv;

static int driver_open_fifo(struct inode *inode, struct file *filp)
{
	struct prvt_devc *dev;
	int minor = MINOR(inode->i_rdev);
	int major = MAJOR(inode->i_rdev);
	pr_info("Open FIFO majr:%d minordev:%d\n", major, minor);
	dev = container_of(inode->i_cdev, struct prvt_devc, new_cdevice);
	filp->private_data = dev;
	return 0;
}

static int driver_release_fifo(struct inode *inode, struct file *filp)
{
	int minor = MINOR(inode->i_rdev);
	int major = MAJOR(inode->i_rdev);
	pr_info("Close FIFO major:%d minordev:%d\n", major, minor);
	return 0;
}

static ssize_t driver_write_fifo(struct file *file, const char __user *buf,
						size_t count, loff_t *ppos)
{
	int ret;
	unsigned int copiedin;
	int avail;
	struct prvt_devc *dev = (struct prvt_devc *)file->private_data;
	if (mutex_lock_interruptible(&dev->w_f_lock))
		return -ERESTARTSYS;

	avail = kfifo_avail(&dev->myfifo);
	pr_info("kfifo available bytes %d\n", avail);
	ret = kfifo_from_user(&dev->myfifo, buf, count, &copiedin);
	mutex_unlock(&dev->w_f_lock);
	pr_info("The data copied: %d\n", copiedin);
	
	/**
	 * in case of -EFAULT -> ret to system 
	 */
	if (ret)
		return ret;
	/** 
	 * I don't need kernel code to invoke write again
	 * with the rest of uncopied data that's failed
	 * because of fifo size limitation -> ret count.
	 */
	return count;
}

static ssize_t driver_read_fifo(struct file *file, char __user *buf,
						size_t count, loff_t *ppos)
{
	int ret;
	unsigned int copiedout;
	struct prvt_devc *dev = (struct prvt_devc *)file->private_data;
	if (mutex_lock_interruptible(&dev->r_f_lock))
		return -ERESTARTSYS;
	ret = kfifo_to_user(&dev->myfifo, buf, count, &copiedout);
	mutex_unlock(&dev->r_f_lock);

	/**
	 * in case of -EFAULT -> ret to system 
	 */
	if (ret)
		return ret;

	return copiedout;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = driver_open_fifo,
	.release = driver_release_fifo,
	.read = driver_read_fifo,
	.write = driver_write_fifo
};

static int __init mcdevdrvs_init(void)
{
	int ret;
	struct device *device = NULL;
	int rc;
	int dev_cnt, fifo_cnt, prnt_dev;

	ret = alloc_chrdev_region(&prv_drv.dev_nr, BASE_MINORS, NR_MINOR_DEVCS, DEVICE_NAME);
	if (ret < 0) {
		pr_err("failed to allocate device numbers: %d\n", ret);
		return ret;
	}
	prv_drv.new_class = class_create(THIS_MODULE, DEVICE_CLASS);
	if (IS_ERR(prv_drv.new_class)) {
		ret = PTR_ERR(prv_drv.new_class);
		pr_err("failed to create class: %d\n", ret);
		goto err_unregister_chrdev;
	}
	for(dev_cnt = 0; dev_cnt < NR_MINOR_DEVCS; dev_cnt++) {
		device = device_create(prv_drv.new_class, NULL, prv_drv.dev_nr+dev_cnt,
								NULL, "mcdevdrvs-%d",dev_cnt);
		if (IS_ERR(device)) {
			ret = PTR_ERR(device);
			pr_err("Could not create device: %d\n", ret);
			goto err_destruct_class;
		}
		cdev_init(&prv_drv.devcs[dev_cnt].new_cdevice, &fops);
		rc = cdev_add(&prv_drv.devcs[dev_cnt].new_cdevice, prv_drv.dev_nr+dev_cnt, 1);
		if (rc) {
			pr_err("Could not register char dev: %d\n", rc);
			goto err_destruct_device;
		}
	}
	for(fifo_cnt = 0; fifo_cnt < NR_MINOR_DEVCS; fifo_cnt++) {
		ret = kfifo_alloc(&prv_drv.devcs[fifo_cnt].myfifo, fsize, GFP_KERNEL);
		if (ret) {
			pr_err("Could not create FIFO\n");
			goto err_unregister_cdev;
		}
	}
	for(prnt_dev = 0; prnt_dev < NR_MINOR_DEVCS; prnt_dev++)
		pr_info("mcdevdrvs driver registered with major %d and Minor %d\n",
					MAJOR(prv_drv.dev_nr+prnt_dev), MINOR(prv_drv.dev_nr+prnt_dev));
	return 0;

err_unregister_cdev:
	for(; fifo_cnt >= 0; fifo_cnt--)
		kfifo_free(&prv_drv.devcs[fifo_cnt].myfifo);
err_destruct_device:
err_destruct_class:
	for(; dev_cnt >= 0; dev_cnt--) {
		cdev_del(&prv_drv.devcs[dev_cnt].new_cdevice);
		device_destroy(prv_drv.new_class, prv_drv.dev_nr+dev_cnt);
	}
	class_destroy(prv_drv.new_class);
err_unregister_chrdev:
	unregister_chrdev_region(prv_drv.dev_nr, NR_MINOR_DEVCS);

	return ret;
}

static void __exit mcdevdrvs_exit(void)
{
	int dev_cnt, fifo_cnt;
	for(dev_cnt = 0; dev_cnt < NR_MINOR_DEVCS; dev_cnt++) {
		cdev_del(&prv_drv.devcs[dev_cnt].new_cdevice);
		device_destroy(prv_drv.new_class, prv_drv.dev_nr+dev_cnt);		
	}
	class_destroy(prv_drv.new_class);
	unregister_chrdev_region(prv_drv.dev_nr, NR_MINOR_DEVCS);
	for(fifo_cnt = 0; fifo_cnt < NR_MINOR_DEVCS; fifo_cnt++)
		kfifo_free(&prv_drv.devcs[fifo_cnt].myfifo);
	pr_info("Removing mcdevdrvs Ldd\n");
}

module_init(mcdevdrvs_init);
module_exit(mcdevdrvs_exit);