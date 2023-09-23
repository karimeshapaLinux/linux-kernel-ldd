#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/bcd.h>
#include <linux/i2c.h>
#include <linux/rtc.h>
#include <linux/property.h>

MODULE_AUTHOR("Arabic Linux Community");
MODULE_DESCRIPTION("ds1307_rtc ldd");
MODULE_LICENSE("GPL");

#define REG_OFFSET_SECS     (0x00)
#define REG_OFFSET_MINS     (0x01)
#define REG_OFFSET_HOURS    (0x02)
#define REG_OFFSET_DAY      (0x03)
#define REG_OFFSET_DATE     (0x04)
#define REG_OFFSET_MONTH    (0x05)
#define REG_OFFSET_YEAR     (0x06)

#define BLOCK_START_ADD     (0x00)
#define NR_REGS             (7)

/*
 * For debugging purposes.
 */
u8 rtc_info[NR_REGS] = {
	[REG_OFFSET_SECS] = 1,
	[REG_OFFSET_MINS] = 1,
	[REG_OFFSET_HOURS] = 1,
	[REG_OFFSET_DAY] = 1,
	[REG_OFFSET_DATE] = 1,
	[REG_OFFSET_MONTH] = 1,
	[REG_OFFSET_YEAR] = 71
};
struct ds1307_priv_dev {
	struct device *dev;
	struct rtc_device *rtc;
	struct i2c_client *client;
	const char *name;
};

static const struct of_device_id ds1307_of_match_dev[] = {
	{.compatible = "org,ds1307rtc",},
	{}
};

static int ds1307_get_time(struct device *dev, struct rtc_time *t)
{

	struct ds1307_priv_dev *priv_dev;
	int tmp;
    
	priv_dev = dev_get_drvdata(dev);

	/*
	 * Let it generic for testing and teaching purposes.
	 * Also tried using i2c_smbus_read_block_data()
	 * but seems adapter driver does not support that,
	 * look at /drivers/i2c/busses/i2c-bcm2835.c
	 * bcm2835_i2c_func() -> I2C_FUNC_SMBUS_EMUL not
	 * including I2C_FUNC_SMBUS_READ_BLOCK_DATA.
	 * Synch & concurrent access neglected.
	 */
	rtc_info[REG_OFFSET_SECS] = i2c_smbus_read_byte_data(
		priv_dev->client, REG_OFFSET_SECS);
	rtc_info[REG_OFFSET_MINS] = i2c_smbus_read_byte_data(
		priv_dev->client, REG_OFFSET_MINS);
	rtc_info[REG_OFFSET_HOURS] = i2c_smbus_read_byte_data(
		priv_dev->client, REG_OFFSET_HOURS);
	rtc_info[REG_OFFSET_DAY] = i2c_smbus_read_byte_data(
		priv_dev->client, REG_OFFSET_DAY);
	rtc_info[REG_OFFSET_DATE] = i2c_smbus_read_byte_data(
		priv_dev->client, REG_OFFSET_DATE);
	rtc_info[REG_OFFSET_MONTH] = i2c_smbus_read_byte_data(
		priv_dev->client, REG_OFFSET_MONTH);
	rtc_info[REG_OFFSET_YEAR] = i2c_smbus_read_byte_data(
		priv_dev->client, REG_OFFSET_YEAR);

	t->tm_sec = bcd2bin(rtc_info[REG_OFFSET_SECS] & 0x7f);
	t->tm_min = bcd2bin(rtc_info[REG_OFFSET_MINS] & 0x7f);
	tmp = rtc_info[REG_OFFSET_HOURS] & 0x3f;
	t->tm_hour = bcd2bin(tmp);
	t->tm_wday = bcd2bin(rtc_info[REG_OFFSET_DAY] & 0x07) - 1;
	t->tm_mday = bcd2bin(rtc_info[REG_OFFSET_DATE] & 0x3f);
	tmp = rtc_info[REG_OFFSET_MONTH] & 0x1f;
	t->tm_mon = bcd2bin(tmp) - 1;
	t->tm_year = bcd2bin(rtc_info[REG_OFFSET_YEAR]) + 100;

	return 0;
}


