#!/bin/sh

for f in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 47 48 46 85 90
do
    echo ${f} > /sys/class/gpio/export
    echo out > /sys/class/gpio/gpio${f}/direction
done

echo 0 > /sys/class/gpio/gpio0/value
echo 0 > /sys/class/gpio/gpio1/value
echo 0 > /sys/class/gpio/gpio2/value
echo 0 > /sys/class/gpio/gpio3/value
echo 0 > /sys/class/gpio/gpio4/value
echo 0 > /sys/class/gpio/gpio5/value
echo 0 > /sys/class/gpio/gpio6/value
echo 0 > /sys/class/gpio/gpio7/value
echo 0 > /sys/class/gpio/gpio8/value
echo 0 > /sys/class/gpio/gpio9/value
echo 0 > /sys/class/gpio/gpio10/value
echo 0 > /sys/class/gpio/gpio11/value
echo 0 > /sys/class/gpio/gpio12/value
echo 0 > /sys/class/gpio/gpio13/value
echo 0 > /sys/class/gpio/gpio14/value
echo 0 > /sys/class/gpio/gpio47/value

echo 1 > /sys/class/gpio/gpio46/value
echo 1 > /sys/class/gpio/gpio48/value
echo 1 > /sys/class/gpio/gpio85/value
echo 1 > /sys/class/gpio/gpio90/value

echo 0 > /sys/class/gpio/gpio90/value
echo 0 > /sys/class/gpio/gpio46/value
echo 1 > /sys/class/gpio/gpio46/value
echo 1 > /sys/class/gpio/gpio90/value



