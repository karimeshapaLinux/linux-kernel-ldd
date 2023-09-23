#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/kfifo.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/of_platform.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include "pltdata.h"

MODULE_AUTHOR("Arabic Linux Community");
MODULE_DESCRIPTION("pltdrv_gpiopulse ldd");
MODULE_LICENSE("GPL");

#define DEVICE_NAME			"pltdrv-gpiopulse"
#define DEVICE_CLASS		"pltdrv-gpiopulseclass"

static struct class *class;
struct gpiopulse_prvt_devc {
	struct device *device;
	struct timer_list timer;
	struct pltdata_gpiopulse plt_data_prv;
};

static void timer_cbk(struct timer_list * timer)
{
	struct gpiopulse_prvt_devc *priv_dev;
	int val;
	priv_dev = container_of(timer, struct gpiopulse_prvt_devc, timer);

	/* Toggle pin */
	val = gpiod_get_value(priv_dev->plt_data_prv.desc);
	gpiod_set_value(priv_dev->plt_data_prv.desc, !val);
	if (priv_dev->plt_data_prv.enablepulse)
		mod_timer(timer, jiffies + msecs_to_jiffies(
			priv_dev->plt_data_prv.pulsetime));
}

static ssize_t enablepulse_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	long enablepulse; 
	int ret;
	struct gpiopulse_prvt_devc * priv_dev = dev_get_drvdata(dev);

	ret = kstrtol(buf, 10, &enablepulse);
	if(ret)
		return ret;
	priv_dev->plt_data_prv.enablepulse = enablepulse;
	if (enablepulse)
		mod_timer(&priv_dev->timer, jiffies + msecs_to_jiffies(
			priv_dev->plt_data_prv.pulsetime));

	return count;
}

static ssize_t pulsetime_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	long pulsetime; 
	int ret;
	struct gpiopulse_prvt_devc * priv_dev = dev_get_drvdata(dev);

	ret = kstrtol(buf, 10, &pulsetime);
	if(ret)
		return ret;
	priv_dev->plt_data_prv.pulsetime = pulsetime;

	return count;
}

static ssize_t label_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct gpiopulse_prvt_devc * priv_dev = dev_get_drvdata(dev);
	return sprintf(buf, "sysfs show : device label %s\n",
		priv_dev->plt_data_prv.label);
}

static DEVICE_ATTR_WO(enablepulse);
static DEVICE_ATTR_WO(pulsetime);
static DEVICE_ATTR_RO(label);

static struct attribute * gpiopulse_attrs[] = {
	&dev_attr_enablepulse.attr,
	&dev_attr_pulsetime.attr,
	&dev_attr_label.attr,
	NULL
};

static struct attribute_group gpiopulse_attr_grp = {
	.attrs = gpiopulse_attrs
};

static const struct attribute_group *gpiopulse_attr_grps[] = {
	&gpiopulse_attr_grp,
	NULL
};

static const struct of_device_id pltdrv_dt_matchtbl_devcs[] = {
	{.compatible = "org,pltdrv-gpiopulse",},
	{},
};

int parse_dt_arg_dev(struct platform_device *ofdev,
	struct pltdata_gpiopulse *plt_data)
{
	struct device *dev = &ofdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *child = NULL; 
	const char *label;
	int gpio;
	int res;

	if (!np)
		return -EINVAL;
	
	child = of_get_next_child(np, NULL);

	gpio = of_get_gpio(child, 0);
	res = devm_gpio_request(dev, gpio, ofdev->name);
	if (res) {
		dev_err(dev, "Failed request gpio\n");
		return -ENOMEM;
	}

	plt_data->desc = gpio_to_desc(gpio);

	if(of_property_read_string(child,"label", &label)) {
		dev_info(dev,"Could not get label property\n");
		return -EINVAL;		
	}
	strcpy(plt_data->label,label);

