#include <linux/module.h>
#include <linux/platform_device.h>
#include "pltdata.h"

#define NR_FIFO_DEVICES	(2)

MODULE_AUTHOR("Arabic Linux Community");
MODULE_DESCRIPTION("pltdevcs Load");
MODULE_LICENSE("GPL");

void pltdevs_load_release(struct device*dev)
{
	pr_info("pltdevs_load released\n");
}

struct pltdata_fifo pltdata_fifo_lst[NR_FIFO_DEVICES] = {
	[0] = {0xAA, 8},
	[1] = {0xBB, 8}
};

struct platform_device plt_dev0 = {
	.name = "plt-fifo-dev",
	.id = 0,
	.dev = {
		.platform_data = &pltdata_fifo_lst[0],
		.release = pltdevs_load_release
	}
};

struct platform_device plt_dev1 = {
	.name = "plt-fifo-dev",
	.id = 1,
	.dev = {
		.platform_data = &pltdata_fifo_lst[1],
		.release = pltdevs_load_release
	}
};

struct platform_device *pltdevices[NR_FIFO_DEVICES] = {
	&plt_dev0,
	&plt_dev1
};

static int __init pltdevs_load_init(void)
{
	/*
	 * Register one by one.
	 * platform_device_register(&plt_dev0);
	 * platform_device_register(&plt_dev1);
	 */

	/*
	 * Adding group of devices.
	 */
	platform_add_devices(pltdevices, NR_FIFO_DEVICES);
	pr_info("pltdevs_load initialized\n");

	return 0;
}

static void __exit pltdevs_load_exit(void)
{
	platform_device_unregister(&plt_dev0);
	platform_device_unregister(&plt_dev1);
	pr_info("pltdevs_load exit\n");
}

module_init(pltdevs_load_init);
module_exit(pltdevs_load_exit);