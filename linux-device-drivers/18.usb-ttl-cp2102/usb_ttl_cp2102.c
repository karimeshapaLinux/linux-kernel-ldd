/*
 * AN571 documentation
 * https://www.silabs.com/documents/public/application-notes/AN571.pdf
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/usb/serial.h>

#include "cp2102if.h"

#define DEFAULT_BAUD_RATE	(9600)
#define PARITY_ODD_MASK		(0x0010)
#define PARITY_EVEN_MASK	(0x0020)
#define STOP_1_MASK		(0x0000)
#define DATA_8_MASK		(0X0800)

#define MCR_DTR			(0x0001)
#define MCR_RTS			(0x0002)
#define MCR_CTS			(0x0010)
#define MCR_DSR			(0x0020)
#define MCR_RI			(0x0040)
#define MCR_DCD			(0x0080)
#define MCR_DTR_MASK		(0x0100)
#define MCR_RTS_MASK		(0x0200)

MODULE_AUTHOR("Arabic Linux Community");
MODULE_DESCRIPTION("serial_cp2102 ldd");
MODULE_LICENSE("GPL");

static const struct usb_device_id id_table[] = {
	{USB_DEVICE(0x10C4, 0xEA60)},
	{}
};
MODULE_DEVICE_TABLE(usb, id_table);

struct cp2102_prv_dev {
	u8 bInterfaceNumber;
	struct mutex mutex;
	u16	mcr;
	u8 msr;
	struct usb_serial_port *port;
};

static int serial_cp2102_open(struct tty_struct *tty,
	struct usb_serial_port *port)
{
	int ret;
	struct cp2102_prv_dev *prv_dev = usb_get_serial_port_data(port);
	
	dev_info(&port->dev, "serial_cp2102_open...\n");

	/* AN571 page 8
	 * Host drivers should enable the interface before
	 * sending any of the other commands specified by this document.
	 */
	ret = cp2102_enableif(port, prv_dev->bInterfaceNumber);
	tty->driver_data = port;

	/* Inform usb core to generic open usb */
	ret = usb_serial_generic_open(tty, port);

	if (ret) {
		cp2102_disableif(port, prv_dev->bInterfaceNumber);
		return ret;
	}

	return 0;
}

static void serial_cp2102_close(struct usb_serial_port *port)
{	
	struct cp2102_prv_dev *prv_dev = usb_get_serial_port_data(port);

	dev_info(&port->dev, "serial_cp2102_close...\n");

	/* Inform usb core to generic close usb */
	usb_serial_generic_close(port);

	/* Disable Interface */
	cp2102_disableif(port, prv_dev->bInterfaceNumber);
}

static void serial_cp2102_set_termios(struct tty_struct *tty,
		struct usb_serial_port *port, const struct ktermios *old_termios)
{
	speed_t baudrate;
	u16 lctrl;
	struct cp2102_prv_dev *prv_dev = usb_get_serial_port_data(port);

	dev_info(&port->dev, "serial_cp2102_set_termios...\n");

	/* Check old & new */
	if (old_termios && !tty_termios_hw_change(&tty->termios, old_termios))
		return;
	
	/*
	 * AN571 page 9
	 * bits 3-0: Stop bits.
	 * bits 7-4: Parity setting.
	 * bits 15-8: Word length,
	 * legal values are 5, 6, 7 and 8.
	 */

	mutex_lock(&prv_dev->mutex);

	/* Set baud rate */
	baudrate = tty_get_baud_rate(tty);
	pr_info("Set baud rate value: %d \n", baudrate);
	if (baudrate > 0) {
		if (cp2102_set_baud(port, prv_dev->bInterfaceNumber, (u32)baudrate))
			baudrate = DEFAULT_BAUD_RATE;
		
		/* Encode baud rate to our termios */
		tty_encode_baud_rate(tty, baudrate, baudrate);
	}

	/*
	 * A lot of options,
	 * no need to complicate.
	 */

	/* Set parity, odd/even */
	if (C_PARENB(tty)) {
		if (C_PARODD(tty))
				lctrl |= PARITY_ODD_MASK;
			else
				lctrl |= PARITY_EVEN_MASK;
	}

	/* Set 1 stop bit default */
	lctrl |= STOP_1_MASK;

	/* Set 8 bit size default */
	lctrl |= DATA_8_MASK;

	/* write line control */
	cp2102_set_lctrl(port, prv_dev->bInterfaceNumber, lctrl);

	mutex_unlock(&prv_dev->mutex);

	/* No flow control */
}

