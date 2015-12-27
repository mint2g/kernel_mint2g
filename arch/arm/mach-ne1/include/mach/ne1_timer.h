/*
 * linux/asm/arch/ne1_timer.h
 *
 * Copyright (C) NEC Electronics Corporation 2007, 2008
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __ASM_ARCH_NE1_TIMER_H
#define __ASM_ARCH_NE1_TIMER_H


#define	TMR_TMRCNT			0x00		/* timer counter */
#define	TMR_TMRCTRL			0x04		/* timer control */
#define	TMR_TMRRST			0x08		/* timer reset */
#define	TMR_GTOPSTA			0x0c		/* GTO pulse start */
#define	TMR_GTOPEND			0x10		/* GTO pulse end */
#define	TMR_GTICTRL			0x14		/* GTI control */
#define	TMR_GTIRISECAP			0x18		/* GTI rising edge capture */
#define	TMR_GTIFALLCAP			0x1c		/* GTI falling edge capture */
#define	TMR_GTINT			0x20		/* GT interrupt */
#define	TMR_GTINTEN			0x24		/* GT interrupt enable */
#define	TMR_PSCALE			0x28		/* pre-scaler */

#define	TMRCTRL_CE			0x00000001
#define	TMRCTRL_CAE			0x00000002

#define	GTINT_TCI			0x00000010
#define	GTINTEN_TCE			0x00000010


#endif /* __ASM_ARCH_NE1_TIMER_H */

