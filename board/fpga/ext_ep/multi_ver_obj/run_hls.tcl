#
# Copyright (c) 2020, Wuklab, UCSD.
#

# If the script is run in this way:
#   vivado_hls -f run_hls.tcl -tclargs $(TARGET_BOARD)
set board "[lindex $argv 2]"

# Create a project
open_project	-reset generated_hls_project

set CFLAGS "-I../../../../include/ -I../include"
add_files	dispatcher.cpp			-cflags $CFLAGS
add_files	parser.cpp			-cflags $CFLAGS
add_files	versions.cpp			-cflags $CFLAGS
add_files	waitqueue1.cpp			-cflags $CFLAGS
add_files	waitqueue2.cpp			-cflags $CFLAGS
add_files	sendqueue.cpp			-cflags $CFLAGS
add_files	multi_ver_obj.cpp		-cflags $CFLAGS

add_files -tb	multi_ver_obj_tb.cpp		-cflags $CFLAGS
add_files -tb	versions_tb.cpp			-cflags $CFLAGS
add_files -tb	../tb/mem_sim.cpp		-cflags $CFLAGS
add_files -tb	../tb/soc_sim.cpp		-cflags $CFLAGS
add_files -tb	../tb/net_sim.cpp		-cflags $CFLAGS
add_files -tb	../tb/xbar_sim.cpp		-cflags $CFLAGS
add_files -tb	../tb/pattern_generator.cpp	-cflags $CFLAGS

# Specify the top-level function for synthesis
set_top		multi_ver_obj2

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
create_clock -period 5.00 -name default
set_clock_uncertainty 0.25

# UG902: Resets all registers and memories in the design.
# Any static or global variable variables initialized in the C
# code is reset to its initialized value.
config_rtl -reset all -reset_level low

# Simulate the C code
#csim_design

# Synthesis the C code
csynth_design

# Export IP block
export_design -format ip_catalog -display_name "multi_ver_obj_hls" -description "extended API multiversion object HLS" -vendor "Wuklab.UCSD" -version "1.0"

# Do not perform any other steps
# - The basic project will be opened in the GUI
exit
