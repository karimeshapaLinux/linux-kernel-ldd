#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/kfifo.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/of_platform.h>
#include <linux/device.h>
#include "pltdata.h"

MODULE_AUTHOR("Arabic Linux Community");
MODULE_DESCRIPTION("pltdrv_attr ldd");
MODULE_LICENSE("GPL");

#define DEFAULT_FIFO_SIZE	(16)
#define BASE_MINORS			(0)
#define NR_MINOR_DEVCS		(2)
#define DEVICE_NAME			"pltdrv-fifo"
#define DEVICE_CLASS		"pltdrv-fifoclass"

struct fifo_prvt_devc {
	struct cdev new_cdevice;
	struct kfifo myfifo;
	struct mutex r_f_lock;
	struct mutex w_f_lock;
	struct pltdata_fifo plt_data_prv;
	dev_t devnr;
};

struct prvt_drv {
	dev_t b_devnr;
	struct class *new_class;
	int nr_devcs_probed;
};
struct prvt_drv prv_drv;

static int setup_pltdrv_dev(struct fifo_prvt_devc *dev)
{
	int ret;
	int fsize = dev->plt_data_prv.fsize;
	mutex_init(&dev->r_f_lock);
	mutex_init(&dev->w_f_lock);
	ret = kfifo_alloc(&dev->myfifo,
				fsize, GFP_KERNEL);
	return ret;
}

static void uninstall_pltdrv_dev(struct fifo_prvt_devc *dev)
{
	kfifo_free(&dev->myfifo);
}

static int pltdrv_open_fifo(struct inode *inode, struct file *filp)
{
	struct fifo_prvt_devc *dev;
	dev = container_of(inode->i_cdev, struct fifo_prvt_devc, new_cdevice);
	filp->private_data = dev;
	pr_info("Open FIFO driver\n");
	return 0;
}

static int pltdrv_release_fifo(struct inode *inode, struct file *filp)
{
	pr_info("Close FIFO driver\n");
	return 0;
}

