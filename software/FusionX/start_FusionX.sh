#!/bin/sh
#########################################################################################################
##
## Name:            start_FusionX.sh
## Created:         March 2023
## Author(s):       Philip Smart
## Description:     Sharp MZ series FusionX start script.
##                  This script starts required services which form the FusionX ecosphere.
##                  tranZPUter SW memory as a User ROM application.
##
## Credits:         
## Copyright:       (c) 2018-2023 Philip Smart <philip.smart@net2net.org>
##
## History:         March 2023    - Initial script written.
##
#########################################################################################################
## This source file is free software: you can redistribute it and#or modify
## it under the terms of the GNU General Public License as published
## by the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This source file is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.
#########################################################################################################

# Constants.
FUSIONXDIR=/apps/FusionX

cd ${FUSIONXDIR}

# If not already installed, install the Sharp MZ TTY terminal driver.
if [ "`lsmod | grep ttymzdrv`" = "" ]; then
    echo "Loading SharpMZ TTY Driver."
    cd ${FUSIONXDIR}/modules
    insmod ttymzdrv.ko
    sleep 1
fi

# Start a Getty login process on the SharpMZ TTY terminal.
setsid getty 9600 /dev/ttymz0 ansi &

# Detach CPU 1 from scheduler and IRQ's as it will be dedicated to the z80drv.
for f in `ps -eaf |grep -v kthread_z80 | awk '{print $1}'`
do
    taskset -pc 0 $f >/dev/null 2>/dev/null
done

# Detach IRQ's
for I in $(ls /proc/irq)
do
    if [[ -d "/proc/irq/$I" ]]
    then
        echo 0 > /proc/irq/$I/smp_affinity_list 2>/dev/null
    fi
done

# Load the Z80 driver. Small pause to ensure the driver is fully loaded and initialsed prior to loading ROM images.
echo "Loading Z80 Driver."
cd ${FUSIONXDIR}/modules
rmmod z80drv 2>/dev/null
insmod z80drv.ko
sleep 1

# Start the K64F virtual cpu.
echo "Starting K64F Virtual CPU."
${FUSIONXDIR}/bin/k64fcpu &

# Start the Sharp MZ Arbiter.
echo "Starting Sharp MZ Arbiter."
${FUSIONXDIR}/bin/sharpbiter &

# Ensure the system is set for performance mode with max frequency.
# NB: Enabling this prior to starting the Z80 results in a kernel error.
echo performance > /sys/devices//system/cpu/cpufreq/policy0/scaling_governor
echo 1200000 > /sys/devices//system/cpu/cpufreq/policy0/scaling_min_freq 

# Done.
echo "FusionX running."
