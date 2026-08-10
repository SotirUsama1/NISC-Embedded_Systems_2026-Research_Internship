#!/bin/bash

SYSCFG="/Applications/ti/ccs2100/ccs/utils/sysconfig_1.28.0"

echo "Checking SysConfig..."

if [ ! -d "$SYSCFG" ]; then
    echo "SysConfig not found."
    exit 1
fi

if [ -d "$SYSCFG/dist/deviceData/CC1352P7" ]; then
    echo "✓ CC1352P7 is supported."
else
    echo "✗ CC1352P7 device files are missing."
    echo
    echo "Your SysConfig version is too old."
    echo "Install a newer TI SDK / SysConfig."
fi
