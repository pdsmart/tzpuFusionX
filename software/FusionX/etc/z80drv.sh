#!/bin/sh

FUSIONXDIR=/apps/FusionX

cd ${FUSIONXDIR}/modules
${FUSIONXDIR/etc/cpuset.sh
rmmod z80drv 2>/dev/null
insmod z80drv.ko
#echo performance > /sys/devices//system/cpu/cpufreq/policy0/scaling_governor 
#drvid=`ps -eaf | grep kthread_z80 | grep -v grep | awk '{print $1}'`
#taskset -pc 1 $drvid
