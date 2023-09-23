/*
 * AN571 documentation
 * https://www.silabs.com/documents/public/application-notes/AN571.pdf
 */

#include "cp2102if.h"

#define IFC_ENABLE_REQ		(0x00)	/* Enable or disable the interface. */
#define SET_LINE_CTL_REQ	(0x03)	/* Set the line control. */
#define GET_LINE_CTL_REQ	(0x04)	/* Get the line control. */
#define SET_BAUDDIV_REQ		(0x01)	/* Set the baud rate divisor. */
#define GET_BAUDDIV_REQ		(0x02)	/* Get the baud rate divisor. */
#define SET_BAUDRATE_REQ	(0x1E)	/* Set the baud rate. */
#define GET_BAUDRATE_REQ	(0x1D)	/* Get the baud rate. */
#define SET_FLOW_REQ		(0x13)	/* Set flow control. */
#define GET_FLOW_REQ		(0x14)	/* Get flow control. */
#define GET_MDMSTS_REQ		(0x08)	/* Get modem status. */
#define SET_MHS_REQ		(0x07)	/* Set modem handshaking. */
#define RESET_REQ		(0x11)	/* Reset. */


#define wValue_IFC_ENABLE	(0x0001)
#define wValue_IFC_DISABLE	(0x0000)

int cp2102_enableif(struct usb_serial_port *port, u8 bInterfaceNumber)
{
	int status;
	struct usb_device * usbdev = port->serial->dev;

	/* AN571 page 8
	 * bmRequestType 01000001b = 65 = 0x41
	 * wIndex = interface num.
	 */
	u8 bmRequestType = 0x41;
	u8 wIndex = bInterfaceNumber;

	/*
	int usb_control_msg_send(struct usb_device *dev, __u8 endpoint, __u8 request,
			 __u8 requesttype, __u16 value, __u16 index,
			 const void *data, __u16 size, int timeout,
			 gfp_t memflags);
	*/
	status = usb_control_msg_send(usbdev, 0,
			IFC_ENABLE_REQ, bmRequestType, wValue_IFC_ENABLE,
			wIndex, NULL, 0,
			USB_CTRL_SET_TIMEOUT, GFP_KERNEL);
	if (status) {
		dev_err(&port->dev, "%s failed status: %d\n", __func__, status);
		status = usb_translate_errors(status);
	}

	return status;
}

int cp2102_disableif(struct usb_serial_port *port, u8 bInterfaceNumber)
{
	int status;
	struct usb_device * usbdev = port->serial->dev;
	
	/* AN571 page 8
	 * bmRequestType 01000001b = 65 = 0x41
	 * wIndex = interface num.
	 */
	u8 bmRequestType = 0x41;
	u8 wIndex = bInterfaceNumber;

	status = usb_control_msg_send(usbdev, 0,
			IFC_ENABLE_REQ, bmRequestType, wValue_IFC_DISABLE,
			wIndex, NULL, 0,
			USB_CTRL_SET_TIMEOUT, GFP_KERNEL);
	if (status) {
		dev_err(&port->dev, "%s failed status: %d\n", __func__, status);
		status = usb_translate_errors(status);
	}

	return status;
}

int cp2102_set_lctrl(struct usb_serial_port *port,
	u8 bInterfaceNumber, u16 val)
{

	int status;
	struct usb_device * usbdev = port->serial->dev;
	
	/* AN571 page 9
	 * bmRequestType 01000001b = 65 = 0x41
	 * wIndex = interface num.
	 */
	u8 bmRequestType = 0x41;
	u8 wIndex = bInterfaceNumber;

	status = usb_control_msg_send(usbdev, 0,
			SET_LINE_CTL_REQ, bmRequestType, val,
			wIndex, NULL, 0,
			USB_CTRL_SET_TIMEOUT, GFP_KERNEL);
	if (status) {
		dev_err(&port->dev, "%s failed status: %d\n", __func__, status);
		status = usb_translate_errors(status);
	}

	return status;
}

