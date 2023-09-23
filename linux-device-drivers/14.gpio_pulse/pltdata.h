struct pltdata_gpiopulse {
	int enablepulse;
	int pulsetime;
	char label[16];
	struct gpio_desc *desc;
};
