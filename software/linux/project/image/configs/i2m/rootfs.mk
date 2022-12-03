IMAGE_INSTALL_DIR:=$(OUTPUTDIR)
-include $(PROJ_ROOT)/../sdk/verify/application/app.mk
-include $(PROJ_ROOT)/release/customer_tailor/$(CUSTOMER_TAILOR)

LIB_DIR_PATH:=$(PROJ_ROOT)/release/$(PRODUCT)/$(CHIP)/common/$(TOOLCHAIN)/$(TOOLCHAIN_VERSION)

.PHONY: rootfs root app app_ba app_jpeg app_disp app_zkfs
rootfs:root app_ba app_jpeg app_disp app_zkfs
root:
	cd rootfs; tar xf rootfs.tar.gz -C $(OUTPUTDIR)
	cp rootfs_add_files/* $(OUTPUTDIR)/rootfs/ -rf
	sed -i 's/\#PermitRootLogin prohibit-password/PermitRootLogin yes/' $(OUTPUTDIR)/rootfs/etc/ssh/sshd_config
	# foe no login
	sed -i 's/console\:\:respawn\:\/sbin\/getty -L  console 0 vt100 \# GENERIC_SERIAL/console::respawn:-\/bin\/sh/' $(OUTPUTDIR)/rootfs/etc/inittab
	#tar xf busybox/$(BUSYBOX).tar.gz -C $(OUTPUTDIR)/rootfs
	tar xf $(LIB_DIR_PATH)/package/$(LIBC).tar.gz -C $(OUTPUTDIR)/rootfs/lib
	mkdir -p $(miservice$(RESOUCE))/lib
	mkdir -p $(miservice$(RESOUCE))/bin
	cp $(LIB_DIR_PATH)/mi_libs/dynamic/* $(miservice$(RESOUCE))/lib/
	cp $(LIB_DIR_PATH)/ex_libs/dynamic/* $(miservice$(RESOUCE))/lib/
	
	mkdir -p $(miservice$(RESOUCE))
	if [ "$(BOARD)" = "010A" ]; then \
		cp -rf $(PROJ_ROOT)/board/ini/* $(miservice$(RESOUCE)) ;\
	else \
		cp -rf $(PROJ_ROOT)/board/ini/LCM $(miservice$(RESOUCE)) ;\
	fi;

	if [ "$(BOARD)" = "010A" ]; then \
		cp -rf $(PROJ_ROOT)/board/$(CHIP)/$(BOARD_NAME)/config/* $(miservice$(RESOUCE)) ; \
	else \
		cp -rf $(PROJ_ROOT)/board/$(CHIP)/$(BOARD_NAME)/config/fbdev.ini  $(miservice$(RESOUCE)) ; \
	fi;

	cp -vf $(PROJ_ROOT)/board/$(CHIP)/mmap/$(MMAP)  $(miservice$(RESOUCE))/mmap.ini
	cp -rvf $(LIB_DIR_PATH)/bin/config_tool/*  $(miservice$(RESOUCE))
	cd  $(miservice$(RESOUCE)); chmod +x config_tool; ln -sf config_tool dump_config; ln -sf config_tool dump_mmap
	if [ "$(BOARD)" = "010A" ]; then \
		cp -rf $(PROJ_ROOT)/board/$(CHIP)/pq  $(miservice$(RESOUCE))/ ; \
		find   $(miservice$(RESOUCE))/pq/ -type f ! -name "Main.bin" -type f ! -name "Main_Ex.bin" -type f ! -name "Bandwidth_RegTable.bin"| xargs rm -rf ; \
	fi;

	if [ $(interface_vdec) = "enable" ]; then \
		cp -rf $(PROJ_ROOT)/board/$(CHIP)/vdec_fw  $(miservice$(RESOUCE))/ ; \
	fi;

	mkdir -p $(OUTPUTDIR)/rootfs/config
	#cp -rf etc/* $(OUTPUTDIR)/rootfs/etc
	if [ "$(appconfigs$(RESOUCE))" != "" ]; then \
		mkdir -p  $(appconfigs$(RESOUCE)); \
		mkdir -p $(OUTPUTDIR)/rootfs/appconfigs;\
	fi;
	
	if [ $(BENCH) = "yes" ]; then \
		cp -rf /home/edie.chen/bench  $(miservice$(RESOUCE)) ; \
		cp  $(miservice$(RESOUCE))/bench/demo.bash  $(miservice$(RESOUCE))/bench.sh ; \
		chmod 755  $(miservice$(RESOUCE))/bench.sh ; \
	fi;

	if [ "$(PHY_TEST)" = "yes" ]; then \
		mkdir  $(miservice$(RESOUCE))/sata_phy ; \
		cp $(LIB_DIR_PATH)/bin/sata_phy/*  $(miservice$(RESOUCE))/sata_phy ; \
	fi;

	mkdir -p $(OUTPUTDIR)/rootfs/lib/modules/
	mkdir -p  $(miservice$(RESOUCE))/modules/$(KERNEL_VERSION)

	touch ${OUTPUTDIR}/rootfs/etc/mdev.conf
	echo mice 0:0 0660 =input/ >> ${OUTPUTDIR}/rootfs/etc/mdev.conf
	echo mouse.* 0:0 0660 =input/ >> ${OUTPUTDIR}/rootfs/etc/mdev.conf
	echo event.* 0:0 0660 =input/ >> ${OUTPUTDIR}/rootfs/etc/mdev.conf
	echo '$$DEVNAME=bus/usb/([0-9]+)/([0-9]+) 0:0 0660 =bus/usb/%1/%2' >> ${OUTPUTDIR}/rootfs/etc/mdev.conf
	echo sd[a-z][0-9]  0:0  660  @/etc/hotplug/udisk_insert >> ${OUTPUTDIR}/rootfs/etc/mdev.conf
	echo sd[a-z]       0:0  660  $/etc/hotplug/udisk_remove >> ${OUTPUTDIR}/rootfs/etc/mdev.conf 	
	echo mmcblk[0-9]p[0-9]  0:0  660  @/etc/hotplug/sdcard_insert >> ${OUTPUTDIR}/rootfs/etc/mdev.conf
	echo mmcblk[0-9]        0:0  660   $/etc/hotplug/sdcard_remove >> ${OUTPUTDIR}/rootfs/etc/mdev.conf

	echo export PATH=\$$PATH:/config >> ${OUTPUTDIR}/rootfs/etc/init.d/rcS
	echo export TERMINFO=/usr/share/terminfo >> ${OUTPUTDIR}/rootfs/etc/init.d/rcS
	echo export LD_LIBRARY_PATH=\$$LD_LIBRARY_PATH:/config/lib:/config/wifi >> ${OUTPUTDIR}/rootfs/etc/init.d/rcS
	sed -i '/^mount.*/d' $(OUTPUTDIR)/rootfs/etc/init.d/rcS
	echo mkdir -p /dev/pts >> ${OUTPUTDIR}/rootfs/etc/init.d/rcS
	echo mount -t sysfs none /sys >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS
	echo mount -t tmpfs mdev /dev >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS
	echo mount -t debugfs none /sys/kernel/debug/ >>  $(OUTPUTDIR)/rootfs/etc/init.d/rcS
	echo mdev -s >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS
	cp -rvf $(PROJ_ROOT)/tools/$(TOOLCHAIN)/$(TOOLCHAIN_VERSION)/fw_printenv/* $(OUTPUTDIR)/rootfs/etc/
	echo "$(ENV_CFG)" > $(OUTPUTDIR)/rootfs/etc/fw_env.config
	if [ "$(ENV_CFG1)" != "" ]; then \
		echo "$(ENV_CFG1)" >> $(OUTPUTDIR)/rootfs/etc/fw_env.config ; \
	fi;
	cd $(OUTPUTDIR)/rootfs/etc/;ln -sf fw_printenv fw_setenv
	echo mkdir -p /var/lock >> ${OUTPUTDIR}/rootfs/etc/init.d/rcS
	if [ "$(FPGA)" = "1" ]; then \
		echo mount -t jffs2 /dev/mtdblock1 /config >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS ; \
	fi;
	echo -e $(foreach block, $(USR_MOUNT_BLOCKS), "mount -t $($(block)$(FSTYPE)) $($(block)$(MOUNTPT)) $($(block)$(MOUNTTG))\n") >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS

	chmod 755 $(LIB_DIR_PATH)/bin/debug/*
	cp -rf $(LIB_DIR_PATH)/bin/debug/*  $(miservice$(RESOUCE))
	mkdir -p $(OUTPUTDIR)/rootfs/customer
	rm -f $(OUTPUTDIR)/miservice/config/setup_system.sh
	touch $(OUTPUTDIR)/miservice/config/setup_system.sh
	chmod 755 $(OUTPUTDIR)/miservice/config/setup_system.sh
	rm -f $(OUTPUTDIR)/miservice/config/setup_sd_system.sh
	touch $(OUTPUTDIR)/miservice/config/setup_sd_system.sh
	chmod 755 $(OUTPUTDIR)/miservice/config/setup_sd_system.sh

	# creat insmod ko scrpit
	if [ -f "$(PROJ_ROOT)/kbuild/$(KERNEL_VERSION)/$(CHIP)/configs/$(PRODUCT)/$(BOARD)/$(TOOLCHAIN)/$(TOOLCHAIN_VERSION)/$(FLASH_TYPE)/modules/kernel_mod_list" ]; then \
		cat $(PROJ_ROOT)/kbuild/$(KERNEL_VERSION)/$(CHIP)/configs/$(PRODUCT)/$(BOARD)/$(TOOLCHAIN)/$(TOOLCHAIN_VERSION)/$(FLASH_TYPE)/modules/kernel_mod_list | \
			sed 's#\(.*\).ko#insmod /config/modules/$(KERNEL_VERSION)/\1.ko#' > $(OUTPUTDIR)/miservice/config/setup_system.sh; \
		cat $(PROJ_ROOT)/kbuild/$(KERNEL_VERSION)/$(CHIP)/configs/$(PRODUCT)/$(BOARD)/$(TOOLCHAIN)/$(TOOLCHAIN_VERSION)/$(FLASH_TYPE)/modules/kernel_mod_list | \
			sed 's#\(.*\).ko\(.*\)#$(PROJ_ROOT)/kbuild/$(KERNEL_VERSION)/$(CHIP)/configs/$(PRODUCT)/$(BOARD)/$(TOOLCHAIN)/$(TOOLCHAIN_VERSION)/$(FLASH_TYPE)/modules/\1.ko#' | \
				xargs -i cp -rvf {}  $(miservice$(RESOUCE))/modules/$(KERNEL_VERSION); \
		echo "# kernel_mod_list" >> $(OUTPUTDIR)/miservice/config/setup_system.sh; \
	fi;

	if [ -f "$(LIB_DIR_PATH)/modules/$(KERNEL_VERSION)/misc_mod_list" ]; then \
		cat $(LIB_DIR_PATH)/modules/$(KERNEL_VERSION)/misc_mod_list | \
			sed 's#\(.*\).ko#insmod /config/modules/$(KERNEL_VERSION)/\1.ko#' >> $(OUTPUTDIR)/miservice/config/setup_system.sh; \
		cat $(LIB_DIR_PATH)/modules/$(KERNEL_VERSION)/misc_mod_list | \
			sed 's#\(.*\).ko\(.*\)#$(LIB_DIR_PATH)/modules/$(KERNEL_VERSION)/\1.ko#' | \
				xargs -i cp -rvf {}  $(miservice$(RESOUCE))/modules/$(KERNEL_VERSION); \
		echo "# misc_mod_list" >> $(OUTPUTDIR)/miservice/config/setup_system.sh; \
	fi;

	if [ -f "$(LIB_DIR_PATH)/modules/$(KERNEL_VERSION)/.mods_depend" ]; then \
		cat $(LIB_DIR_PATH)/modules/$(KERNEL_VERSION)/.mods_depend | \
			sed '2,20s#\(.*\)#insmod /config/modules/$(KERNEL_VERSION)/\1.ko\nif [ $$? -eq 0 ]; then\n	busybox mknod /dev/\1 c $$major $$minor\n	let minor++\nfi\n\n#' >> $(OUTPUTDIR)/miservice/config/setup_system.sh; \
		cat $(LIB_DIR_PATH)/modules/$(KERNEL_VERSION)/.mods_depend | \
			sed 's#\(.*\)#$(LIB_DIR_PATH)/modules/$(KERNEL_VERSION)/\1.ko#' | \
				xargs -i cp -rvf {}  $(miservice$(RESOUCE))/modules/$(KERNEL_VERSION); \
		echo "# mi module" >> $(OUTPUTDIR)/miservice/config/setup_system.sh; \
	fi;

	if [ "$(PROJECT)" = "2D07" ]; then \
		cp ../../kernel/drivers/sstar/gpio_key_sample/gpio_led_heartbeat/gpio_led_heartbeat.ko $(miservice$(RESOUCE))/modules/$(KERNEL_VERSION);\
		echo "insmod /config/modules/4.9.84/gpio_led_heartbeat.ko" >> $(OUTPUTDIR)/miservice/config/setup_system.sh; \
	elif [ "$(PROJECT)" = "2D06" ]; then \
		cp ../../kernel/drivers/sstar/gpio_key_sample/gpio_led_heartbeat_2D06/gpio_led_heartbeat.ko $(miservice$(RESOUCE))/modules/$(KERNEL_VERSION);\
		echo "#insmod /config/modules/4.9.84/gpio_led_heartbeat.ko" >> $(OUTPUTDIR)/miservice/config/setup_system.sh; \
	fi;

	if [ -f "$(LIB_DIR_PATH)/modules/$(KERNEL_VERSION)/misc_mod_list_late" ]; then \
		cat $(LIB_DIR_PATH)/modules/$(KERNEL_VERSION)/misc_mod_list_late | sed 's#\(.*\).ko#insmod /config/modules/$(KERNEL_VERSION)/\1.ko#' >> $(OUTPUTDIR)/miservice/config/setup_system.sh; \
		cat $(LIB_DIR_PATH)/modules/$(KERNEL_VERSION)/misc_mod_list_late | sed 's#\(.*\).ko\(.*\)#$(LIB_DIR_PATH)/modules/$(KERNEL_VERSION)/\1.ko#' | \
			xargs -i cp -rvf {}  $(miservice$(RESOUCE))/modules/$(KERNEL_VERSION); \
		echo "# misc_mod_list_late" >> $(OUTPUTDIR)/miservice/config/setup_system.sh; \
	fi;

	sed -i 's/mi_common/insmod \/config\/modules\/$(KERNEL_VERSION)\/mi_common.ko\nmajor=\`cat \/proc\/devices \| busybox awk "\\\\$$2==\\""mi"\\" {print \\\\$$1}"\\n`\nminor=0/g' $(OUTPUTDIR)/miservice/config/setup_system.sh; \
	sed -i '/# mi module/a	major=`cat /proc/devices | busybox awk "\\\\$$2==\\""mi_poll"\\" {print \\\\$$1}"`\nbusybox mknod \/dev\/mi_poll c $$major 0' $(OUTPUTDIR)/miservice/config/setup_system.sh; \
	if [ $(PHY_TEST) = "yes" ]; then \
		echo -e "\033[41;33;5m !!! Replace "mdrv-sata-host.ko" with "sata_bench_test.ko" !!!\033[0m" ; \
		sed '/mdrv-sata-host/d' $(OUTPUTDIR)/miservice/config/setup_system.sh >  $(miservice$(RESOUCE))/demotemp.sh ; \
		echo insmod /config/sata_phy/sata_bench_test.ko >>  $(miservice$(RESOUCE))/demotemp.sh ; \
		cp  $(miservice$(RESOUCE))/demotemp.sh $(OUTPUTDIR)/miservice/config/setup_system.sh ; \
		rm  $(miservice$(RESOUCE))/demotemp.sh ; \
	fi;

	if [ $(interface_wlan) = "enable" ]; then \
		mkdir -p  $(miservice$(RESOUCE))/wifi ; \
		if [ $(FLASH_TYPE) = "spinand" ]; then \
			cp -rf $(LIB_DIR_PATH)/wifi/libs/ap/*   $(miservice$(RESOUCE))/wifi ; \
			cp -rf $(LIB_DIR_PATH)/wifi/bin/ap/*   $(miservice$(RESOUCE))/wifi ; \
		fi;	\
		find $(LIB_DIR_PATH)/wifi/bin/ -maxdepth 1 -type f -exec cp -P {}  $(miservice$(RESOUCE))/wifi \; ;\
		find $(LIB_DIR_PATH)/wifi/bin/ -maxdepth 1 -type l -exec cp -P {}  $(miservice$(RESOUCE))/wifi \; ;\
		find $(LIB_DIR_PATH)/wifi/libs/ -maxdepth 1 -type f -exec cp -P {}  $(miservice$(RESOUCE))/wifi \; ;\
		find $(LIB_DIR_PATH)/wifi/libs/ -maxdepth 1 -type l -exec cp -P {}  $(miservice$(RESOUCE))/wifi \; ;\
		cp -rf $(LIB_DIR_PATH)/wifi/modules/*   $(miservice$(RESOUCE))/wifi ; \
		cp -rf $(LIB_DIR_PATH)/wifi/configs/*   $(miservice$(RESOUCE))/wifi ; \
	fi;
	if [ "$(appconfigs$(RESOUCE))" != "" ]; then \
		if [ -f "$(miservice$(RESOUCE))/wifi/wpa_supplicant.conf" ]; then	\
			mv  $(miservice$(RESOUCE))/wifi/wpa_supplicant.conf $(appconfigs$(RESOUCE));	\
			cp $(OUTPUTDIR)/appconfigs/wpa_supplicant.conf $(appconfigs$(RESOUCE))/wpa_supplicant.conf_bak;	\
		fi;	\
		cp -rf $(PROJ_ROOT)/board/$(CHIP)/$(BOARD_NAME)/config/model/LCM.ini $(appconfigs$(RESOUCE));	\
	fi;
	# Enable MIU protect on CMDQ buffer as default (While List: CPU)
	# [I5] The 1st 1MB of MIU is not for CMDQ buffer
#	echo 'echo set_miu_block3_status 0 0x70 0 0x100000 1 > /proc/mi_modules/mi_sys_mma/miu_protect'  >>  $(miservice$(RESOUCE))/demo.sh

#	echo mount -t jffs2 /dev/mtdblock3 /config                                                       >> $(OUTPUTDIR)/rootfs/etc/profile
	ln -fs /config/modules/$(KERNEL_VERSION) $(OUTPUTDIR)/rootfs/lib/modules/
	find  $(miservice$(RESOUCE))/modules/$(KERNEL_VERSION) -name "*.ko" | xargs $(TOOLCHAIN_REL)strip  --strip-unneeded;
	#find $(OUTPUTDIR)/rootfs/lib/ -name "*.so*" | xargs $(TOOLCHAIN_REL)strip  --strip-unneeded;
	find $(OUTPUTDIR)/rootfs/lib/ -name "*.so*" -a -name "*[!p][!y]" | xargs $(TOOLCHAIN_REL)strip  --strip-unneeded;
	echo mkdir -p /dev/pts                                                                           >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS
	echo mount -t devpts devpts /dev/pts                                                             >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS
	echo "busybox telnetd&"                                                                          >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS

	if [ "$(FLASH_WP_RANGE)" != "" ]; then \
		echo "if [ -e  /sys/class/mstar/msys/protect ]; then"                                        >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS ; \
		echo "    echo $(FLASH_WP_RANGE) > /sys/class/mstar/msys/protect"                            >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS ; \
		echo "fi;"                                                                                   >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS ; \
	fi;

	if [ "$(PROJECT)" =  "2D07" ]; then \
		echo "echo 85 > /sys/class/gpio/export"                                                      >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
		echo "echo out > /sys/class/gpio/gpio85/direction"                                           >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
		echo "echo 1 > /sys/class/gpio/gpio85/value"                                                 >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
		echo "echo 86 > /sys/class/gpio/export"                                                      >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
		echo "echo out > /sys/class/gpio/gpio86/direction"                                           >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
		echo "echo 1 > /sys/class/gpio/gpio86/value"                                                 >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
		echo "echo 90 > /sys/class/gpio/export"                                                      >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
		echo "echo out > /sys/class/gpio/gpio90/direction"                                           >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
		echo "echo 1 > /sys/class/gpio/gpio90/value"                                                 >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
		echo "echo 47 > /sys/class/gpio/export"                                                      >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
		echo "echo out > /sys/class/gpio/gpio47/direction"                                           >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
		echo "echo 0 > /sys/class/gpio/gpio47/value"                                                 >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
	fi;

	echo ". /config/profile.sh"                                                                      >> $(OUTPUTDIR)/rootfs/etc/profile;\
	echo 'export LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:/config/lib:/config/wifi'                         >> ${OUTPUTDIR}/miservice/config/profile.sh;\
	echo 'export PATH=$$PATH:/config:/config/wifi:/apps/FusionX/bin'                                 >> ${OUTPUTDIR}/miservice/config/profile.sh;\
	echo "export TERMINFO=/usr/share/terminfo"                                                       >> $(OUTPUTDIR)/miservice/config/profile.sh;\
	echo "export TERM=screen"                                                                        >> $(OUTPUTDIR)/miservice/config/profile.sh;\

	echo "if [ -e /etc/core.sh ]; then"                                                              >> ${OUTPUTDIR}/rootfs/etc/init.d/rcS; \
	echo '    echo "|/etc/core.sh %p" > /proc/sys/kernel/core_pattern'                               >> ${OUTPUTDIR}/rootfs/etc/init.d/rcS; \
	echo "chmod 777 /etc/core.sh"                                                                    >> ${OUTPUTDIR}/rootfs/etc/init.d/rcS; \
	echo "fi;"                                                                                       >> ${OUTPUTDIR}/rootfs/etc/init.d/rcS; \
	
	echo "if [ -e /config/setup_system.sh ]; then"                                                   >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
	echo "    /config/setup_system.sh"                                                               >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
	echo "fi;"                                                                                       >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
	echo "if [ -e /config/setup_sd_system.sh ]; then"                                                >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
	echo "    /config/setup_sd_system.sh"                                                            >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
	echo "fi;"                                                                                       >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
	echo "chmod 777 /dev/null;"                                                                      >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
	echo mdev -s                                                                                     >> $(OUTPUTDIR)/miservice/config/setup_system.sh; \
	if [ $(BENCH) = "yes" ]; then \
		echo ./config/bench.sh                                                                       >> $(OUTPUTDIR)/miservice/config/setup_system.sh ; \
	fi;
	if [ "$(BOARD)" = "011A" ]; then \
		sed -i 's/mi_sys.ko/mi_sys.ko cmdQBufSize=128 logBufSize=0/g' $(OUTPUTDIR)/miservice/config/setup_system.sh ;\
	fi; \
	if [ $(TOOLCHAIN) = "glibc" ]; then \
		cp -rvf $(PROJ_ROOT)/tools/$(TOOLCHAIN)/$(TOOLCHAIN_VERSION)/htop/terminfo $(OUTPUTDIR)/miservice/config/;	\
		cp -rvf $(PROJ_ROOT)/tools/$(TOOLCHAIN)/$(TOOLCHAIN_VERSION)/htop/htop $(OUTPUTDIR)/miservice/config/;	\
		echo export TERMINFO=/usr/share/terminfo                                                     >> $(OUTPUTDIR)/miservice/config/setup_system.sh;	\
		echo export TERM=screen                                                                      >> $(OUTPUTDIR)/miservice/config/setup_system.sh;	\
	fi; \

	# Setup the root and fusionx users.
	echo 'fusionx:x:2000:2000:FusionX Admin:/apps/FusionX:/bin/sh'                                   >> ${OUTPUTDIR}/rootfs/etc/passwd; \
	echo 'fusionx:x:2000:'                                                                           >> ${OUTPUTDIR}/rootfs/etc/group; \
	#grep -v "^root" ${OUTPUTDIR}/rootfs/etc/shadow                                                  >> ${OUTPUTDIR}/rootfs/etc/shadow.bak; \
	#echo 'root:$$1$$.qK9kwhb$$yut3JmVlN4MtXH.c6Z3BB1:1::::::'                                        >  ${OUTPUTDIR}/rootfs/etc/shadow; \
	echo 'fusionx:$$1$$I42MsnIW$$EzdD/xodQmYkVj6p9j.9o.:1:0:99999:7:::'                              >> ${OUTPUTDIR}/rootfs/etc/shadow; \
	#cat ${OUTPUTDIR}/rootfs/etc/shadow.bak                                                          >> ${OUTPUTDIR}/rootfs/etc/shadow; \
	#cat ${OUTPUTDIR}/rootfs/etc/shadow                                                               >  ${OUTPUTDIR}/rootfs/etc/shadow.bak; \

	# Setup rcS to set the root password on initial setup.
	echo "if [ -e /etc/init.d/rcS.setup ]; then"                                                     >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
	echo "    . /etc/init.d/rcS.setup"                                                               >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
	echo "    rm -f /etc/init.d/rcS.setup"                                                           >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
	echo "fi"                                                                                        >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
	echo "passwd <<-eof"                                                                             >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS.setup; \
	echo "fusionx"                                                                                   >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS.setup; \
	echo "fusionx"                                                                                   >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS.setup; \
	echo "eof"                                                                                       >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS.setup; \
	echo "passwd fusionx <<-eof"                                                                     >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS.setup; \
	echo "fusionx"                                                                                   >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS.setup; \
	echo "fusionx"                                                                                   >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS.setup; \
	echo "eof"                                                                                       >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS.setup; \
	echo "chmod 755 /etc/init.d/rcS.setup"                                                           >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \

	echo "ctrl_interface=/tmp/wifi/run/wpa_supplicant"                                               >> ${OUTPUTDIR}/miservice/config/wpa_supplicant.conf; \
	echo "update_config=1"                                                                           >> ${OUTPUTDIR}/miservice/config/wpa_supplicant.conf; \
	echo "network={"                                                                                 >> ${OUTPUTDIR}/miservice/config/wpa_supplicant.conf; \
	echo "        ssid=\"RICHMONDAIR\""                                                              >> ${OUTPUTDIR}/miservice/config/wpa_supplicant.conf; \
	#echo "        ssid=\"SSID\""                                                                    >> ${OUTPUTDIR}/miservice/config/wpa_supplicant.conf; \
	echo "        scan_ssid=1"                                                                       >> ${OUTPUTDIR}/miservice/config/wpa_supplicant.conf; \
	echo "        psk=\"8058680686123iJ\""                                                           >> ${OUTPUTDIR}/miservice/config/wpa_supplicant.conf; \
	#echo "        psk=\"PASSWORD FOR SSID\""                                                        >> ${OUTPUTDIR}/miservice/config/wpa_supplicant.conf; \
	echo "}"                                                                                         >> ${OUTPUTDIR}/miservice/config/wpa_supplicant.conf; \
	echo ""                                                                                          >> ${OUTPUTDIR}/miservice/config/wpa_supplicant.conf; \
	echo ""                                                                                          >> ${OUTPUTDIR}/miservice/config/wpa_supplicant.conf; \

	echo "# Copy required config files."                                                             >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS.setup; \
	echo "if [ -e /config/wpa_supplicant.conf ]; then"                                               >> ${OUTPUTDIR}/rootfs/etc/init.d/rcS.setup; \
	echo "  cp /config/wpa_supplicant.conf /etc/wpa_supplicant.conf"                                 >> ${OUTPUTDIR}/rootfs/etc/init.d/rcS.setup; \
	echo "fi"                                                                                        >> ${OUTPUTDIR}/rootfs/etc/init.d/rcS.setup; \

	echo "# Rename the upgrade image to prevent another upgrade."                                    >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS.setup; \
	echo "mkdir -p /upgrade"                                                                         >> ${OUTPUTDIR}/rootfs/etc/init.d/rcS.setup; \
	echo "mount /dev/mmcblk1p1 /upgrade"                                                             >> ${OUTPUTDIR}/rootfs/etc/init.d/rcS.setup; \
	echo "mv /upgrade/fusionx.bin /upgrade/fusionx.lst"                                              >> ${OUTPUTDIR}/rootfs/etc/init.d/rcS.setup; \
	echo "umount /upgrade"                                                                           >> ${OUTPUTDIR}/rootfs/etc/init.d/rcS.setup; \
	echo "#/dev/mmcblk1p1  /upgrade        vfat    defaults        0       0"                        >> ${OUTPUTDIR}/rootfs/etc/fstab; \

	# Start wifi client mode if config file available.
	echo "if [ -e /etc/wpa_supplicant.conf ]; then"                                                  >> ${OUTPUTDIR}/rootfs/etc/init.d/rcS; \
	echo "  /config/wifi/ssw01bInit.sh"                                                              >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
	echo "  nohup /config/wifi/wpa_supplicant -c /etc/wpa_supplicant.conf -i wlan0 &"                >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
	echo "fi"                                                                                        >> ${OUTPUTDIR}/rootfs/etc/init.d/rcS; \

	# Setup ZRAM.
	echo "/dev/zram0      none            swap    sw              0      -1"                         >> ${OUTPUTDIR}/rootfs/etc/fstab; \
	echo "# Setup ZRAM as SWAP, highest priority."                                                   >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
	echo "echo 8388608 > /sys/block/zram0/disksize"                                                  >> ${OUTPUTDIR}/rootfs/etc/init.d/rcS; \
	echo "mkswap /dev/zram0"                                                                         >> ${OUTPUTDIR}/rootfs/etc/init.d/rcS; \
	echo "swapon /dev/zram0"                                                                         >> ${OUTPUTDIR}/rootfs/etc/init.d/rcS; \

	# Setup the SD Card so that it initialises required devices and files on startup.
	echo "/dev/mmcblk1p2  none            swap    sw              0      -2"                         >> ${OUTPUTDIR}/rootfs/etc/fstab; \
	echo "# Add SD Swap Partition."                                                                  >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
	echo "swapon /dev/mmcblk1p2"                                                                     >> ${OUTPUTDIR}/rootfs/etc/init.d/rcS; \
	echo "mount -a"                                                                                  >> ${OUTPUTDIR}/rootfs/etc/init.d/rcS; \

	echo "# Invoke the WebServer Application."                                                       >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
	echo "if [ -e /apps/start_WebServer.sh ]; then"                                                  >> ${OUTPUTDIR}/rootfs/etc/init.d/rcS; \
	echo "  nohup /apps/start_WebServer.sh &"                                                        >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
	echo "fi"                                                                                        >> ${OUTPUTDIR}/rootfs/etc/init.d/rcS; \

	echo "# Invoke the FusionX Application."                                                         >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
	echo "if [ -e /apps/start_FusionX.sh ]; then"                                                    >> ${OUTPUTDIR}/rootfs/etc/init.d/rcS; \
	echo "  nohup /apps/start_FusionX.sh &"                                                          >> $(OUTPUTDIR)/rootfs/etc/init.d/rcS; \
	echo "fi"                                                                                        >> ${OUTPUTDIR}/rootfs/etc/init.d/rcS; \

	if [ -d $(PROJ_ROOT)/FusionX ]; then \
		mkdir -p ${OUTPUTDIR}/FusionX/; \
		cp -pr $(PROJ_ROOT)/FusionX/ ${OUTPUTDIR}/FusionX/; \
	fi;\
	
	mkdir -p $(OUTPUTDIR)/vendor; \
	mkdir -p $(OUTPUTDIR)/customer; \
	mkdir -p $(OUTPUTDIR)/rootfs/vendor; \
	mkdir -p $(OUTPUTDIR)/rootfs/customer;
