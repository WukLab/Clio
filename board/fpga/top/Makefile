#
# Copyright (c) 2020, Wuklab, UCSD.
#

VIVADO = vivado
FLAGS ?= release

all:
	$(VIVADO) -mode tcl -source scripts/run_vivado.tcl

g:
	$(VIVADO) generated_vivado_project/generated_vivado_project.xpr &

clean:
	find ./ -name "generated_hls_project" | xargs rm -rf
	find ./ -name "generated_vivado_project" | xargs rm -rf
	find ./ -name "*.log" | xargs rm -rf
	find ./ -name "*.jou" | xargs rm -rf
	find ./ -name "*.str" | xargs rm -rf
	find ./ -name ".Xil" | xargs rm -rf
	find ./ -name "awsver.txt" | xargs rm -rf
