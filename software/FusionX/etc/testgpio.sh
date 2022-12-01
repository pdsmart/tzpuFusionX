#!/bin/sh

for f in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 47 48 85 86 90 73 59
do
	echo "PIN: ${f} ---------------------------------------------"
	sleep 1
	echo "Export PIN"
	echo $f > /sys/class/gpio/export
	echo "  Direction OUT"
	echo out > /sys/class/gpio/gpio${f}/direction
	echo "    OUT 1"
	echo 1 > /sys/class/gpio/gpio${f}/value
	echo "    OUT 0"
	echo 0 > /sys/class/gpio/gpio${f}/value
	echo "  Direction IN"
	echo in > /sys/class/gpio/gpio${f}/direction
	echo "    IN"
	cat /sys/class/gpio/gpio${f}/value
	echo "-------------------------------------------------------"
	sleep 1
done
