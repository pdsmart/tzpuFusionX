# Select the target host.
#MODEL          := MZ2000
#MODEL          := MZ1500
#MODEL          := MZ700
#MODEL          := MZ80A
#MODEL          := PCW8XXX
#MODEL          := PCW9XXX
KERNEL         := $(PWD)/../../../linux/kernel
FUSIONX        := $(PWD)/../..
CROSS          := arm-linux-gnueabihf-
CTRLINC        = -IZeta/API -IZ80/API -DTARGET_HOST_$(MODEL)=1

ccflags-y      = -O2 -I${KERNEL}/drivers/sstar/include -I${KERNEL}/drivers/sstar/include/infinity2m -I${KERNEL}/drivers/sstar/gpio/infinity2m -D__KERNEL_DRIVER__ -DTARGET_HOST_$(MODEL)=1
ifeq ($(DEBUG),y)
ccflags-y      += -DTTYMZ_DEBUG
endif
obj-m	       += ttymzdrv.o
ttymzdrv-objs  += ttymz.o z80io.o sharpmz.o
ttymzdrv-objs  += ../../../linux/kernel/drivers/sstar/gpio/infinity2m/gpio_table.o
ttymzdrv-objs  += ../../../linux/kernel/drivers/sstar/gpio/infinity2m/mhal_gpio.o
ttymzdrv-objs  += ../../../linux/kernel/drivers/sstar/gpio/infinity2m/mhal_pinmux.o
ttymzdrv-objs  += ../../../linux/kernel/drivers/sstar/gpio/infinity2m/padmux_tables.o

all: 
	@echo "Specify target host, ie. make <host>"
	@echo "Supported hosts: MZ80A, MZ700, MZ2000, PCW8XXX, PCW9XXX"

MZ80A:	MODEL_MZ80A
MZ700:	MODEL_MZ700
MZ1500:	MODEL_MZ1500
MZ2000: MODEL_MZ2000
PCW8XXX: MODEL_PCW8XXX
PCW9XXX: MODEL_PCW9XXX

MODEL_MZ80A:
	$(MAKE) MODEL=MZ80A BUILD_MZ80A
MODEL_MZ700:
	$(MAKE) MODEL=MZ700 BUILD_MZ700
MODEL_MZ1500:
	$(MAKE) MODEL=MZ1500 BUILD_MZ1500
MODEL_MZ2000:
	$(MAKE) MODEL=MZ2000 BUILD_MZ2000
MODEL_PCW8XXX:
	$(MAKE) MODEL=PCW8XXX BUILD_PCW8XXX
MODEL_PCW9XXX:
	$(MAKE) MODEL=PCW8XXX BUILD_PCW9XXX

BUILD_MZ80A:	kmod
BUILD_MZ700:	kmod
BUILD_MZ1500:	kmod
BUILD_MZ2000:	kmod
BUILD_PCW8XXX:	kmod
BUILD_PCW9XXX:	kmod

kmod:
	@echo ""
	@echo "Build TTYMZ driver for host: $(MODEL)"
	make -C $(KERNEL) ARCH=arm CROSS_COMPILE=$(CROSS)  M="$(PWD)" modules

install:
	@echo "Copy kernel driver..."
	@cp ttymzdrv.ko $(FUSIONX)/modules/

clean:
	make -C $(KERNEL) M=$(PWD) clean
	@rm -f ttymz
