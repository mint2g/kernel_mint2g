#
#***************************************************************
#
#  Component: VLX nano-kernel device driver interface (NK DDI)
#
#  Copyright (C) 2011, Red Bend Ltd.
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License Version 2
#  as published by the Free Software Foundation.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
#  You should have received a copy of the GNU General Public License Version 2
#  along with this program. If not, see <http://www.gnu.org/licenses/>.
#
#  Contributor(s):
#    Vladimir Grouzdev (vladimir.grouzdev@redbend.com)
#    Guennadi Maslov (guennadi.maslov@redbend.com)
#    Chi Dat Truong (chidat.truong@redbend.com)
#
#***************************************************************
#

# Each configuration option enables a list of files.

obj-y			:= console.o ddi.o perfmon.o
obj-$(CONFIG_PROC_FS)	+= proc.o nktags.o
obj-$(CONFIG_SMP)	+= smp.o vuart_tty.o
obj-$(CONFIG_SCHED_INFLUENCE_LOAD_BALANCING) += sched.o

# Don't use vtimer or vtick
ifeq (0, 1)
ifeq ($(CONFIG_GENERIC_CLOCKEVENTS),y)
obj-y += vtimer.o
else
obj-y += vtick.o
endif
endif

g_vlog-objs := vlog.o
obj-$(CONFIG_MODEM_VLOG) += g_vlog.o
