#ifndef _USB_HW_H
#define _USB_HW_H

extern int usb_alloc_vbus_irq(void);
extern void usb_free_vbus_irq(int irq);
extern int usb_get_vbus_irq(void);
extern int usb_get_vbus_state(void);

enum vbus_irq_type {
	        VBUS_PLUG_IN,
		        VBUS_PLUG_OUT
};

extern void usb_set_vbus_irq_type(int irq, int irq_type);
#endif
