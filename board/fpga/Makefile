#
# Copyright (c) 2020-2021, Wuklab, UCSD.
#

include Makefile.mk

.DEFAULT_GOAL := all

RTL_SRC=generated_rtl
VERILOG_LIB_DIR=src/lib/verilog

help h:
	@echo ""
	@echo "************************************************************************************"
	@echo "Clio Build Help"
	@echo ""
	@echo "  [Default]"
	@echo "    make or make all             -> Build everything sequentially"
	@echo "         This include: gen_rtl, package_ip, net, and top"
	@echo ""
	@echo "  [Single Step]"
	@echo "    make gen_rtl or make gen     -> Compile SpinalHDL into RTL"
	@echo "    make package_ip or make ip   -> Package compiled RTL into Vivado IPs"
	@echo "    make net                     -> Compile and package the reliable transport layer"
	@echo "    make vivado_top or make top  -> Build the top Vivado"
	@echo "************************************************************************************"
	@echo ""

PHONY += all
all: gen_rtl package_ip net vivado_top

# Step 1: Generate RTL from Scala
PHONY += gen_rtl gen
gen_rtl gen:
	mkdir -p generated_rtl
	sbt "runMain wuklab.LegoMemSystemGenerate"
	sbt "runMain wuklab.MonitorRegistersGenerator"
	sbt "runMain wuklab.PingPongGenerate"
	sbt "runMain wuklab.KeyValuePhysicalInterfaceGenerate"
	sbt "runMain wuklab.KeyValueVirtualInterfaceGenerate"

# Step 2: Package IPs
PHONY += package_ip ip
package_ip ip:
	$(VIVADO) -nojournal -nolog -mode tcl -source scripts/create_system.tcl
	$(VIVADO) -nojournal -nolog -mode tcl -source scripts/create_pingpong.tcl
	$(VIVADO) -nojournal -nolog -mode tcl -source scripts/create_monitor_write.tcl
	$(VIVADO) -nojournal -nolog -mode tcl -source scripts/create_monitor_read.tcl
	$(VIVADO) -nojournal -nolog -mode tcl -source scripts/create_ip.tcl -tclargs kv_physcial $(RTL_SRC)/KeyValuePhysicalEndpoint.v $(VERILOG_LIB_DIR)/legomem_system_lib.v
	$(VIVADO) -nojournal -nolog -mode tcl -source scripts/create_ip.tcl -tclargs kv_virtual  $(RTL_SRC)/KeyValueVirtualEndpoint.v $(VERILOG_LIB_DIR)/legomem_system_lib.v
	#
	# vivado -mode tcl -source scripts/create_ip.tcl -tclargs pointer_chasing pointer_chasing.v
	# vivado -mode tcl -source scripts/create_ip.tcl -tclargs data_frame data_frame.v

# Step 3: Generate reliable network
PHONY += net
net:
	make -C net_reliable

# Step 4: Generate Top
PHONY += top vivado_top
vivado_top top:
	make -C top

PHONY += clean
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

.PHONY: $(PHONY)
