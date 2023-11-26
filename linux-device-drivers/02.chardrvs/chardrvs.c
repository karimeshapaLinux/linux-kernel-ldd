#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/kfifo.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include "chardrvs.h"
#define GET_DEVICE()	(chardrvs_ptr)
#define GET_FIFO_SIZE()	(fsize)

MODULE_AUTHOR("Linux Community");
MODULE_DESCRIPTION("chardrvs ldd");
MODULE_LICENSE("GPL");

static int fsize = DEFAULT_FIFO_SIZE;
module_param(fsize, int, S_IRUGO);

static struct chardrvs_priv_dev *chardrvs_ptr;

static int setup_chardrvs(struct chardrvs_priv_dev *dev)
{
	int ret;

	mutex_init(&dev->lock);
	mutex_init(&dev->r_f_lock);
	mutex_init(&dev->w_f_lock);
	dev->usrs_cnt = DEFAULT_NR_USERS;
	/**
	 * Init waitqueue for process sleep
	 */
	init_waitqueue_head(&dev->wq_f);
	ret = kfifo_alloc(&dev->myfifo,
				fsize, GFP_KERNEL);
	if (!ret)
		dev->avail = fsize;
	return ret;
}

static void uninstall_chardrvs(struct chardrvs_priv_dev *dev)
{
	kfifo_free(&dev->myfifo);
}

static int chardrvs_open(struct inode *inode, struct file *filp)
{
	struct chardrvs_priv_dev *dev = GET_DEVICE();

	if (atomic_read(&dev->ref_cntr) >= dev->usrs_cnt) {
		pr_err("Too many users open files \n");
		return -EMFILE; /** Too many open files */
	}
	atomic_inc(&dev->ref_cntr);
	filp->private_data = dev;
	pr_info("Open chardrvs driver\n");
	return 0;
}

static int chardrvs_release(struct inode *inode, struct file *filp)
{
	struct chardrvs_priv_dev *dev = (struct chardrvs_priv_dev *)filp->private_data;

	if (atomic_read(&dev->ref_cntr) > 0)
		atomic_dec(&dev->ref_cntr);
	pr_info("Close chardrvs driver\n");
	return 0;
}

static ssize_t chardrvs_write_fifo(struct file *file, const char __user *buf,
						size_t count, loff_t *ppos)
{
	int ret;
	unsigned int copiedin;
	struct chardrvs_priv_dev *dev = (struct chardrvs_priv_dev *)file->private_data;

	if (mutex_lock_interruptible(&dev->w_f_lock))
		return -ERESTARTSYS;

	/**
	 * sleep until get event
	 */
	if (wait_event_interruptible(dev->wq_f, (dev->avail >= count)))
			return -ERESTARTSYS;

	ret = kfifo_from_user(&dev->myfifo, buf, count, &copiedin);
	mutex_unlock(&dev->w_f_lock);
	pr_info("The data copiedin: %d\n", copiedin);

	mutex_lock(&dev->lock);
	dev->avail = kfifo_avail(&dev->myfifo);
	mutex_unlock(&dev->lock);
	/**
	 * in case of -EFAULT -> ret to system 
	 */
	if (ret)
		return ret;

	return copiedin;
}

static ssize_t chardrvs_read_fifo(struct file *file, char __user *buf,
						size_t count, loff_t *ppos)
{
	int ret;
	unsigned int copiedout;
	struct chardrvs_priv_dev *dev = (struct chardrvs_priv_dev *)file->private_data;

	if (mutex_lock_interruptible(&dev->r_f_lock))
		return -ERESTARTSYS;

	ret = kfifo_to_user(&dev->myfifo, buf, count, &copiedout);
	/**
	 * wake up writer
	 */
	wake_up_interruptible(&dev->wq_f); 
	mutex_unlock(&dev->r_f_lock);
	pr_info("The data copiedout: %d\n", copiedout);

	mutex_lock(&dev->lock);
	dev->avail = kfifo_avail(&dev->myfifo);
	mutex_unlock(&dev->lock);
	/**
	 * in case of -EFAULT -> ret to system 
	 */
	if (ret)
		return ret;

	return copiedout;
}

