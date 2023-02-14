#!/bin/sh

FUSIONXDIR=/apps/FusionX

# Setup screen width, used to load correct RFS ROM images.
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

# Load the original RFS ROM images.
${FUSIONXDIR}/bin/z80ctrl --loadrom --file  ${FUSIONXDIR}/rom/MROM_256_${SCREENWIDTH}c.bin         --addr 0x000000
${FUSIONXDIR}/bin/z80ctrl --loadrom --file  ${FUSIONXDIR}/rom/USER_ROM_256_${SCREENWIDTH}c.bin     --addr 0x80000
${FUSIONXDIR}/bin/z80ctrl --loadrom --file  ${FUSIONXDIR}/rom/USER_ROM_II_256_${SCREENWIDTH}c.bin  --addr 0x100000
${FUSIONXDIR}/bin/z80ctrl --loadrom --file  ${FUSIONXDIR}/rom/USER_ROM_III_256_${SCREENWIDTH}c.bin --addr 0x180000

# Add the RFS Virtual Hardware to the driver.
${FUSIONXDIR}/bin/z80ctrl --adddev --device rfs

# Start the Z80 (ie. MZ-80A virtual processor).
${FUSIONXDIR/bin/z80ctrl --start

# Ensure the system is set for performance mode with max frequency.
# NB: Enabling this prior to starting the Z80 results in a kernel error.
echo performance > /sys/devices//system/cpu/cpufreq/policy0/scaling_governor
echo 1200000 > /sys/devices//system/cpu/cpufreq/policy0/scaling_min_freq 

# Done.
echo "FusionX loaded and configured in RFS mode."
