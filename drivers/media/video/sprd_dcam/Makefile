# SC88XX Platform Support

ifeq ($(CONFIG_ARCH_SC8810),y)
sprd_dcam-objs := common/sensor_drv.o common/sensor_cfg.o common/dc_cfg.o common/dc_product_cfg.o common/isp_control.o sc8810/dcam_service_sc8810.o sc8810/dcam_drv_sc8810.o sc8810/dcam_v4l2.o
# sensor
obj-$(CONFIG_DCAM_SENSOR_OV2640) += sensor_ov2640.o
obj-$(CONFIG_DCAM_SENSOR_OV2659) += sensor_ov2659.o
obj-$(CONFIG_DCAM_SENSOR_OV3640) += sensor_ov3640.o
obj-$(CONFIG_DCAM_SENSOR_OV5640) += sensor_ov5640.o
obj-$(CONFIG_DCAM_SENSOR_OV2655) += sensor_ov2655.o
obj-$(CONFIG_DCAM_SENSOR_OV7660) += sensor_ov7660.o
obj-$(CONFIG_DCAM_SENSOR_OV7670) += sensor_ov7670.o
obj-$(CONFIG_DCAM_SENSOR_OV7675) += sensor_ov7675.o
obj-$(CONFIG_DCAM_SENSOR_OV7690) += sensor_ov7690.o
obj-$(CONFIG_DCAM_SENSOR_OV9655) += sensor_ov9655.o
obj-$(CONFIG_DCAM_SENSOR_OV9660) += sensor_ov9660.o
obj-$(CONFIG_DCAM_SENSOR_GC0306) += sensor_gc0306.o
obj-$(CONFIG_DCAM_SENSOR_GC0307) += sensor_gc0307.o
obj-$(CONFIG_DCAM_SENSOR_GC0309) += sensor_gc0309.o
obj-$(CONFIG_DCAM_SENSOR_GT2005) += sensor_gt2005.o
obj-$(CONFIG_DCAM_SENSOR_HI253) += sensor_hi253.o
obj-$(CONFIG_DCAM_SENSOR_HI351) += sensor_hi351.o
obj-$(CONFIG_DCAM_SENSOR_BG0323) += sensor_bg0323.o
obj-$(CONFIG_DCAM_SENSOR_S5K5CAGX) += sensor_s5k5cagx.o
obj-$(CONFIG_DCAM_SENSOR_S5KA3DFX) += sensor_s5ka3dfx.o
obj-$(CONFIG_DCAM_SENSOR_SR200PC10) += sensor_sr200pc10.o
obj-$(CONFIG_DCAM_SENSOR_S5KCCGX) += sensor_s5k5ccgx.o
# ATV
obj-$(CONFIG_NMI601_ATV) += sensor_nmi601.o
endif

ifeq ($(CONFIG_ARCH_SC8825),y)
sprd_dcam-objs := sc8825/dcam_drv_sc8825.o sc8825/gen_scale_coef.o sc8825/sin_cos.o sc8825/dcam_v4l2.o \
sc8825/csi2/csi_access.o sc8825/csi2/csi_api.o sc8825/csi2/csi_driver.o sc8825/csi2/csi_log.o sc8825/csi2/csi_system.o
endif

obj-y += sprd_dcam.o
