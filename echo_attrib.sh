#!/bin/bash
echo $1 > /sys/class/ledclass/led03/led_attr
dmesg