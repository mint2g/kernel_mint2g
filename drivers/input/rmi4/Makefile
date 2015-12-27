obj-$(CONFIG_RMI4_BUS) += rmi_bus.o
obj-$(CONFIG_RMI4_I2C) += rmi_i2c.o
obj-$(CONFIG_RMI4_SPI) += rmi_spi.o
obj-$(CONFIG_RMI4_GENERIC) += rmi_driver.o rmi_f01_dev_info.o
obj-$(CONFIG_RMI4_F09) += rmi_f09.o
obj-$(CONFIG_RMI4_F1A) += rmi_f1a_key.o
obj-$(CONFIG_RMI4_F11) += rmi_f11_touch.o
obj-$(CONFIG_RMI4_F17) += rmi_f17.o
obj-$(CONFIG_RMI4_F19) += rmi_f19.o
obj-$(CONFIG_RMI4_F21) += rmi_f21.o
obj-$(CONFIG_RMI4_F34) += rmi_f34_firmware.o
obj-$(CONFIG_RMI4_F54) += rmi_f54_check.o
obj-$(CONFIG_RMI4_DEV) += rmi_dev.o synaptics_fw_updater.o

ifeq ($(KERNELRELEASE),)

# KERNELDIR ?= /home/<AndroidKernelDirectory>
PWD := $(shell pwd)

.PHONY: build clean

build:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c
else

$(info Building with KERNELRELEASE = ${KERNELRELEASE})
obj-m +=    rmi_dev.o

endif

