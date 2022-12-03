#!/bin/sh

for f in 48 85 86 90
do
    echo ${f} > /sys/class/gpio/export
    echo out > /sys/class/gpio/gpio${f}/direction
done

echo 1 > /sys/class/gpio/gpio48/value
echo 1 > /sys/class/gpio/gpio85/value
echo 1 > /sys/class/gpio/gpio86/value
echo 1 > /sys/class/gpio/gpio90/value

