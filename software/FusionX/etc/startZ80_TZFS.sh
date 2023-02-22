#!/bin/sh

FUSIONXDIR=/apps/FusionX

# Setup screen width, used to load correct Monitor ROM image.
SCREENWIDTH=40
if [[ "$#" -ne 0 ]]; then
    if [[ "$1" -eq 80 ]]; then
        SCREENWIDTH=80
    fi
fi
echo "Screen width set to: ${SCREENWIDTH}"

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
cd ${FUSIONXDIR}/modules
rmmod z80drv 2>/dev/null
insmod z80drv.ko
sleep 1

# Load the original Monitor and TZFS ROM images. This is done in the K64F daemon but can be manually enabled.
#${FUSIONXDIR}/bin/z80ctrl --loadrom --file  ${FUSIONXDIR}/roms/monitor_${SCREENWIDTH}c_sa1510.rom        --addr 0x000000  --type 1
#${FUSIONXDIR}/bin/z80ctrl --loadrom --file  ${FUSIONXDIR}/roms/tzfs.rom --offset 0x000000 --len 0x001800 --addr 0x00E800  --type 1
#${FUSIONXDIR}/bin/z80ctrl --loadrom --file  ${FUSIONXDIR}/roms/tzfs.rom --offset 0x001800 --len 0x001000 --addr 0x01F000  --type 1
#${FUSIONXDIR}/bin/z80ctrl --loadrom --file  ${FUSIONXDIR}/roms/tzfs.rom --offset 0x002800 --len 0x001000 --addr 0x02F000  --type 1
#${FUSIONXDIR}/bin/z80ctrl --loadrom --file  ${FUSIONXDIR}/roms/tzfs.rom --offset 0x003800 --len 0x001000 --addr 0x03F000  --type 1

# Add the TZPU Virtual Hardware to the driver.
${FUSIONXDIR}/bin/z80ctrl --adddev --device tzpu

# Start the K64F Virtual CPU Emulation.
${FUSIONXDIR}/bin/k64fcpu &

# Ensure the system is set for performance mode with max frequency.
# NB: Enabling this prior to starting the Z80 results in a kernel error.
echo performance > /sys/devices//system/cpu/cpufreq/policy0/scaling_governor
echo 1200000 > /sys/devices//system/cpu/cpufreq/policy0/scaling_min_freq 

# Done.
echo "FusionX loaded and configured in RFS mode."
