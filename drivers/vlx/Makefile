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

#
# Makefile for the input core drivers.
#

# Each configuration option enables a list of files.

#VLX virtual event
obj-$(CONFIG_INPUT_VEVDEV_BE)	+= vevdev-be.o
obj-$(CONFIG_INPUT_VEVDEV_FE)	+= vevdev-fe.o

#VLX virtual lcd
obj-$(CONFIG_FB_VLCD_BACKEND)     += vlcd-be.o vlcd-vogl.o
obj-$(CONFIG_FB_VLCD_FRONTEND)    += vlcd-fe.o

obj-$(CONFIG_VDISPLAY_BACKEND)  += vdisplay-be.o vdisplay-common.o
obj-$(CONFIG_VDISPLAY_FRONTEND) += vdisplay-fe.o vdisplay-common.o

#VLX virtual audio
obj-$(CONFIG_SND_VAUDIO_BE)	+= vaudio-be.o
obj-$(CONFIG_SND_VAUDIO_FE)	+= vaudio-fe.o

#VLX virtual video
obj-$(CONFIG_VVIDEO_BE)		+= vvideo-be.o
obj-$(CONFIG_VVIDEO_BE_HW)	+= vvideo-be-hw.o
obj-$(CONFIG_VVIDEO_FE)		+= vvideo-fe.o vvideo-util.o

#VLX virtual Real Time Clock frontend
obj-$(CONFIG_VRTC_FE)		+= vrtc-fe.o

#VLX virtual Real Time Clock backend
obj-$(CONFIG_VRTC_BE)		+= vrtc-be.o

#VLX virtual watchdog
obj-$(CONFIG_VWDT_FRONTEND)       += vwatchdog-sf.o
obj-$(CONFIG_VWDT_BACKEND)        += vwatchdog-sb.o

obj-$(CONFIG_VUART_FRONTEND)	  += osware-vuart.o

#VLX virtual ethernet
obj-$(CONFIG_NKERNEL_VETH)	+= veth.o

#VLX virtual block device
obj-$(CONFIG_VBD_FE)            += vlx-vbd-fe.o
obj-$(CONFIG_VBD_BE)            += vlx-vbd-be.o

#VLX virtual block device v.2
obj-$(CONFIG_VBD2_FE)           += vlx-vbd2-fe.o vlx-vipc.o
obj-$(CONFIG_VBD2_BE)           += vlx-vbd2-be.o

#VLX virtual pipe
obj-$(CONFIG_VPIPE)		+= vpipe.o

#VLX virtual user land pipe
obj-$(CONFIG_VUPIPE)		+= vupipe.o

#VLX virtual stream
obj-$(CONFIG_VBPIPE)		+= vbpipe.o

#VLX virtual memory technology device
obj-$(CONFIG_VMQ)		+= vlx-vmq.o
obj-$(CONFIG_VMTD_FE)		+= vmtd-fe.o vlx-vipc.o
obj-$(CONFIG_VMTD_BE)		+= vmtd-be.o

#VLX virtual message queue tests
obj-$(CONFIG_VMQ_TESTS)		+= vmq-tests.o

#VLX VRPC communication
obj-$(CONFIG_VLX_VRPC) += vrpc.o

#VLX virtual battery backend
obj-$(CONFIG_VBATTERY_BACKEND)  += vbattery-be.o

#VLX virtual battery frontend
obj-$(CONFIG_VBATTERY_FRONTEND) += vbattery-fe.o

#VLX virtual shared memory
obj-$(CONFIG_VSHM) += vshm.o

#VLX virtual pmem backend
obj-$(CONFIG_VPMEM_BACKEND)  += vpmem.o vpmem-be.o

#VLX virtual pmem frontend
obj-$(CONFIG_VPMEM_FRONTEND) += vpmem.o vpmem-fe.o

#VLX virtual blitter backend
obj-$(CONFIG_VBLITTER_BACKEND)  += vblitter-be.o vblitter-soft.o

#VLX virtual blitter frontend
obj-$(CONFIG_VBLITTER_FRONTEND) += vblitter-fe.o

#VLX vlink library
obj-$(CONFIG_VLINK_LIB)  += vlink-lib.o

#VLX VRPQ frontend driver
obj-$(CONFIG_VRPQ_FE)  += vrpq-common.o vrpq-fe.o 

#VLX VRPQ backend driver
obj-$(CONFIG_VRPQ_BE)  += pmem-user.o vrpq-common.o vrpq-be.o

#VLX VUMEM frontend driver
obj-$(CONFIG_VUMEM_FE)  += vumem-arch.o vumem-common.o vumem-fe.o

#VLX VUMEM backend driver
obj-$(CONFIG_VUMEM_BE)  += vumem-arch.o vumem-common.o vumem-be.o

#VLX VOGL frontend driver
obj-$(CONFIG_VOGL_FE)  += vogl-common.o vogl-fe.o 

#VLX VOGL backend driver
obj-$(CONFIG_VOGL_BE)  += vogl-common.o vogl-be.o

#VLX virtual cross-interrupt benchmark driver
obj-$(CONFIG_XIRQ_BENCH)	+= xirq-bench.o

#VLX Monitoring interface
obj-$(CONFIG_VLX_MONITORING) += perfmonitor.o


#VLX Information
obj-$(CONFIG_VINFO)		+= vinfo.o

#VLX user mode virtual driver proxy driver
obj-$(CONFIG_VLX_UMP)		+= vlx-ump.o

#VLX console history driver
obj-$(CONFIG_VLX_HISTORY)	+= vlx-history.o

#uevent dumping driver
obj-$(CONFIG_UEVENT_DUMP)	+= uevent-dump.o

#VLX memory balloon driver
obj-$(CONFIG_VLX_BALLOON)	+= vlx-balloon.o

#VLX memory balloon focus driver
obj-$(CONFIG_VLX_BALLOON_FOCUS)	+= vlx-balloon-focus.o