long chardrvs_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct chardrvs_priv_dev *dev = (struct chardrvs_priv_dev *)filp->private_data;

	if (_IOC_TYPE(cmd) != CHARDRVS_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > CHARDRVS_IOC_MAX_NR)
		return -ENOTTY;
	
	switch(cmd) {
		case CHARDRVS_IOCSETNRUSERS:
			if (! capable (CAP_SYS_ADMIN))
				return -EPERM;
			ret = __get_user(dev->usrs_cnt, (unsigned long __user *)arg);
			pr_info("number of fifo users changed to %d\n", dev->usrs_cnt);
			break;

		case CHARDRVS_IOCGETNRUSERS:
			if (! capable (CAP_SYS_ADMIN))
				return -EPERM;
			ret = __put_user(dev->usrs_cnt, (unsigned long __user *)arg);
			break;

		case CHARDRVS_IOCQUERYAVAILSIZE:
			ret = __put_user(dev->avail, (unsigned long __user *)arg);
			break;

		default:
			return -ENOTTY;
	}

	return ret;
}

static unsigned int chardrvs_poll(struct file *filp, poll_table *wait)
{
	struct chardrvs_priv_dev *dev = (struct chardrvs_priv_dev *)filp->private_data;
	unsigned int mask = 0;

	mutex_lock(&dev->lock);
	poll_wait(filp, &dev->wq_f, wait);
	mutex_unlock(&dev->lock);
	if (dev->avail)
		mask |= POLLOUT;
	return mask;
}

#ifdef CHARDRVS_DBG
static int chardrvs_read_procmem(struct seq_file *sf, void *unused)
{
	struct chardrvs_priv_dev *dev = GET_DEVICE();

	seq_printf(sf, "Fifo max number of users %d\n", dev->usrs_cnt);
	seq_printf(sf, "Fifo avialable entries %d\n", dev->avail);
	seq_printf(sf, "Fifo current number of users %d\n", atomic_read(&dev->ref_cntr));
	seq_printf(sf, "Fifo size %d\n", GET_FIFO_SIZE());

	return 0;
}

static int chardrvs_read_dbgmem(struct seq_file *sf, void *unused)
{
	struct chardrvs_priv_dev *dev = GET_DEVICE();

	seq_printf(sf, "class at %p, cdev at %p, fifo at %p\n",
		dev->new_class, &dev->new_cdevice, &dev->myfifo);

	seq_printf(sf, "proc file at %p, dbg dir at %p, dbg file at %p\n",
		dev->proc_file, dev->dbg_dir, dev->dbg_file);

	return 0;
}

static int chardrvs_proc_open(struct inode *inode, struct file *file)
{
	/*
	 * By default using -> seq_open(file, &operations);
	 * struct seq_operations {
	 * 	void * (*start) (struct seq_file *m, loff_t *pos);
	 * 	void (*stop) (struct seq_file *m, void *v);
	 * 	void * (*next) (struct seq_file *m, void *v, loff_t *pos);
	 * 	int (*show) (struct seq_file *m, void *v);
	 * };
	 */	
	return single_open(file, chardrvs_read_procmem, NULL);
}

static int chardrvs_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, chardrvs_read_dbgmem, NULL);
}

static struct proc_ops chardrvs_proc_fops = {
	/**
	 * mostly for device info
	 */
	.proc_open = chardrvs_proc_open,
	.proc_read = seq_read
};

static const struct file_operations chardrvs_dbg_fops = {
	/**
	 * mostly for device registers
	 * manipulation and status
	 */
	.open = chardrvs_dbg_open,
	.read = seq_read
};
#endif

static struct file_operations chardrvs_fops = {
	.owner = THIS_MODULE,
	.open = chardrvs_open,
	.release = chardrvs_release,
	.read = chardrvs_read_fifo,
	.write = chardrvs_write_fifo,
	.unlocked_ioctl = chardrvs_ioctl,
	.poll = chardrvs_poll,
	.llseek = no_llseek
};


