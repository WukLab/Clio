SUMMARY = "Recipe for  build an external xilinx-axidma Linux kernel module"
SECTION = "PETALINUX/modules"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://COPYING;md5=12f884d2ae1ff87c09e5b7ccc2c4ca7e"

inherit module

SRC_URI = "file://Makefile \ 
           file://axi_dma.c \
           file://axidma_chrdev.c \
           file://axidma_dma.c \
           file://axidma_of.c \
           file://axidma.h \
           file://axidma_ioctl.h \
           file://COPYING \
          "

S = "${WORKDIR}"

# The inherit of module.bbclass will automatically name module packages with
# "kernel-module-" prefix as required by the oe-core build environment.