	if(of_property_read_s32(np,"org,enablepulse",&plt_data->enablepulse)){
		dev_info(dev,"Could not get enablepulse property\n");
		return -EINVAL;
	}
	if(of_property_read_s32(np,"org,pulsetime",&plt_data->pulsetime) ){
		dev_info(dev,"Could not get pulsetime property\n");
		return -EINVAL;
	}

	return 0;
}

/**
 * Platform bus binding code, called when device detected
 */
static int pltdrv_gpiopulse_probe(struct platform_device *ofdev)
{
	int ret;
	struct gpiopulse_prvt_devc * priv_dev;
	struct device *dev = &ofdev->dev;

	dev_info(dev,"pltdrv_gpioplulse: plt Device detected\n");

	priv_dev = (struct gpiopulse_prvt_devc *)devm_kzalloc(dev,
					sizeof(struct gpiopulse_prvt_devc), GFP_KERNEL);
	
	if(IS_ERR(priv_dev)) {
		dev_err(dev, "Could not allocate memory for gpiopulse_prvt_devc\n");
		return -ENOMEM;
	}
	ret = parse_dt_arg_dev(ofdev, &priv_dev->plt_data_prv);
	if(ret)
		return ret;

	dev_info(dev,"Device enablepulse: %d\n",
		priv_dev->plt_data_prv.enablepulse);
	dev_info(dev,"Device pulsetime: %d\n",
		priv_dev->plt_data_prv.pulsetime);
	dev_info(dev,"Device label: %s\n",
		priv_dev->plt_data_prv.label);

	priv_dev->device = device_create_with_groups(class,
						dev, 0, NULL, gpiopulse_attr_grps,
						priv_dev->plt_data_prv.label);
	if (IS_ERR(priv_dev->device)) {
		ret = PTR_ERR(priv_dev->device);
		dev_err(dev,"Could not create device: %d\n", ret);
		return ret;
	}

	ret = gpiod_direction_output(priv_dev->plt_data_prv.desc,1);
	if(ret){
		dev_err(dev,"Failed set gpio direction\n");
		return ret;
	}

	timer_setup(&priv_dev->timer, timer_cbk, 0);
	mod_timer(&priv_dev->timer, jiffies + msecs_to_jiffies(
		priv_dev->plt_data_prv.pulsetime));
	
	dev_set_drvdata(dev, priv_dev);
	dev_set_drvdata(priv_dev->device, priv_dev);
	
	return 0;
}

static int pltdrv_gpiopulse_remove(struct platform_device *ofdev)
{
	struct gpiopulse_prvt_devc * priv_dev = dev_get_drvdata(&ofdev->dev);
	del_timer(&priv_dev->timer);
	device_unregister(priv_dev->device);
	dev_info(&ofdev->dev,"pltdrv_gpiopulse_remove device\n");

	return 0;
}

struct platform_driver pltdrv_gpiopulse = {
	.probe = pltdrv_gpiopulse_probe,
	.remove = pltdrv_gpiopulse_remove,
	.driver = {
		.name = "plt-gpiopulse-dev",
		.of_match_table = of_match_ptr(pltdrv_dt_matchtbl_devcs)
	}
};

static int __init pltdrv_gpiopulse_init(void)
{
	int ret;

	class = class_create(THIS_MODULE, DEVICE_CLASS);
	if (IS_ERR(class)) {
		ret = PTR_ERR(class);
		pr_err("failed to create class: %d\n", ret);
		return ret;
	}
	platform_driver_register(&pltdrv_gpiopulse);
	pr_info("pltdrv_gpiopulse_init Init\n");

	return 0;
}

static void __exit pltdrv_gpiopulse_exit(void)
{
	platform_driver_unregister(&pltdrv_gpiopulse);
	class_destroy(class);
	
	pr_info("pltdrv_gpiopulse_exit exit\n");
}

module_init(pltdrv_gpiopulse_init);
module_exit(pltdrv_gpiopulse_exit);