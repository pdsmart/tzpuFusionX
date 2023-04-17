#MODEL       := MZ2000
#MODEL       := MZ700
#MODEL       := MZ80A
#MODEL       := PCW8XXX
#MODEL       := PCW9XXX
KERNEL      := $(PWD)/../../../linux/kernel
FUSIONX     := $(PWD)/../..
CROSS       := arm-linux-gnueabihf-
ccflags-y   = -O2 -I${src}/Zeta/API -I${src}/Z80/API -I${KERNEL}/drivers/sstar/include -I${KERNEL}/drivers/sstar/include/infinity2m -I${KERNEL}/drivers/sstar/gpio/infinity2m -D__KERNEL_DRIVER__ -DTARGET_HOST_$(MODEL)=1
CTRLINC     = -IZeta/API -IZ80/API -DTARGET_HOST_$(MODEL)=1

obj-m       += z80drv.o
z80drv-objs += src/z80driver.o Z80.o src/z80io.o src/z80menu.o # emumz.o sharpmz.o osd.o
z80drv-objs += ../../../linux/kernel/drivers/sstar/gpio/infinity2m/gpio_table.o
z80drv-objs += ../../../linux/kernel/drivers/sstar/gpio/infinity2m/mhal_gpio.o
z80drv-objs += ../../../linux/kernel/drivers/sstar/gpio/infinity2m/mhal_pinmux.o
z80drv-objs += ../../../linux/kernel/drivers/sstar/gpio/infinity2m/padmux_tables.o


all: 
	@echo "Specify target host, ie. make <host>"
	@echo "Supported hosts: MZ80A, MZ700, MZ2000, PCW8XXX, PCW9XXX"

MZ80A:	MODEL_MZ80A
MZ700:	MODEL_MZ700
MZ2000: MODEL_MZ2000
PCW8XXX: MODEL_PCW8XXX
PCW9XXX: MODEL_PCW9XXX

MODEL_MZ80A:
	$(MAKE) MODEL=MZ80A BUILD_MZ80A
MODEL_MZ700:
	$(MAKE) MODEL=MZ700 BUILD_MZ700
MODEL_MZ2000:
	$(MAKE) MODEL=MZ2000 BUILD_MZ2000
MODEL_PCW8XXX:
	$(MAKE) MODEL=PCW8XXX BUILD_PCW8XXX
MODEL_PCW9XXX:
	$(MAKE) MODEL=PCW8XXX BUILD_PCW9XXX

BUILD_MZ80A:	sharpbiter k64fcpu kmod z80ctrl
BUILD_MZ700:	sharpbiter k64fcpu kmod z80ctrl
BUILD_MZ2000:	sharpbiter k64fcpu kmod z80ctrl
BUILD_PCW8XXX:	kmod z80ctrl
BUILD_PCW9XXX:	kmod z80ctrl


sharpbiter:
	@echo ""
	@echo "Build Sharp MZ Arbiter for host: $(MODEL)"
	$(CROSS)gcc $(CTRLINC) src/sharpbiter.c -o sharpbiter

k64fcpu:
	@echo ""
	@echo "Build K64F Daemon for host: $(MODEL)"
	$(CROSS)gcc $(CTRLINC) src/k64fcpu.c -o k64fcpu

kmod:
	@echo ""
	@echo "Build driver for host: $(MODEL)"
	make -C $(KERNEL) ARCH=arm CROSS_COMPILE=$(CROSS)  M="$(PWD)" modules

z80ctrl:
	@echo ""
	@echo "Build z80ctrl tool for host: $(MODEL)"
	$(CROSS)gcc $(CTRLINC) src/z80ctrl.c -o z80ctrl

install:
	@echo "Copy kernel driver..."
	@cp z80drv.ko $(FUSIONX)/modules/
	@echo "Copy z80ctrl app..."
	@cp z80ctrl $(FUSIONX)/bin/
	@if [ -f sharpbiter ]; then\
		echo "Copy sharpbiter app...";\
		cp sharpbiter $(FUSIONX)/bin/;\
	fi
	@if [ -f k64fcpu ]; then\
		echo "Copy k64fcpu app...";\
		cp k64fcpu $(FUSIONX)/bin/;\
	fi

clean:
	make -C $(KERNEL) M=$(PWD) clean
	@rm -f sharpbiter k64fcpu z80ctrl
	
