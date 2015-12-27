/* linux/tsu8111.h
 *
 * header for TSU8111 USB switch device.
 *
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

#ifndef __TSU8111_H__
#define __TSU8111_H__

#include <linux/types.h>

struct tsu8111_platform_data {
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
};

#define FSA9480_ATTACHED (1)
#define FSA9480_DETACHED (0)

void tsu8111_set_switch(const char* buf);
ssize_t tsu8111_get_switch(char* buf);

#ifdef CONFIG_VIDEO_MHL_V1
void tsu8111_CheckAndHookAudioDock(void);
#endif

#endif /* __TSU8111_H__ */