int cp2102_get_lctrl(struct usb_serial_port *port,
	u8 bInterfaceNumber, u16 *val)
{
	int status;
	struct usb_device * usbdev = port->serial->dev;
	
	/* AN571 page 10
	 * bmRequestType 11000001b = 193 = 0xC1
	 * wIndex = interface num.
	 */
	u8 bmRequestType = 0xC1;
	u8 wIndex = bInterfaceNumber;

	status = usb_control_msg_recv(usbdev, 0,
			GET_LINE_CTL_REQ, bmRequestType, 0,
			wIndex, val, sizeof(*val),
			USB_CTRL_SET_TIMEOUT, GFP_KERNEL);
	if (status) {
		dev_err(&port->dev, "%s failed status: %d\n", __func__, status);
		status = usb_translate_errors(status);
	}

	return status;
}

int cp2102_set_baud(struct usb_serial_port *port,
	u8 bInterfaceNumber, u32 val)
{
	int status;
	struct usb_device * usbdev = port->serial->dev;
	
	/* AN571 page 8
	 * bmRequestType 01000001b = 65 = 0x41
	 * wIndex = interface num.
	 */
	u8 bmRequestType = 0x41;
	u8 wIndex = bInterfaceNumber;
	
	/*
	 * wLength = 4 bytes (endianess concern!).
	 */
	pr_info("cp2102_set_baud value: %d \n", val);
	status = usb_control_msg_send(usbdev, 0,
		SET_BAUDRATE_REQ, bmRequestType, 0,
		wIndex, &val, sizeof(val),
		USB_CTRL_SET_TIMEOUT, GFP_KERNEL);
	if (status) {
		dev_err(&port->dev, "%s failed status: %d\n", __func__, status);
		status = usb_translate_errors(status);
		pr_info("cp2102_set_baud error: %d \n", status);
	}

	return status;
}

int cp2102_get_baud(struct usb_serial_port *port,
	u8 bInterfaceNumber, u32 *val)
{
	int status;
	struct usb_device * usbdev = port->serial->dev;
	
	/* AN571 page 9
	 * bmRequestType 01000001b = 193 = 0xC1
	 * wIndex = interface num.
	 */
	u8 bmRequestType = 0xC1;
	u8 wIndex = bInterfaceNumber;
	
	/*
	 * wLength = 4 bytes (endianess concern!).
	 */
	status = usb_control_msg_recv(usbdev, 0,
		GET_BAUDRATE_REQ, bmRequestType, 0,
		wIndex, val, sizeof(*val),
		USB_CTRL_SET_TIMEOUT, GFP_KERNEL);
	if (status) {
		dev_err(&port->dev, "%s failed status: %d\n", __func__, status);
		status = usb_translate_errors(status);
	}

	return status;
}

int cp2102_get_mdmstat(struct usb_serial_port *port,
	u8 bInterfaceNumber, u8 *val)
{
	int status;
	struct usb_device * usbdev = port->serial->dev;
	
	/* AN571 page 11
	 * bmRequestType 11000001b = 193 = 0xC1
	 * wIndex = interface num.
	 */
	u8 bmRequestType = 0xC1;
	u8 wIndex = bInterfaceNumber;

	status = usb_control_msg_recv(usbdev, 0,
			GET_MDMSTS_REQ, bmRequestType, 0,
			wIndex, val, sizeof(*val),
			USB_CTRL_SET_TIMEOUT, GFP_KERNEL);
	if (status) {
		dev_err(&port->dev, "%s failed status: %d\n", __func__, status);
		status = usb_translate_errors(status);
	}

	return status;
}

int cp2102_set_mdmhndshk(struct usb_serial_port *port,
	u8 bInterfaceNumber, u16 val)
{
	int status;
	struct usb_device * usbdev = port->serial->dev;
	
	/* AN571 page 10
	 * bmRequestType 01000001b = 65 = 0x41
	 * wIndex = interface num.
	 */
	u8 bmRequestType = 0x41;
	u8 wIndex = bInterfaceNumber;

	status = usb_control_msg_send(usbdev, 0,
			SET_MHS_REQ, bmRequestType, val,
			wIndex, NULL, 0,
			USB_CTRL_SET_TIMEOUT, GFP_KERNEL);
	if (status) {
		dev_err(&port->dev, "%s failed status: %d\n", __func__, status);
		status = usb_translate_errors(status);
	}

	return status;
}

int cp2102_set_fctrl(struct usb_serial_port *port,
	u8 bInterfaceNumber, struct cp2102_fctrl *val)
{
	return 0;
}

int cp2102_get_fctrl(struct usb_serial_port *port,
	u8 bInterfaceNumber, struct cp2102_fctrl *val)
{
	return 0;
}

void cp2012_reset(void)
{

}
