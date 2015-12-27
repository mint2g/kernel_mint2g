/* linux/fsa880.h
 *
 * header for fsa880 USB switch device.
 *
 * Copyright (c) by Wonguk Jeong <wonguk.jeong@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifndef _FSA880_H_
#define _FSA880_H_

#include <linux/types.h>
//#include <linux/mfd/88pm860x.h>


struct fsa880_platform_data {
       int intb_gpio;
       void (*usb_cb) (u8 attached);
       void (*uart_cb) (u8 attached);
       void (*charger_cb) (u8 attached);
       void (*jig_cb) (u8 attached);
       void (*reset_cb) (void);
	void (*mhl_cb)(u8 attached);
	void (*cardock_cb)(bool attached);
	void (*deskdock_cb)(bool attached);
	void (*id_open_cb)(void);
#define FSA880_ATTACHED (1)
#define FSA880_DETACHED (0)
};

void fsa880_set_switch(const char* buf);
ssize_t fsa880_get_switch(char* buf);


#endif /* _FSA880_H_ */

