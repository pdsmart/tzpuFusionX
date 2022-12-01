#!/bin/sh

cd /customer
./cpuset.sh
rmmod z80drv 2>/dev/null
insmod z80drv.ko
#drvid=`ps -eaf | grep kthread_z80 | grep -v grep | awk '{print $1}'`
#taskset -pc 1 $drvid
