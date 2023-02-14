#MODEL       := MZ2000
#MODEL       := MZ700
MODEL       := MZ80A
KERNEL      := $(PWD)/../../../linux/kernel
FUSIONX     := $(PWD)/../..
CROSS       := arm-linux-gnueabihf-
ccflags-y   += -O2 -I${src}/Zeta/API -I${src}/Z80/API -I${KERNEL}/drivers/sstar/include -I${KERNEL}/drivers/sstar/include/infinity2m -I${KERNEL}/drivers/sstar/gpio/infinity2m
CTRLINC     += -IZeta/API -IZ80/API

obj-m       += z80drv.o
z80drv-objs += $(MODEL)/z80driver.o Z80.o $(MODEL)/z80io.o $(MODEL)/z80menu.o # emumz.o sharpmz.o osd.o
z80drv-objs += ../../../linux/kernel/drivers/sstar/gpio/infinity2m/gpio_table.o
z80drv-objs += ../../../linux/kernel/drivers/sstar/gpio/infinity2m/mhal_gpio.o
z80drv-objs += ../../../linux/kernel/drivers/sstar/gpio/infinity2m/mhal_pinmux.o
z80drv-objs += ../../../linux/kernel/drivers/sstar/gpio/infinity2m/padmux_tables.o


all:
	@echo "Build driver for host: $(MODEL)"
	@echo ""
	make -C $(KERNEL) ARCH=arm CROSS_COMPILE=$(CROSS)  M="$(PWD)" modules
	$(CROSS)gcc $(CTRLINC) $(MODEL)/z80ctrl.c -o z80ctrl

install:
	@echo "Copy kernel driver..."
	@cp z80drv.ko $(FUSIONX)/modules/
	@echo "Copy z80ctrl app..."
	@cp z80ctrl $(FUSIONX)/bin/

clean:
	make -C $(KERNEL) M=$(PWD) clean
	
