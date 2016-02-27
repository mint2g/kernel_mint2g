obj-y += io.o irq.o timer.o devices.o arch_asm.o arch_init.o globalregs.o \
         clock_common.o clock_sc8810.o regulator.o regulator-devices.o \
         dma.o reserve.o pinmap.o adi.o gpio_eic.o adc.o efuse.o dcdc_cal.o \
         watchdog.o

obj-y += sys_debug.o
obj-y += sec-common.o sec-reboot.o

ifeq ($(CONFIG_NKERNEL),y)
obj-$(CONFIG_PM) += pm_vlx.o
else
obj-$(CONFIG_PM) += pm_orig.o
endif
obj-$(CONFIG_PM) += pm.o  pm_sc8810.o standby.o pm_debug.o

obj-$(CONFIG_SPRD_DCDC_DEBUG) += dcdc_debug.o

obj-$(CONFIG_DEBUG_FS) += clock_debug.o

sprdboard-$(CONFIG_MACH_SP8810EA) += board-sp8810ea/
sprdboard-$(CONFIG_MACH_SP8810EC) += board-sp8810ec/
sprdboard-$(CONFIG_MACH_SP8810GA) += board-sp8810ga/
sprdboard-$(CONFIG_MACH_SP8810GB) += board-sp8810gb/
sprdboard-$(CONFIG_MACH_SP6820GA) += board-sp6820ga/
sprdboard-$(CONFIG_MACH_AMAZING)  += board-amazing/
sprdboard-$(CONFIG_MACH_KYLETD)  += board-kyletd/
sprdboard-$(CONFIG_MACH_KYLEW)  += board-kylew/
sprdboard-$(CONFIG_MACH_MINT)  += board-mint/
sprdboard-$(CONFIG_MACH_SP8810EB) += board-sp8810eb/
sprdboard-$(CONFIG_MACH_Z788)  += board-z788/
sprdboard-$(CONFIG_MACH_SP7710G_OPENPHONE)  += board-sp7710g_openphone/
sprdboard-$(CONFIG_MACH_SP7702) += board-sp7702/
sprdboard-$(CONFIG_MACH_OPENPHONE_SC6820) += board-sc6820-openphone/

obj-y += $(sprdboard-y)

obj-$(CONFIG_SC8810_VIBRATOR) += vibrator.o
obj-$(CONFIG_ANDROID_RAM_CONSOLE) += ram_console.o
obj-$(CONFIG_CPU_FREQ_SC8810) += cpufreq-sc8810.o
obj-$(CONFIG_SEC_DEBUG) += sec_debug.o sec_gaf.o sec_getlog.o
obj-$(CONFIG_BCMDHD)  += dhd_adapter.o
obj-$(CONFIG_BCMDHD89)  += dhd_adapter.o
obj-$(CONFIG_SPRD_MEM_POOL)    += sprd_mem_pool.o
ifeq ($(CONFIG_RTL8723AS),m)
obj-y  += rtl8723_adapter.o
endif

obj-y += ss_musb_info.o

sprdboarddirs := $(patsubst %,arch/arm/mach-sc8810/%,$(sprdboard-y))

# all compiler warnings in here are treated as errors from now on
# expect declration after statements, long live c90
# make sure any modifications to defconfig or source files passesi this -psych.half
KBUILD_CPPFLAGS += $(patsubst %,-I%,$(sprdboarddirs)) -Werror -Wno-error=declaration-after-statement

# remove warnings for variables, you got them right spreadtrum -psych.half
#$(warning $(sprdboarddirs))
#$(warning $(KBUILD_CPPFLAGS))