static ssize_t pltdrv_write_fifo(struct file *file, const char __user *buf,
						size_t count, loff_t *ppos)
{
	int ret;
	unsigned int copiedin;
	int avail;
	struct fifo_prvt_devc *dev = (struct fifo_prvt_devc *)file->private_data;

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

static ssize_t pltdrv_read_fifo(struct file *file, char __user *buf,
						size_t count, loff_t *ppos)
{
	int ret;
	unsigned int copiedout;
	struct fifo_prvt_devc *dev = (struct fifo_prvt_devc *)file->private_data;

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
	.open = pltdrv_open_fifo,
	.release = pltdrv_release_fifo,
	.read = pltdrv_read_fifo,
	.write = pltdrv_write_fifo
};

static ssize_t show_fsize(struct device *dev, struct device_attribute *attr, 
			char *buf)
{
	struct fifo_prvt_devc * dev_data;
	dev_data = (struct fifo_prvt_devc *)dev_get_drvdata(dev);
	return sprintf(buf, "sysfs show : device fifo size %d\n",
		dev_data->plt_data_prv.fsize);
}

static ssize_t show_fversion(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct fifo_prvt_devc * dev_data;
	dev_data = (struct fifo_prvt_devc *)dev_get_drvdata(dev);
	return sprintf(buf, "sysfs show : device fifo version %d\n",
		dev_data->plt_data_prv.version);
}

static ssize_t store_fversion(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	long ver; 
	int ret;
	struct fifo_prvt_devc * dev_data;
	dev_data = (struct fifo_prvt_devc *)dev_get_drvdata(dev);
	ret = kstrtol(buf, 10, &ver);
	if(ret)
		return ret;
	dev_data->plt_data_prv.version = ver;
	return count;
}

static DEVICE_ATTR(fifosize, S_IRUGO, show_fsize, NULL);
static DEVICE_ATTR(fifoversion, S_IRUGO|S_IWUSR, show_fversion, store_fversion);

static struct attribute * fifo_attrs[] = {
	&dev_attr_fifosize.attr,
	&dev_attr_fifoversion.attr,
	NULL
};

static struct attribute_group fifo_attr_grp = {
	.attrs = fifo_attrs
};

/**
 * Platform bus binding code, called when device detected
 */
static int pltdrv_probe(struct platform_device *ofdev)
{
	int ret;
	struct device *device = NULL;
	struct fifo_prvt_devc * dev_data;
	struct pltdata_fifo * plt_data;
	pr_info("pltdrv_probe: plt Device detected\n");
	plt_data = (struct pltdata_fifo *)dev_get_platdata(&ofdev->dev);
	if(!plt_data) {
		pr_err("No platfrom data for a device\n");
		return -ENOMEM;
	}
	dev_data = (struct fifo_prvt_devc *)devm_kzalloc(&ofdev->dev,
						sizeof(struct fifo_prvt_devc), GFP_KERNEL);

	if(!dev_data) {
		pr_err("Could allocate memory for a device: %d\n", ret);
		return -EINVAL;
	}
	dev_data->plt_data_prv.version = plt_data->version;
	dev_data->plt_data_prv.fsize = plt_data->fsize;
	pr_info("Device version: %d\n", dev_data->plt_data_prv.version);
	pr_info("Device fsize: %d\n", dev_data->plt_data_prv.fsize);

	dev_data->devnr = prv_drv.b_devnr + ofdev->id;
	pr_info("Device NR: %d\n",  MINOR(dev_data->devnr));

	dev_set_drvdata(&ofdev->dev, dev_data);

	device = device_create(prv_drv.new_class, &ofdev->dev, dev_data->devnr,
								NULL, "pltdrv-fifo-%d",ofdev->id);
	if (IS_ERR(device)) {
		ret = PTR_ERR(device);
		pr_err("Could not create device: %d\n", ret);
		return ret;
	}
	cdev_init(&dev_data->new_cdevice, &fops);
	dev_data->new_cdevice.owner = THIS_MODULE;
	ret = cdev_add(&dev_data->new_cdevice,dev_data->devnr, 1);
	if (ret) {
		pr_err("Could not register char dev: %d\n", ret);
		goto err_destruct_device;
	}
	ret = setup_pltdrv_dev(dev_data);
	if (ret) {
		pr_err("Could not create FIFO\n");
		goto err_unregister_cdev;
	}
	ret = sysfs_create_group(&ofdev->dev.kobj, &fifo_attr_grp);
	if (ret) {
		pr_err("Could not create sysfs group\n");
		goto err_setup_pltdrv;
	}
	pr_info("Probe ends Successfully: NR of devices %d\n",
				++prv_drv.nr_devcs_probed);
	return 0;
err_setup_pltdrv:
	uninstall_pltdrv_dev(dev_data);
err_unregister_cdev:
	cdev_del(&dev_data->new_cdevice);
err_destruct_device:
	device_destroy(prv_drv.new_class, dev_data->devnr);
	return ret;
}

static int pltdrv_remove(struct platform_device *ofdev)
{
	struct fifo_prvt_devc * dev_data;
	dev_data = dev_get_drvdata(&ofdev->dev);
	uninstall_pltdrv_dev(dev_data);
	cdev_del(&dev_data->new_cdevice);
	device_destroy(prv_drv.new_class, dev_data->devnr);
	sysfs_remove_group(&ofdev->dev.kobj, &fifo_attr_grp);
	pr_info("plt Device removed\n");
	pr_info("Remove device ends Successfully: NR of devices %d\n",
				--prv_drv.nr_devcs_probed);

	return 0;
}

struct platform_driver pltdrv = {
	.probe = pltdrv_probe,
	.remove = pltdrv_remove,
	.driver = {
		.name = "plt-fifo-dev"
	}
};

static int __init pltdrv_init(void)
{
	int ret;
	ret = alloc_chrdev_region(&prv_drv.b_devnr, BASE_MINORS, NR_MINOR_DEVCS, DEVICE_NAME);
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
	pr_info("pltdrv_attr Init\n");
	
	platform_driver_register(&pltdrv);

	return 0;
err_unregister_chrdev:
	unregister_chrdev_region(prv_drv.b_devnr, NR_MINOR_DEVCS);

	return ret;
}

static void __exit pltdrv_exit(void)
{
	platform_driver_unregister(&pltdrv);
	class_destroy(prv_drv.new_class);
	unregister_chrdev_region(prv_drv.b_devnr, NR_MINOR_DEVCS);
	pr_info("pltdrv_attr exit\n");
}

module_init(pltdrv_init);
module_exit(pltdrv_exit);