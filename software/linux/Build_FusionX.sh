#!/bin/bash
#########################################################################################################
##
## Name:            Build_FusionX.sh
## Created:         2019
## Author(s):       SigmaStar, tailored for FusionX
## Description:     SigmaStar linux and u-boot build script.
##                  This script builds the complete OS and software set required for FusionX.
##                  It builds the Linux kernel, rootfs filesystem, U-boot loader and apps.
##
## Credits:         
## Copyright:       (c) SigmaStar, tailoring by Philip Smart for FusionX
##
## History:         October 2022   - Initial tailoring for FusionX
##
## Synopsis:        Build_FusionX.sh -f nand -p ssd202 -o 2D06 -m 256
##
#########################################################################################################


clean=0
while getopts "f:p:q:o:ct" opt; do
  case $opt in
    f)
      flashtype=$OPTARG
      ;;
    p)
      chip=$OPTARG
      ;;
    q)
      fastboot=$OPTARG
      ;;
    o)
      project=$OPTARG
      ;;
    c)
      clean=1
      ;;
    t)
      TESTMODE=YES
      ;;
    \?)
      echo "Invalid option: -$OPTARG" >&2
      ;;
  esac
done

DATE=$(date +%m%d)
BACKUPDIR=$(date +%m%d%H%M)
#images will output in ${RELEASEDIR}/images/
RELEASEDIR=`pwd`
FUSIONXDIR=`pwd`\..\FusionX
WEBSRVDIR=`pwd`\..\WebServer
Z80EMUDIR=`pwd`\..\Z80emu
#release source code
#find ./boot/ | grep -v boot/.git | cpio -pdm ${RELEASEDIR}/
#find ./project/ | grep -v project/.git | cpio -pdm ${RELEASEDIR}/
#find ./kernel/ | grep -v kernel/.git | cpio -pdm ${RELEASEDIR}/
#find ./sdk/ | grep sdk/verify | grep -v sdk/verify/application/smarttalk | cpio -pdm ${RELEASEDIR}/

#save code version
#repo manifest -o snapshot.xml -r
#cp snapshot.xml ${RELEASEDIR}/sdk_version.xml

export ARCH=arm

echo chose ${flashtype}

if [ ! "${project}" = "2D06" -a ! "${project}" = "2D07" ]; then
	project=2D07
	#exit 1
fi	

# Make the output directories if not present.
mkdir -p ${RELEASEDIR}/project/image/output/
for f in miservice appconfigs customer vendor rootfs images
do
    mkdir -p ${RELEASEDIR}/project/image/output/$f
done 

export PROJECT=${project}

if [ "${project}" = "2D06" ]; then
	#KERNEL_DEFCONFIG=infinity2m_spinand_ssc011a_s01a_minigui_doublenet_defconfig
	KERNEL_DEFCONFIG=infinity2m_spinand_fusionx_defconfig
elif [ "${project}" = "2D07" ]; then
	KERNEL_DEFCONFIG=infinity2m_spinand_ssc011a_s01a_minigui_defconfig
fi

# Test mode.
if [ "${TESTMODE}" = "" ]; then

# build uboot
cd ${RELEASEDIR}/boot
declare -x ARCH="arm"
declare -x CROSS_COMPILE="arm-linux-gnueabihf-"
if [ "${flashtype}" = "nor" ]; then
	make infinity2m_defconfig
else
	#make infinity2m_spinand_defconfig
	make fusionx_defconfig
fi
if [ "${clean}" = "1" ]; then
	make clean
fi
make -j8

if [ "${flashtype}" = "nor" ]; then
	if [ -d ../project/board/i2m/boot/nor/uboot ]; then
		cp u-boot.xz.img.bin ../project/board/i2m/boot/nor/uboot
	fi
else
	if [ -d ../project/board/i2m/boot/spinand/uboot ]; then
		cp u-boot_spinand.xz.img.bin ../project/board/i2m/boot/spinand/uboot
	fi
fi

#build kernel
cd ${RELEASEDIR}/kernel
declare -x ARCH="arm"
declare -x CROSS_COMPILE="arm-linux-gnueabihf-"
if [ "${flashtype}" = "nor" ]; then
	if [ "${fastboot}" = "fastboot" ]; then
		echo "Make Kernel 1"
		make infinity2m_ssc011a_s01a_fastboot_defconfig
	else
		echo "Make Kernel 2"
		make infinity2m_ssc011a_s01a_minigui_defconfig
	fi
	
