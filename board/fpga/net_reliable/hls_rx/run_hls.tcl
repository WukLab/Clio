#
# Copyright (c) 2020, Wuklab, UCSD.
#

# If the script is run in this way:
#   vivado_hls -f run_hls.tcl -tclargs $(TARGET_BOARD)
set board "[lindex $argv 2]"

# Create a project
open_project	-reset generated_hls_project 

add_files	rx_64_top.cpp	-cflags -I../../../../include/
add_files -tb	tb.cpp		-cflags -I../../../../include/

# Specify the top-level function for synthesis
set_top		rx_64

###########################
# Solution settings

# Create solution1
open_solution -reset solution1

# Specify a Xilinx device and clock period
#
# VCU118:	xcvu9p-flga2104-1-i
# VCU108:	xcvu095-ffva2104-2-e
# ZCU106:	xczu7ev-ffvc1156-2-e
# ArtyA7:	xc7a100tcsg324-1
switch $board {
	vcu118 {
		set_part {xcvu9p-flga2104-1-i}
	}
	vcu108 {
		set_part {xcvu095-ffva2104-2-e}
	}
	zcu106 {
		set_part {xczu7ev-ffvc1156-2-e}
	}
	default {
		puts "Unknown board: $board"
		exit
	}
}
create_clock -period 4.00 -name default
set_clock_uncertainty 0.25

# UG902: Resets all registers and memories in the design.
# Any static or global variable variables initialized in the C
# code is reset to its initialized value.
config_rtl -reset all -reset_async

# Simulate the C code 
#csim_design

# Synthesis the C code
csynth_design

# Export IP block
export_design -format ip_catalog -display_name "relnet_rx_hls" -description "reliable network receiver HLS" -vendor "Wuklab.UCSD" -version "1.0"

# Do not perform any other steps
# - The basic project will be opened in the GUI 
exit
