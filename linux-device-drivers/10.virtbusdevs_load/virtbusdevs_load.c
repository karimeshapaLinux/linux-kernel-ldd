#include <linux/module.h>
#include <linux/device.h>
#include "virtbus.h"

MODULE_AUTHOR("Arabic Linux Community");
MODULE_DESCRIPTION("virtbusdevs Load");
MODULE_LICENSE("GPL");

void virtbus_load_release(struct device*dev)
{
	pr_info("virtbus_load released\n");
}

struct virtbus_dev virtbus_dev0 = {
	.name = "virtbus-fifo-dev",
	.id = 0,
	.dev = {
		.release = virtbus_load_release
	}
};

struct virtbus_dev virtbus_dev1 = {
	.name = "virtbus-fifo-dev",
	.id = 1,
	.dev = {
		.release = virtbus_load_release
	}
};

static int __init virtbusdevs_load_init(void)
{
	register_virtbus_device(&virtbus_dev0);
	register_virtbus_device(&virtbus_dev1);
	pr_info("virtbusdevs_load initialized\n");

	return 0;
}

static void __exit virtbusdevs_load_exit(void)
{
	unregister_virtbus_device(&virtbus_dev0);
	unregister_virtbus_device(&virtbus_dev1);
	pr_info("virtbusdevs_load exit\n");
}

module_init(virtbusdevs_load_init);
module_exit(virtbusdevs_load_exit);