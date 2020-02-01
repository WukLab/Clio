include config.mk

RESET_SCRIPT = ../../scripts/rst.tcl
BITSTREAM = $(HARDWARE_DIR)/pl.bit

# import files
FILE_USER_DEVICETREE = project-spec/meta-user/recipes-bsp/device-tree/files/system-user.dtsi
FILE_PL_DEVICETREE = 

.PHONY: reset boot config

boot: reset
	petalinux-boot --jtag --u-boot --fpga --bitstream $(BITSTREAM)

reset:
	/tools/Xilinx/SDK/2019.1/bin/xsdb $(RESET_SCRIPT)

config:
	petalinux-config --get-hw-description=$(HARDWARE_DIR) --silentconfig
