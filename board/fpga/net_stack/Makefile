#
# Copyright (c) 2020, Wuklab, UCSD.
#

VIVADO = vivado

HLS_IP_CORES	:= hls_rx,hls_tx,hls_queue,tx_arbiter

all:
	$(Q)./generate_hls.sh $(HLS_IP_CORES)
	$(VIVADO) -mode tcl -source run_vivado.tcl
	
hls:
	$(Q)./generate_hls.sh $(HLS_IP_CORES)

g:
	$(VIVADO) generated_vivado_project/generated_vivado_project.xpr

clean:
	find ./ -name "generated_hls_project" | xargs rm -rf
	find ./ -name "generated_vivado_project" | xargs rm -rf
	find ./ -name "*.log" | xargs rm -rf
	find ./ -name "*.jou" | xargs rm -rf
	find ./ -name "*.str" | xargs rm -rf
	find ./ -name ".Xil" | xargs rm -rf
	find ./ -name "awsver.txt" | xargs rm -rf
