struct virtbus_drvr {
	/*
	 * For simplicity only embed device_driver.
	 * However we can add different attributes
	 * for the bus as well.
	 */
	struct device_driver driver;
};

struct virtbus_dev {
	char *name;
	int id;
	struct virtbus_drvr *driver;
	struct device dev;
};

extern struct bus_type virtbus_type;
extern int register_virtbus_device(struct virtbus_dev *virtbusdev);
extern void unregister_virtbus_device(struct virtbus_dev *virtbusdev);
extern int register_virtbus_driver(struct virtbus_drvr *virtbusdrvr);
extern void unregister_virtbus_driver(struct virtbus_drvr *virtbusdrvr);
