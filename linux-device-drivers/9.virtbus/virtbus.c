#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include "virtbus.h"

MODULE_AUTHOR("Arabic Linux Community");
MODULE_DESCRIPTION("virtbus ldd");
MODULE_LICENSE("GPL");

static int virtbus_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	if (add_uevent_var(env, "virtbus udev event"))
		return -ENOMEM;
	return 0;
}

static int virtbus_match(struct device *dev, struct device_driver *driver)
{
	return !strncmp(dev_name(dev), driver->name, strlen(driver->name));
}

static void virtbus_release(struct device *dev)
{
	dev_info(dev, "virtbus released");
}

struct device virtbus_represnted_dev = {
	.release  = virtbus_release
};

struct bus_type virtbus_type = {
	.name = "virtbus",
	.match = virtbus_match,
	.uevent  = virtbus_uevent,
};

static void virtbus_dev_release(struct device *dev)
{ 
	dev_info(dev, "virtbus device released");
}

int register_virtbus_device(struct virtbus_dev *virtbusdev)
{
	pr_info("A new device %s-%d registered to virtbus",
		virtbusdev->name, virtbusdev->id);

	virtbusdev->dev.bus = &virtbus_type;
	virtbusdev->dev.parent = &virtbus_represnted_dev;
	virtbusdev->dev.release = virtbus_dev_release;

	dev_set_name(&virtbusdev->dev,"%s-%d", virtbusdev->name, virtbusdev->id);
	return device_register(&virtbusdev->dev);
}
EXPORT_SYMBOL(register_virtbus_device);

void unregister_virtbus_device(struct virtbus_dev *virtbusdev)
{
	device_unregister(&virtbusdev->dev);
}
EXPORT_SYMBOL(unregister_virtbus_device);

int register_virtbus_driver(struct virtbus_drvr *virtbusdrvr)
{
	int ret;
	pr_info("A new driver %s registered to virtbus", virtbusdrvr->driver.name);
	virtbusdrvr->driver.bus = &virtbus_type;
	ret = driver_register(&virtbusdrvr->driver);
	
	return ret;
}
EXPORT_SYMBOL(register_virtbus_driver);

void unregister_virtbus_driver(struct virtbus_drvr *virtbusdrvr)
{
	pr_info("A driver %s unregistered to virtbus", virtbusdrvr->driver.name);
	driver_unregister(&virtbusdrvr->driver);
}
EXPORT_SYMBOL(unregister_virtbus_driver);

static int __init virtbus_init(void)
{
	int ret;

	ret = bus_register(&virtbus_type);
	if (ret) {
		pr_err("Unable to register to virtbus, %d\n",ret);
		return ret;
	}
	dev_set_name(&virtbus_represnted_dev,"virtbus");
	ret = device_register(&virtbus_represnted_dev);
	if (ret) {
		pr_err("Unable to create represnted device of virtbus %d\n",ret);
		goto unreg_bus;
	}
	pr_info("...virtbus_init...\n");
	return 0;

unreg_bus:
	bus_unregister(&virtbus_type);
	return ret;
}

static void virtbus_exit(void)
{
	pr_info("...virtbus_exit...\n");
	device_unregister(&virtbus_represnted_dev);
	bus_unregister(&virtbus_type);
}
module_init(virtbus_init);
module_exit(virtbus_exit);