else
	if [ "${fastboot}" = "fastboot" ]; then
		echo "Make Kernel 3"
		make infinity2m_spinand_ssc011a_s01a_minigui_fastboot_defconfig
	else
		echo "Make Kernel 4 - ${KERNEL_DEFCONFIG}"
		#make infinity2m_spinand_ssc011a_s01a_minigui_defconfig
		make ${KERNEL_DEFCONFIG}
	fi
	
fi
if [ "${clean}" = "1" ]; then
	make clean
fi
make -j8

#build project
cd ${RELEASEDIR}/project
if [ "${flashtype}" = "nor" ]; then
	if [ "${fastboot}" = "fastboot" ]; then
		echo test fastboot
		./setup_config.sh ./configs/nvr/i2m/8.2.1/nor.glibc-ramfs.011a.64
	else
		if [ "${chip}" = "ssd201" ]; then
			./setup_config.sh ./configs/nvr/i2m/8.2.1/nor.glibc-squashfs.011a.64
		fi
		if [ "${chip}" = "ssd202" ]; then
			./setup_config.sh ./configs/nvr/i2m/8.2.1/nor.glibc-squashfs.011a.128
		fi
	fi
else
	if [ "${fastboot}" = "fastboot" ]; then
		if [ "${chip}" = "ssd201" ]; then 
			./setup_config.sh ./configs/nvr/i2m/8.2.1/spinand.ram-glibc-squashfs.011a.64
		elif [ "${chip}" = "ssd202" ]; then	
			./setup_config.sh ./configs/nvr/i2m/8.2.1/spinand.ram-glibc-squashfs.011a.128
		fi
	else
		if [ "${chip}" = "ssd201" ]; then
			./setup_config.sh ./configs/nvr/i2m/8.2.1/spinand.glibc.011a.64
		fi
		if [ "${chip}" = "ssd202" ]; then
			./setup_config.sh ./configs/nvr/i2m/8.2.1/spinand.glibc.011a.128
		fi
	fi

fi


cd ${RELEASEDIR}/project/kbuild/4.9.84
if [ "${flashtype}" = "nor" ]; then
	if [ "${fastboot}" = "fastboot" ]; then
		./release.sh -k ${RELEASEDIR}/kernel -b 011A-fastboot -p nvr -f nor -c i2m -l glibc -v 8.2.1
	else
		./release.sh -k ${RELEASEDIR}/kernel -b 011A -p nvr -f nor -c i2m -l glibc -v 8.2.1
	fi
else
	if [ "${fastboot}" = "fastboot" ]; then
		echo fast release
		./release.sh -k ${RELEASEDIR}/kernel -b 011A-fastboot -p nvr -f spinand -c i2m -l glibc -v 8.2.1
	else
		./release.sh -k ${RELEASEDIR}/kernel -b 011A -p nvr -f spinand -c i2m -l glibc -v 8.2.1
	fi
	
fi

cd ${RELEASEDIR}/project
make clean;make image-nocheck

# Save the rootfs before copying latest release.
if [ -f ${RELEASEDIR}/project/image/rootfs/rootfs ]; then
    mv ${RELEASEDIR}/project/image/rootfs/rootfs ${RELEASEDIR}/project/image/rootfs/rootfs.${BACKUPDIR}
fi
if [ -f ${RELEASEDIR}/project/image/rootfs/rootfs.tar.gz ]; then
    mv ${RELEASEDIR}/project/image/rootfs/rootfs.tar.gz ${RELEASEDIR}/project/image/rootfs/rootfs.tar.gz.${BACKUPDIR}
fi
# Copy latest release.
echo "Copying rootfs to image dir..."
mkdir -p ${RELEASEDIR}/project/image/rootfs/rootfs
cd ${RELEASEDIR}/project/image/rootfs/rootfs
tar -xf ${RELEASEDIR}/buildroot/output/images/rootfs.tar
cd ${RELEASEDIR}/project/image/rootfs
tar -cf rootfs.tar.gz rootfs

