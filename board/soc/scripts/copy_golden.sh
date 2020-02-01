#!/bin/bash

# Parameter check
if [ -z "$1" ]; then
    echo "$0 - Setup Petalinux Project, copy default configs and scripts"
    echo "Usage: $0 HARDWARE_DIRECTORY"
fi

# config
cp ../golden/project-spec/configs/config project-spec/configs
cp ../golden/project-spec/configs/config.old project-spec/configs
cp ../golden/project-spec/configs/rootfs_config project-spec/configs
cp ../golden/project-spec/configs/rootfs_config.old project-spec/configs

# u-boot startup file
cp ../golden/project-spec/meta-user/recipes-bsp/u-boot/files/platform-top.h project-spec/meta-user/recipes-bsp/u-boot/files

# Makefile
cp ../golden/Makefile .
echo "HARDWARE_DIR = $1" > config.mk


