#include <linux/init.h>
#include <linux/usb.h>
#include <linux/module.h>

MODULE_AUTHOR("Arabic Linux Community");
MODULE_DESCRIPTION("Dummy usb_ttl ldd");
MODULE_LICENSE("GPL");

#define CP2102_VENDOR_ID	(0x10C4)
#define CP2102_PRODUCT_ID	(0xEA60)

static struct usb_device_id usb_ttl_table[] = {
	{USB_DEVICE(CP2102_VENDOR_ID, CP2102_PRODUCT_ID)},
	{},
};
MODULE_DEVICE_TABLE(usb, usb_ttl_table);

static int usb_ttl_probe(struct usb_interface *intf,
	const struct usb_device_id *id)
{
	dev_info(&intf->dev, "usb_ttl device probed...\n");
	return 0;
}

static void usb_ttl_disconnect(struct usb_interface *intf)
{
	dev_info(&intf->dev, "usb_ttl device disconnected...\n");
}

static struct usb_driver usb_ttl_drv = {
	.name = "usb-ttl-drv",
	.id_table = usb_ttl_table,
	.probe = usb_ttl_probe,
	.disconnect = usb_ttl_disconnect,
};

static int __init usb_ttl_init(void)
{
	int ret;

	pr_info("usb_ttl_init...\n");
	ret = usb_register(&usb_ttl_drv);
	if(ret) {
		pr_info("usb_ttl_init register error\n");
		return -ret;
	}

	return 0;
}

static void __exit usb_ttl_exit(void)
{
	pr_info("usb_ttl_exit...\n");
	usb_deregister(&usb_ttl_drv);
}

module_init(usb_ttl_init);
module_exit(usb_ttl_exit);