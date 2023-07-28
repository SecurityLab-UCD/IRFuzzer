#!/bin/bash

# check if we are root
if [ "$(id -u)" != "0" ]; then
    echo "This script must be run as root"
    exit 1
fi

echo core >/proc/sys/kernel/core_pattern
cd /sys/devices/system/cpu
echo performance | tee cpu*/cpufreq/scaling_governor
