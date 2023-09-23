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

/*
 * Still needed to be implemented
 * as a part of set_termios.
 */
struct cp2102_fctrl {
    u32 ulControlHandshake;
    u32 ulFlowReplace;
    u32 ulXonLimit;
    u32 ulXoffLimit;
};

int cp2102_enableif(struct usb_serial_port *port, u8 bInterfaceNumber);
int cp2102_disableif(struct usb_serial_port *port, u8 bInterfaceNumber);
int cp2102_set_lctrl(struct usb_serial_port *port,
	u8 bInterfaceNumber, u16 val);
int cp2102_get_lctrl(struct usb_serial_port *port,
    u8 bInterfaceNumber, u16 *val);
int cp2102_set_baud(struct usb_serial_port *port,
    u8 bInterfaceNumber, u32 val);
int cp2102_get_baud(struct usb_serial_port *port,
    u8 bInterfaceNumber, u32 *val);
int cp2102_get_mdmstat(struct usb_serial_port *port,
    u8 bInterfaceNumber, u8 *val);
int cp2102_set_mdmhndshk(struct usb_serial_port *port,
    u8 bInterfaceNumber, u16 val);
int cp2102_set_fctrl(struct usb_serial_port *port,
    u8 bInterfaceNumber, struct cp2102_fctrl *val);
int cp2102_get_fctrl(struct usb_serial_port *port,
    u8 bInterfaceNumber, struct cp2102_fctrl *val);
void cp2012_reset(void);