static int __init chardrvs_init(void)
{
	int ret;
	struct device *device = NULL;

	chardrvs_ptr = kmalloc(sizeof(struct chardrvs_priv_dev), GFP_KERNEL);
	if (!chardrvs_ptr) {
		ret = -ENOMEM;
		pr_err("failed to allocate memory for device: %d\n", ret);
		return ret;
	}
	memset(chardrvs_ptr, 0, sizeof(struct chardrvs_priv_dev));

	ret = alloc_chrdev_region(&chardrvs_ptr->dev_nr, BASE_MINORS, NR_MINORS, DRIVER_NAME);
	if (ret < 0) {
		pr_err("failed to allocate device numbers: %d\n", ret);
		goto err_free_dev;
	}

	chardrvs_ptr->new_class = class_create(THIS_MODULE, DRIVER_CLASS);
	if (IS_ERR(chardrvs_ptr->new_class)) {
		ret = PTR_ERR(chardrvs_ptr->new_class);
		pr_err("failed to create class: %d\n", ret);
		goto err_unregister_chrdev;
	}

	device = device_create(chardrvs_ptr->new_class, NULL, chardrvs_ptr->dev_nr, NULL, DRIVER_NAME);
	if (IS_ERR(device)) {
		ret = PTR_ERR(device);
		pr_err("Could not create device: %d\n", ret);
		goto err_destruct_class;
	}

	cdev_init(&chardrvs_ptr->new_cdevice, &chardrvs_fops);

	ret = cdev_add(&chardrvs_ptr->new_cdevice, chardrvs_ptr->dev_nr, 1);
	if (ret) {
		pr_err("Could not register char dev: %d\n", ret);
		goto err_destruct_device;
	}

	ret = setup_chardrvs(chardrvs_ptr);
	if (ret) {
		pr_err("Could not create FIFO\n");
		goto err_unregister_cdev;
	}
#ifdef CHARDRVS_DBG
	chardrvs_ptr->proc_file = proc_create("chardrvsinfo", S_IRUSR, NULL, &chardrvs_proc_fops);
	if (!chardrvs_ptr->proc_file) {
		pr_err("couldn't create /proc/chardrvsinfo\n");
		ret = -ENOMEM;
		goto err_setup_chardrvs;
	}

	chardrvs_ptr->dbg_dir = debugfs_create_dir("chardrvsdbg", NULL);
	if(!chardrvs_ptr->dbg_dir) {
		pr_err("couldn't create /sys/kernel/debug/chardrvsdbg\n");
		ret = -ENOMEM;
		goto err_procfs;
	}

	chardrvs_ptr->dbg_file = debugfs_create_file("meminfo", S_IRUSR, chardrvs_ptr->dbg_dir,
			NULL, &chardrvs_dbg_fops);
	if(!chardrvs_ptr->dbg_file) {
		pr_err("couldn't create /sys/kernel/debug/chardrvsdbg/meminfo\n");
		ret = -ENOMEM;
		goto err_dbg_dir;
	}
#endif
	pr_info("chardrvs driver registered with major %d\n", MAJOR(chardrvs_ptr->dev_nr));
	return 0;

#ifdef CHARDRVS_DBG
err_dbg_dir:
	debugfs_remove(chardrvs_ptr->dbg_dir);
err_procfs:
	proc_remove(chardrvs_ptr->proc_file);
err_setup_chardrvs:
	uninstall_chardrvs(chardrvs_ptr);
#endif
err_unregister_cdev:
	cdev_del(&chardrvs_ptr->new_cdevice);
err_destruct_device:
	device_destroy(chardrvs_ptr->new_class, chardrvs_ptr->dev_nr);
err_destruct_class:
	class_destroy(chardrvs_ptr->new_class);
err_unregister_chrdev:
	unregister_chrdev_region(chardrvs_ptr->dev_nr, NR_MINORS);
err_free_dev:
	kfree(chardrvs_ptr);

	return ret;
}

static void __exit chardrvs_clean(void)
{
	struct chardrvs_priv_dev *dev = GET_DEVICE();

	cdev_del(&dev->new_cdevice);
	device_destroy(dev->new_class, dev->dev_nr);
	class_destroy(dev->new_class);
	unregister_chrdev_region(dev->dev_nr, NR_MINORS);
	uninstall_chardrvs(dev);
#ifdef CHARDRVS_DBG
	proc_remove(dev->proc_file);
	debugfs_remove(dev->dbg_dir);
#endif
	kfree(dev);

	pr_info("Removing chardrvs driver\n");
}

module_init(chardrvs_init);
module_exit(chardrvs_clean);