static int ds1307_set_time(struct device *dev, struct rtc_time *t)
{

	struct ds1307_priv_dev *priv_dev;
	int tmp;

	priv_dev = dev_get_drvdata(dev);

	rtc_info[REG_OFFSET_SECS] = bin2bcd(t->tm_sec);
	rtc_info[REG_OFFSET_MINS] = bin2bcd(t->tm_min);
	rtc_info[REG_OFFSET_HOURS] = bin2bcd(t->tm_hour);
	rtc_info[REG_OFFSET_DAY] = bin2bcd(t->tm_wday + 1);
	rtc_info[REG_OFFSET_DATE] = bin2bcd(t->tm_mday);
	rtc_info[REG_OFFSET_MONTH] = bin2bcd(t->tm_mon + 1);
	tmp = t->tm_year - 100;
	rtc_info[REG_OFFSET_YEAR] = bin2bcd(tmp);
	
	/* 
	 * Let it generic for testing and teaching purposes.
	 * Also tried using i2c_smbus_write_block_data()
	 * but seems adapter driver does not support that,
	 * look at /drivers/i2c/busses/i2c-bcm2835.c
	 * bcm2835_i2c_func() -> I2C_FUNC_SMBUS_EMUL not
	 * including I2C_FUNC_SMBUS_READ_BLOCK_DATA.
	 * Synch & concurrent access neglected.
	 */
	i2c_smbus_write_byte_data(priv_dev->client, REG_OFFSET_SECS,
		rtc_info[REG_OFFSET_SECS]);
	i2c_smbus_write_byte_data(priv_dev->client, REG_OFFSET_MINS,
		rtc_info[REG_OFFSET_MINS]);
	i2c_smbus_write_byte_data(priv_dev->client, REG_OFFSET_HOURS,
		rtc_info[REG_OFFSET_HOURS]);
	i2c_smbus_write_byte_data(priv_dev->client, REG_OFFSET_DAY,
		rtc_info[REG_OFFSET_DAY]);
	i2c_smbus_write_byte_data(priv_dev->client, REG_OFFSET_DATE,
		rtc_info[REG_OFFSET_DATE]);
	i2c_smbus_write_byte_data(priv_dev->client, REG_OFFSET_MONTH,
		rtc_info[REG_OFFSET_MONTH]);
	i2c_smbus_write_byte_data(priv_dev->client, REG_OFFSET_YEAR,
		rtc_info[REG_OFFSET_YEAR]);

	return 0;
}

static const struct rtc_class_ops ds1307_ops = {
	.read_time	= ds1307_get_time,
	.set_time	= ds1307_set_time
};

static int ds1307_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct ds1307_priv_dev *priv_dev;
	int err;

	priv_dev = devm_kzalloc(&client->dev, sizeof(struct ds1307_priv_dev), GFP_KERNEL);
	if (!priv_dev)
		return -ENOMEM;

	priv_dev->dev = &client->dev;
	priv_dev->name = client->name;
	priv_dev->client = client;

	pr_info("devm_kzalloc ds1307_probe");
	priv_dev->rtc = devm_rtc_allocate_device(priv_dev->dev);
	if (IS_ERR(priv_dev->rtc))
		return PTR_ERR(priv_dev->rtc);
	priv_dev->rtc->ops = &ds1307_ops;
	dev_set_drvdata(&client->dev, priv_dev);
	
	pr_info("devm_rtc_allocate_device ds1307_probe");
	err = devm_rtc_register_device(priv_dev->rtc);
	if (err)
		return err;

	return 0;
}

static struct i2c_driver ds1307_drv = {
	.probe		= ds1307_probe,
	.driver = {
		.name	= "ds1307_rtc",
		.of_match_table = of_match_ptr(ds1307_of_match_dev),
	}

};

module_i2c_driver(ds1307_drv);