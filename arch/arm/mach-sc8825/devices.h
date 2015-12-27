/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ARCH_ARM_MACH_SPRD_DEVICES_H
#define __ARCH_ARM_MACH_SPRD_DEVICES_H

extern struct platform_device sprd_hwspinlock_device0;
extern struct platform_device sprd_serial_device0;
extern struct platform_device sprd_serial_device1;
extern struct platform_device sprd_serial_device2;
extern struct platform_device sprd_device_rtc;
extern struct platform_device sprd_nand_device;
extern struct platform_device sprd_lcd_device0;
extern struct platform_device sprd_lcd_device1;
extern struct platform_device sprd_otg_device;
extern struct platform_device sprd_backlight_device;
extern struct platform_device sprd_i2c_device0;
extern struct platform_device sprd_i2c_device1;
extern struct platform_device sprd_i2c_device2;
extern struct platform_device sprd_i2c_device3;
extern struct platform_device sprd_spi0_device;
extern struct platform_device sprd_spi1_device;
extern struct platform_device sprd_spi2_device;
extern struct platform_device sprd_keypad_device;
extern struct platform_device sprd_audio_platform_vbc_pcm_device;
extern struct platform_device sprd_audio_cpu_dai_vaudio_device;
extern struct platform_device sprd_audio_cpu_dai_vbc_device;
extern struct platform_device sprd_audio_codec_sprd_codec_device;
extern struct platform_device sprd_battery_device;
extern struct platform_device sprd_vsp_device;
#ifdef CONFIG_ANDROID_PMEM
extern struct platform_device sprd_pmem_device;
extern struct platform_device sprd_pmem_adsp_device;
#endif
#ifdef CONFIG_ION
extern struct platform_device sprd_ion_dev;
#endif
extern struct platform_device sprd_sdio0_device;
extern struct platform_device sprd_sdio1_device;
extern struct platform_device sprd_sdio2_device;
extern struct platform_device sprd_emmc_device;
extern struct platform_device sprd_dcam_device;
extern struct platform_device sprd_scale_device;
extern struct platform_device sprd_rotation_device;
extern struct platform_device sprd_sensor_device;
extern struct platform_device sprd_isp_device;
extern struct platform_device sprd_ahb_bm0_device;
extern struct platform_device sprd_ahb_bm1_device;
extern struct platform_device sprd_ahb_bm2_device;
extern struct platform_device sprd_ahb_bm3_device;
extern struct platform_device sprd_ahb_bm4_device;
extern struct platform_device sprd_axi_bm0_device;
extern struct platform_device sprd_axi_bm1_device;
extern struct platform_device sprd_axi_bm2_device;
extern struct platform_device sprd_spipe_td_device;
extern struct platform_device sprd_slog_td_device;
extern struct platform_device sprd_stty_td_device;
extern struct platform_device sprd_cproc_td_device;
extern struct platform_device sprd_seth_td_device;
extern struct platform_device sprd_peer_state_device;

#endif
