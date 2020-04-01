# DO NOT CHANGE THIS. This is coded through the repo
project_path=generated_hls_project

VIVADO_HLS = vivado_hls

ifeq ($(TARGET_BOARD), )
  TARGET_BOARD = zcu106
endif

all:
	$(VIVADO_HLS) -f run_hls.tcl -tclargs $(TARGET_BOARD)

g:
	$(VIVADO_HLS) -p $(project_path) &

clean:
	rm -rf $(project_path)
	find ./ -name "*.log" | xargs rm -rf
	find ./ -name "*.jou" | xargs rm -rf
	find ./ -name "*.str" | xargs rm -rf
	find ./ -name ".Xil" | xargs rm -rf

help:
	@echo "Help"
	@echo "  # make       <- Compile projects"
	@echo "  # make g     <- Open GUI"
