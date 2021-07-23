#
# Copyright (c) 2020-2021, Wuklab, UCSD.
#

.DEFAULT_GOAL := all

RTL_SRC=generated_rtl
VERILOG_LIB_DIR=src/lib/verilog

all: gen_rtl package_ip net top

# Step 1: Generate RTL from Scala
gen_rtl:
	mkdir -p generated_rtl
	sbt "runMain wuklab.LegoMemSystemGenerate"
	sbt "runMain wuklab.MonitorRegistersGenerator"
	sbt "runMain wuklab.LegoMemSystem512Generate"
	sbt "runMain wuklab.PingPongGenerate"
	sbt "runMain wuklab.KeyValuePhysicalInterfaceGenerate"
	sbt "runMain wuklab.KeyValueVirtualInterfaceGenerate"

# Step 2: Package IPs
package_ip:
	vivado -mode tcl -source scripts/create_system.tcl
	vivado -mode tcl -source scripts/create_pingpong.tcl
	vivado -mode tcl -source scripts/create_monitor.tcl
	vivado -mode tcl -source scripts/create_ip.tcl -tclargs kv_physcial $(RTL_SRC)/KeyValuePhysicalEndpoint.v $(VERILOG_LIB_DIR)/legomem_system_lib.v
	vivado -mode tcl -source scripts/create_ip.tcl -tclargs kv_virtual  $(RTL_SRC)/KeyValueVirtualEndpoint.v $(VERILOG_LIB_DIR)/legomem_system_lib.v
	# vivado -mode tcl -source scripts/create_ip.tcl -tclargs $(RTL_SRC)/kv_virtual  $(RTL_SRC)/kv_virtual.v  $(RTL_SRC)/legomem_system_lib.v
	# vivado -mode tcl -source scripts/create_ip.tcl -tclargs pointer_chasing pointer_chasing.v
	# vivado -mode tcl -source scripts/create_ip.tcl -tclargs data_frame data_frame.v

# Step 3: Generate reliable network
net:
	make -C net_reliable

# Step 4: Generate Top
top:
	make -C top

clean:
	rm -rf generated_rtl/
	rm -rf generated_ip/
	make -C net_reliable clean
	make -C top clean
	find ./ -name "*.log" | xargs rm -rf
	find ./ -name "*.jou" | xargs rm -rf
	find ./ -name "*.str" | xargs rm -rf
	find ./ -name ".Xil" | xargs rm -rf
	find ./ -name "awsver.txt" | xargs rm -rf