#install Image
cd ${RELEASEDIR}
#rm -rf ${RELEASEDIR}/images
#mv ${RELEASEDIR}/images ${RELEASEDIR}/images.${BACKUPDIR}
rm -fr ${RELEASEDIR}/images/*
cp ${RELEASEDIR}/project/image/output/images images -rf

# End of test mode.
fi

# Create SD Root Filesystem. Based on the output of the Flash rootfs, rather than reinvent the wheel and heavily modify the original Industrio dev, use the generated FS and
# patch as needed for FusionX target.
echo "Building sdrootfs...."
if [ -f ${RELEASEDIR}/project/image/output/sdrootfs ]; then
#    mv ${RELEASEDIR}/project/image/output/sdrootfs ${RELEASEDIR}/project/image/output/sdrootfs.${BACKUPDIR}
   rm -fr ${RELEASEDIR}/project/image/output/sdrootfs/*
fi
mkdir -p ${RELEASEDIR}/project/image/output/sdrootfs
if [ -d ${RELEASEDIR}/project/image/output/rootfs ]; then
    echo -n "rootfs "
    cd ${RELEASEDIR}/project/image/output/rootfs
    tar -cf ${RELEASEDIR}/project/image/output/images/rootfs.tar.gz *
    cd ${RELEASEDIR}/project/image/output/sdrootfs
    tar -xf ${RELEASEDIR}/project/image/output/images/rootfs.tar.gz
fi
if [ -d ${RELEASEDIR}/project/image/output/miservice ]; then
    echo -n "miservice "
    cd ${RELEASEDIR}/project/image/output/miservice
    tar -cf ${RELEASEDIR}/project/image/output/images/miservice.tar.gz *
    cd ${RELEASEDIR}/project/image/output/sdrootfs
    tar -xf ${RELEASEDIR}/project/image/output/images/miservice.tar.gz
fi
if [ -d ${RELEASEDIR}/project/image/output/appconfigs ]; then
    echo -n "appconfigs "
    cd ${RELEASEDIR}/project/image/output/appconfigs
    tar -cf ${RELEASEDIR}/project/image/output/images/appconfigs.tar.gz *
    cd ${RELEASEDIR}/project/image/output/sdrootfs/etc
    tar -xf ${RELEASEDIR}/project/image/output/images/appconfigs.tar.gz
    rm -fr ${RELEASEDIR}/project/image/output/sdrootfs/appconfigs
fi
if [ -d ${RELEASEDIR}/project/image/output/customer -a "`ls ${RELEASEDIR}/project/image/output/customer/`" != "" ]; then
    echo -n "customer "
    cd ${RELEASEDIR}/project/image/output/customer
    tar -cf ${RELEASEDIR}/project/image/output/images/customer.tar.gz *
    cd ${RELEASEDIR}/project/image/output/sdrootfs/root
    tar -xf ${RELEASEDIR}/project/image/output/images/customer.tar.gz
    rm -fr ${RELEASEDIR}/project/image/output/sdrootfs/customer
fi
# Setup the applications for FusionX.
mkdir -p ${RELEASEDIR}/project/image/output/sdrootfs/apps
if [ -d ${FUSIONXDIR} ]; then
    echo -n "FusionX "
    cd ${FUSIONXDIR}
    mkdir -p ${RELEASEDIR}/project/image/output/sdrootfs/apps/FusionX/
    cp -r * ${RELEASEDIR}/project/image/output/sdrootfs/apps/FusionX/
    cp start_FusionX.sh ${RELEASEDIR}/project/image/output/sdrootfs/apps/
    chmod +x ${RELEASEDIR}/project/image/output/sdrootfs/apps/start_FusionX.sh
fi
if [ -d ${WEBSRVDIR} ]; then
    echo -n "WebServer "
    cd ${WEBSRVDIR}
    mkdir -p ${RELEASEDIR}/project/image/output/sdrootfs/apps/WebServer/
    cp -r WebServer webfs/ conf/ ${RELEASEDIR}/project/image/output/sdrootfs/apps/WebServer/
    cp start_WebServer.sh ${RELEASEDIR}/project/image/output/sdrootfs/apps/
    chmod +x ${RELEASEDIR}/project/image/output/sdrootfs/apps/start_WebServer.sh
fi
if [ -d ${Z80EMUDIR} ]; then
    echo -n "Z80 "
    cd ${Z80EMUDIR}
    mkdir -p ${RELEASEDIR}/project/image/output/sdrootfs/apps/Z80/
    cp -r Z80/* ${RELEASEDIR}/project/image/output/sdrootfs/apps/Z80/
fi
if [ -d /srv/dvlp/Projects/tzpu/FusionX/software/linux/project/custom ]; then
    echo -n "Custom "
    cd /srv/dvlp/Projects/tzpu/FusionX/software/linux/project/
    mkdir -p ${RELEASEDIR}/project/image/output/sdrootfs/apps/custom/
    cp -r custom/* ${RELEASEDIR}/project/image/output/sdrootfs/apps/custom/
fi

# Copy any new files.
for f in DSK MZF CPM BAS CAS Basic
do
    mkdir -p ${RELEASEDIR}/project/image/output/sdrootfs/apps/FusionX/disk/
    cp -r /srv/dvlp/Projects/tzpu/FusionX/software/${f}/ ${RELEASEDIR}/project/image/output/sdrootfs/apps/FusionX/disk/${f}/
done

# Copy an new rom images.
mkdir -p ${RELEASEDIR}/project/image/output/sdrootfs/apps/FusionX/roms/
cp -r /srv/dvlp/Projects/tzpu/FusionX/software/roms/* ${RELEASEDIR}/project/image/output/sdrootfs/apps/FusionX/roms/

# Make any manual setup changes.
#echo "/dev/zram0     none swap sw 0 -1"         >> ${RELEASEDIR}/project/image/output/sdrootfs/etc/fstab
#echo "/dev/mmcblk1p1 none swap sw 0 -2"         >> ${RELEASEDIR}/project/image/output/sdrootfs/etc/fstab
#echo "export LD_LIBRARY_PATH=/config/lib:/config/wifi" >> ${RELEASEDIR}/project/image/output/sdrootfs/etc/profile
#echo "export PATH=\$PATH:/config:/config/wifi" >> ${RELEASEDIR}/project/image/output/sdrootfs/etc/profile
#echo "echo 8388608 > /sys/block/zram0/disksize" >> ${RELEASEDIR}/project/image/output/miservice/config/setup_sd.sh
#echo "mkswap /dev/zram0"                        >> ${RELEASEDIR}/project/image/output/miservice/config/setup_sd.sh
#echo "swapon /dev/zram0"                        >> ${RELEASEDIR}/project/image/output/miservice/config/setup_sd.sh
#echo "mount -a"                                 >> ${RELEASEDIR}/project/image/output/miservice/config/setup_sd.sh

# Remove unecessary files.
cd ${RELEASEDIR}/project/image/output/sdrootfs
rm -fr customer
rm -fr appconfigs
rm -fr vendor

# Zip up into tarball for writing onto SD card.
echo "Building sdrootfs.tar.gz...."
cd ${RELEASEDIR}/project/image/output/sdrootfs
tar -cf ${RELEASEDIR}/project/image/output/images/sdrootfs.tar.gz *

# Build Upgrade images.
echo "Building Upgrade image in project/image/output/images/ - "
cd ${RELEASEDIR}/project/
echo -n "SigmastarUpgradeSD.bin "
yes | ./make_sd_upgrade_sigmastar.sh
echo -n "SigmastarUpgrade.bin "
yes | ./make_usb_upgrade_sigmastar.sh
echo ""

echo "Copy image to SD card or USB drive, install and reboot FusionX to upgrade."
echo "Make new SD filesystem by commands:"
echo "  sudo bash"
echo "  fdisk # Create three partitions, one FAT32 at disk start, one swap, the last an ext2/3 filesystem, ie:"
echo "      /dev/sdc1          2048  2099199  2097152    1G  b W95 FAT32"
echo "      /dev/sdc2       2099200  6293503  4194304    2G 82 Linux swap / Solaris"
echo "      /dev/sdc3       6293504 62333951 56040448 26.7G 83 Linux"
echo "  mkfs.vfat /dev/<sd partition 1>"
echo "  mkfs.ext2 /dev/<sd partition 3>"
echo "  mkdir -p /mnt/upgrade; mount /dev/<sd partition 1> /mnt/upgrade"
echo "  mkdir -p /mnt/rootfd;  mount /dev/<sd partition 3> /mnt/rootfs"
echo "  cp ${RELEASEDIR}/project/image/output/images/SigmastarUpgradeSD.bin /mnt/upgrade/FusionX.bin"
echo "  cd /mnt/rootfs; tar -xvf /dvlp/Projects/tzpu/FusionX/software/linux/project/image/output/images/sdrootfs.tar.gz; sync; sync"
echo "  cd /; umount /mnt/upgrade; umount /mnt/rootfs"

#tar -cvzf boot_${DATE}.tar.gz boot
#tar -cvzf kernel_${DATE}.tar.gz kernel
#tar -cvzf project_${DATE}.tar.gz project
#tar -cvzf sdk_${DATE}.tar.gz sdk


