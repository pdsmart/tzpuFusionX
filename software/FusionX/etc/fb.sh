#!/bin/sh -x

fbdev=/dev/fb0 ;   width=1280 ; bpp=4
color="\x00\xFF\x00\x00" #red colored

function pixel()
{  xx=$1 ; yy=$2
   printf "$color" | dd bs=$bpp seek=$(($yy * $width + $xx)) \
                                   of=$fbdev &>/dev/null
}
x=0 ; y=0 ; clear
for i in {1..10000}; do
    pixel $((x++)) $((y++))
done
