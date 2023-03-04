# Select the target host.
#MODEL         := MZ2000
#MODEL         := MZ700
#DEBUG         := y
MODEL          := MZ80A
KERNEL         := $(PWD)/../../../linux/kernel
FUSIONX        := $(PWD)/../..
CROSS          := arm-linux-gnueabihf-
ifeq ($(DEBUG),y)
ccflags-y      += -DTTYMZ_DEBUG
else
ccflags-y      += -O2 -I${KERNEL}/drivers/sstar/include -I${KERNEL}/drivers/sstar/include/infinity2m -I${KERNEL}/drivers/sstar/gpio/infinity2m -D__KERNEL_DRIVER__
endif
obj-m	       += ttymzdrv.o
ttymzdrv-objs  += ttymz.o z80io.o sharpmz.o
ttymzdrv-objs  += ../../../linux/kernel/drivers/sstar/gpio/infinity2m/gpio_table.o
ttymzdrv-objs  += ../../../linux/kernel/drivers/sstar/gpio/infinity2m/mhal_gpio.o
ttymzdrv-objs  += ../../../linux/kernel/drivers/sstar/gpio/infinity2m/mhal_pinmux.o
ttymzdrv-objs  += ../../../linux/kernel/drivers/sstar/gpio/infinity2m/padmux_tables.o


all:
	@echo ""
	@echo "Build TTYMZ driver for host: $(MODEL)"
	make -C $(KERNEL) ARCH=arm CROSS_COMPILE=$(CROSS)  M="$(PWD)" modules
	@echo ""

install:
	@echo "Copy kernel driver..."
	@cp ttymz.ko $(FUSIONX)/modules/

clean:
	make -C $(KERNEL) M=$(PWD) clean
