#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/kfifo.h>
#include <linux/fs.h>
#include <linux/cdev.h>

MODULE_AUTHOR("Arabic Linux Community");
MODULE_DESCRIPTION("kernelnewbies ldd");
MODULE_LICENSE("GPL");

#define DEVICE_NAME			"kernelnewbies"
#define DRIVER_CLASS		"kernelnewbiesclass"
#define BASE_MINORS			(0)
#define NR_MINORS			(1)
#define DEFAULT_FIFO_SIZE	(16)

static int fsize = DEFAULT_FIFO_SIZE;
module_param(fsize, int, S_IRUGO);

static dev_t dev_nr;
static struct class *new_class;
static struct cdev new_cdevice;

static DEFINE_MUTEX(read_access);
static DEFINE_MUTEX(write_access);
static struct kfifo myfifo;

static int driver_open_fifo(struct inode *inode, struct file *filp)
{
	pr_info("Open FIFO driver\n");
	return 0;
}

static int driver_release_fifo(struct inode *inode, struct file *filp)
{
	pr_info("Close FIFO driver\n");
	return 0;
}

static ssize_t driver_write_fifo(struct file *file, const char __user *buf,
						size_t count, loff_t *ppos)
{
	int ret;
	unsigned int copiedin;
	int avail;
	if (mutex_lock_interruptible(&write_access))
		return -ERESTARTSYS;

	avail = kfifo_avail(&myfifo);
	pr_info("kfifo available bytes %d\n", avail);

	/**
	 * kfifo_from_user - puts some data from user space into the fifo
	 * @fifo: address of the fifo to be used
	 * @from: pointer to the data to be added
	 * @len: the length of the data to be added
	 * @copied: pointer to output variable to store the number of copied bytes
	 */
	ret = kfifo_from_user(&myfifo, buf, count, &copiedin);
	mutex_unlock(&write_access);
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

	if (mutex_lock_interruptible(&read_access))
		return -ERESTARTSYS;

	/**
	 * kfifo_to_user - copies data from the fifo into user space
	 * @fifo: address of the fifo to be used
	 * @to: where the data must be copied
	 * @len: the size of the destination buffer
	 * @copied: pointer to output variable to store the number of copied bytes
	 */
	ret = kfifo_to_user(&myfifo, buf, count, &copiedout);
	mutex_unlock(&read_access);

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

static int __init kernelnewbies_init(void)
{
	int ret;
	struct device *device = NULL;
	int rc;
	/**
	 * alloc_chrdev_region() - register a range of char device numbers
	 * @dev: output parameter for first assigned number
	 * @baseminor: first of the requested range of minor numbers
	 * @count: the number of minor numbers required
	 * @name: the name of the associated device or driver
	 */
	ret = alloc_chrdev_region(&dev_nr, BASE_MINORS, NR_MINORS, DEVICE_NAME);
	if (ret < 0) {
		pr_err("failed to allocate device numbers: %d\n", ret);
		return ret;
	}

	/**
	 * class_create - create a struct class structure
	 * @owner: pointer to the module that is to "own" this struct class
	 * @name: pointer to a string for the name of this class.
	 */
	new_class = class_create(THIS_MODULE, DRIVER_CLASS);
	if (IS_ERR(new_class)) {
		ret = PTR_ERR(new_class);
		pr_err("failed to create class: %d\n", ret);
		goto err_class_create;
	}

	/**
	 * device_create - creates a device and registers it with sysfs
	 * @class: pointer to the struct class that this device should be registered to
	 * @parent: pointer to the parent struct device of this new device, if any
	 * @devt: the dev_t for the char device to be added
	 * @drvdata: the data to be added to the device for callbacks
	 * @fmt: string for the device's name
	 */
	device = device_create(new_class, NULL, dev_nr, NULL, DEVICE_NAME);
	if (IS_ERR(device)) {
		ret = PTR_ERR(device);
		pr_err("Could not create device: %d\n", ret);
		goto err_device_create;
	}

	/**
	 * cdev_init() - initialize a cdev structure
	 * @cdev: the structure to initialize
	 * @fops: the file_operations for this device
	 *
	 * Initializes @cdev, remembering @fops, making it ready to add to the
	 * system with cdev_add().
	 */
	cdev_init(&new_cdevice, &fops);

	/**
	 * cdev_add() - add a char device to the system
	 * @p: the cdev structure for the device
	 * @dev: the first device number for which this device is responsible
	 * @count: the number of consecutive minor numbers corresponding to this device
	 */
	rc = cdev_add(&new_cdevice, dev_nr, 1);
	if (rc) {
		pr_err("Could not register char dev: %d\n", rc);
		goto err_add_device;
	}

	ret = kfifo_alloc(&myfifo, fsize, GFP_KERNEL);
	if (ret) {
		pr_err("Could not create FIFO\n");
		goto err_alloc_fifo;
	}
	
	pr_info("kernelnewbies driver registered with major %d\n", MAJOR(dev_nr));
	return 0;

err_alloc_fifo:
	cdev_del(&new_cdevice);
err_add_device:
	device_destroy(new_class, dev_nr);
err_device_create:
	class_destroy(new_class);
err_class_create:
	unregister_chrdev_region(dev_nr, NR_MINORS);

	return ret;
}

static void __exit kernelnewbies_exit(void)
{
	cdev_del(&new_cdevice);
	device_destroy(new_class, dev_nr);
	class_destroy(new_class);
	unregister_chrdev_region(dev_nr, NR_MINORS);
	kfifo_free(&myfifo);
	pr_info("Removing kernelnewbies Ldd\n");
}

module_init(kernelnewbies_init);
module_exit(kernelnewbies_exit);
