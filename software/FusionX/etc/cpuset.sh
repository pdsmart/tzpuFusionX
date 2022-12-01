#!/bin/sh

for f in `ps -eaf |grep -v kthread_z80 | awk '{print $1}'`
do
 taskset -pc 0 $f >/dev/null 2>/dev/null
done

for I in $(ls /proc/irq)
do
    if [[ -d "/proc/irq/$I" ]]
    then
        echo 0 > /proc/irq/$I/smp_affinity_list 2>/dev/null
    fi
done