static int serial_cp2102_tiocmset(struct tty_struct *tty,
		unsigned int set, unsigned int clear)
{
	struct usb_serial_port *port;
	struct cp2102_prv_dev *prv_dev;
	u16 mcr;
	int ret;

	port = tty->driver_data;
	prv_dev = usb_get_serial_port_data(port);
	dev_info(&port->dev, "serial_cp2102_tiocmset...\n");

	/* AN571 page 10
	 * bit 0: DTR state (Data Terminal Ready).
	 * bit 1: RTS state (Request to Send).
	 * bits 2–7: reserved.
	 * bit 8: DTR mask, if clear, DTR will not be changed.
	 * bit 9: RTS mask, if clear, RTS will not be changed.
	 * bits 10–15: reserved.
	 */

	mutex_lock(&prv_dev->mutex);

	if (set & TIOCM_RTS) {
		mcr |= MCR_RTS;
		mcr |= MCR_RTS_MASK;
	}
	if (set & TIOCM_DTR) {
		mcr |= MCR_RTS;
		mcr |= MCR_DTR_MASK;
	}
	if (clear & TIOCM_RTS) {
		mcr &= ~MCR_RTS;
		mcr |= MCR_RTS_MASK;
	}
	if (clear & TIOCM_DTR) {
		mcr &= ~MCR_DTR;
		mcr |= MCR_DTR_MASK;
	}
	prv_dev->mcr = mcr;

	/* 
	 * write modem control, called here,
	 * modem handshaking.
	 */
	ret = cp2102_set_mdmhndshk(port, prv_dev->bInterfaceNumber, mcr);

	mutex_unlock(&prv_dev->mutex);

	return ret; 
}

static int serial_cp2102_tiocmget(struct tty_struct *tty)
{
	struct usb_serial_port *port;
	struct cp2102_prv_dev *prv_dev;
	u8 msr;
	int ret;

	port = tty->driver_data;
	prv_dev = usb_get_serial_port_data(port);
	dev_info(&port->dev, "serial_cp2102_tiocmget...\n");

	/* 
	 * read modem status.
	 */
	ret = cp2102_get_mdmstat(port, prv_dev->bInterfaceNumber, &msr);
	if(ret)
		return ret;

	/* AN571 page 11
	 * bit 0: DTR state (as set by host or by handshaking logic in CP210x).
	 * bit 1: RTS state (as set by host or by handshaking logic in CP210x).
	 * bits 2–3: reserved.
	 * bit 4: CTS state (as set by end device).
	 * bit 5: DSR state (as set by end device).
	 * bit 6: RI state (as set by end device).
	 * bit 7: DCD state (as set by end device).
	 */
	ret = ((msr & MCR_DTR) ? TIOCM_DTR : 0) /* Data Terminal is set */
		|((msr & MCR_RTS) ? TIOCM_RTS : 0)	/* Request to send is set */
		|((msr & MCR_CTS) ? TIOCM_CTS : 0)	/* Clear to send is set */
		|((msr & MCR_DSR) ? TIOCM_DSR : 0)	/* Data set ready is set*/
		|((msr & MCR_RI)? TIOCM_RI  : 0)
		|((msr & MCR_DCD) ? TIOCM_CD  : 0);  /* Data carrier detect is set */
	prv_dev->msr = msr;

	return ret;
}

static int serial_cp2102_attach(struct usb_serial *serial)
{
	dev_info(&serial->dev->dev, "serial_cp2102_attach...\n");
	return 0;
}

static void serial_cp2102_disconnect(struct usb_serial *serial)
{
	dev_info(&serial->dev->dev, "serial_cp2102_disconnect...\n");
}

static int serial_cp2102_port_probe(struct usb_serial_port *port)
{
	struct cp2102_prv_dev *prv_dev =  devm_kzalloc(&port->dev,
		sizeof(struct cp2102_prv_dev), GFP_KERNEL);
	if (!prv_dev)
		return -ENOMEM;
	prv_dev->bInterfaceNumber =
		port->serial->interface->cur_altsetting->desc.bInterfaceNumber;
	
	usb_set_serial_port_data(port, prv_dev);
	prv_dev->port = port;

	dev_info(&port->dev, "serial_cp2102_port_probe...\n");
	return 0;
}

static void serial_cp2102_port_remove(struct usb_serial_port *port)
{
	dev_info(&port->dev, "serial_cp2102_port_remove...\n");
}

static void serial_cp2102_release(struct usb_serial *serial)
{
	dev_info(&serial->dev->dev, "serial_cp2102_release...\n");
}

static struct usb_serial_driver serial_cp2102 = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "serial_cp2102",
	},
	.id_table = id_table,
	.num_ports = 1,
	.bulk_in_size = 256,
	.bulk_out_size = 256,
	.open = serial_cp2102_open,
	.close = serial_cp2102_close,
	.set_termios = serial_cp2102_set_termios,
	.tiocmset = serial_cp2102_tiocmset,
	.tiocmget = serial_cp2102_tiocmget,
	.attach = serial_cp2102_attach,
	.disconnect = serial_cp2102_disconnect,
	.port_probe = serial_cp2102_port_probe,
	.port_remove = serial_cp2102_port_remove,
	.release = serial_cp2102_release,
};

static struct usb_serial_driver * const serial_cp2102_drvs[] = {
	&serial_cp2102,
	NULL
};


module_usb_serial_driver(serial_cp2102_drvs, id_table);
