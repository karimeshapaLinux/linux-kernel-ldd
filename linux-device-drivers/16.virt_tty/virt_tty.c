/*
 * This tty driver is a working simplified
 * version with some modifications and fixes
 * of the ldd book tty driver.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>

MODULE_AUTHOR("Arabic Linux Community");
MODULE_DESCRIPTION("virt_tty ldd");
MODULE_LICENSE("GPL");

#define SEND_TIME	(1000)
#define ROOM_SIZE	(255)
#define INITIALIZE_VIRT_TTY_DRV(_drv, _owner, _drvname, _name, _mjr, _type,\
			_subtype, _flags, _inittermios, _c_cflag) {\
	_drv->owner = _owner; \
	_drv->driver_name = _drvname; \
	_drv->name = _name; \
	_drv->major =  _mjr;\
	_drv->type = _type; \
	_drv->subtype = _subtype; \
	_drv->flags = _flags; \
	_drv->init_termios = _inittermios; \
	_drv->init_termios.c_cflag = _c_cflag; \
}

struct virt_tty_dev {
	struct tty_struct *tty;
	int open_count;
	struct mutex mutex;
	struct timer_list timer;
};

static struct virt_tty_dev *virt_tty_prv;
static struct tty_port virt_tty_port;
static struct tty_driver *virt_tty_drv;

static void timer_cbk(struct timer_list *t)
{
	struct virt_tty_dev *dev = from_timer(dev, t, timer);
	char data[] = "Hello ttyvirt driver, ";

	if (!dev)
		return;
	tty_insert_flip_string(dev->tty->port, data, sizeof(data));
	tty_flip_buffer_push(dev->tty->port);
	mod_timer(&dev->timer, jiffies +
		msecs_to_jiffies(SEND_TIME));
}

static int virttty_open(struct tty_struct *tty, struct file *file)
{
	dev_info(tty->dev, "virttty is opened...\n");
	
	mutex_lock(&virt_tty_prv->mutex);
	tty->driver_data = virt_tty_prv;
	virt_tty_prv->tty = tty;
	virt_tty_prv->open_count++;
	if (virt_tty_prv->open_count == 1) {
		timer_setup(&virt_tty_prv->timer, timer_cbk, 0);
		mod_timer(&virt_tty_prv->timer, jiffies +
			msecs_to_jiffies(SEND_TIME));
	}

	mutex_unlock(&virt_tty_prv->mutex);

	return 0;
}

static void do_close(struct virt_tty_dev *dev)
{
	mutex_lock(&dev->mutex);

	if (!dev->open_count)
		goto exit;
	dev->open_count--;
	if (dev->open_count <= 0) 
		del_timer(&dev->timer);
exit:
	mutex_unlock(&dev->mutex);
}

static void virttty_close(struct tty_struct *tty, struct file *file)
{
	struct virt_tty_dev *dev = tty->driver_data;
	if (dev)
		do_close(dev);
}

static int virttty_write(struct tty_struct *tty,
		      const unsigned char *buffer, int count)
{
	struct virt_tty_dev *dev = tty->driver_data;
	int i;
	if (!dev)
		return -ENODEV;
	mutex_lock(&dev->mutex);
	if (!dev->open_count)
		goto exit;

	/* Simulate sending to our HW device */
	for (i = 0; i < count; ++i)
		dev_info(tty->dev, "%d", buffer[i]);
exit:
	mutex_unlock(&dev->mutex);
	return count;
}

static unsigned int virttty_write_room(struct tty_struct *tty)
{
	dev_info(tty->dev, "Room is requested...\n");
	return ROOM_SIZE;
}

static void virttty_set_termios(struct tty_struct *tty, const struct ktermios *old_termios)
{
	/* Check old_termios & new */

	/* write HW control registers.
	 * Set baud rate
	 * Set parity odd/even
	 * Set stop bits 1/2
	 * Set bit size 7/8
	 * Set flow control
	 */

	dev_info(tty->dev,
		"Sending control register configurations to HW device triggered by user-space\n");
}

static int virttty_tiocmget(struct tty_struct *tty)
{
	/* Read HW status registers.
	 * Data Terminal
	 * Request to send
	 * Clear to send
	 * Data set ready
	 * Data carrier detect
	 */
	dev_info(tty->dev,
		"Getting device control register configurations needed by kernel\n");
	return 0;
}

static int virttty_tiocmset(struct tty_struct *tty, unsigned int set,
			 unsigned int clear)
{
	/*
	 * Set & clear HW registers.
	 * Data Terminal Ready
	 * Request to Send
	 */
	dev_info(tty->dev,
		"Setting device control register configurations needed by kernel\n");
	return 0;
}

static const struct tty_operations tty_ops = {
	.open = virttty_open,
	.close = virttty_close,
	.write = virttty_write,
	.write_room = virttty_write_room,
	.set_termios = virttty_set_termios,
	.tiocmget = virttty_tiocmget,
	.tiocmset = virttty_tiocmset,
};

static int __init virttty_init(void)
{
	int ret;

	virt_tty_drv = tty_alloc_driver(1, 0);
	if (!virt_tty_drv)
		return -ENOMEM;
	INITIALIZE_VIRT_TTY_DRV(virt_tty_drv, THIS_MODULE,
		"ttyvirt", "ttyvirtdrv",0,
		TTY_DRIVER_TYPE_SERIAL, SYSTEM_TYPE_TTY,
		(TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV),
		tty_std_termios, B9600);
	tty_set_operations(virt_tty_drv, &tty_ops);
	tty_port_init(&virt_tty_port);
	tty_port_link_device(&virt_tty_port, virt_tty_drv, 0);

	ret = tty_register_driver(virt_tty_drv);
	if (ret) {
		pr_err("Failed to register virt tty driver %d\n", ret);
		goto err_tty_and_prt;
		return ret;
	}
	tty_register_device(virt_tty_drv, 0, NULL);
	virt_tty_prv = kmalloc(sizeof(struct virt_tty_dev), GFP_KERNEL);
	if (!virt_tty_prv) {
		ret = -ENOMEM;
		goto unreg_dev;	
	}
	mutex_init(&virt_tty_prv->mutex);
	virt_tty_prv->open_count = 0;
	pr_info("virttty driver virttty_init\n");
	return 0;

unreg_dev:
	tty_unregister_device(virt_tty_drv, 0);
err_tty_and_prt:
	tty_port_destroy(&virt_tty_port);
	tty_unregister_driver(virt_tty_drv);
	return ret;
}

static void __exit virttty_exit(void)
{
	tty_unregister_device(virt_tty_drv, 0);
	tty_port_destroy(&virt_tty_port);
	tty_unregister_driver(virt_tty_drv);

	if (virt_tty_prv) {
		while (virt_tty_prv->open_count)
			do_close(virt_tty_prv);
		del_timer(&virt_tty_prv->timer);
		kfree(virt_tty_prv);
		virt_tty_prv = NULL;
	}
	pr_info("virttty driver virttty_exit\n");
}

module_init(virttty_init);
module_exit(virttty_exit);
