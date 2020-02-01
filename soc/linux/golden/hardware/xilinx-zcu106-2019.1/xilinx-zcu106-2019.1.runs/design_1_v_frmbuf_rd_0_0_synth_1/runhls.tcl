open_project design_1_v_frmbuf_rd_0_0
set_top v_frmbuf_rd
add_files -cflags " -I /tmp/tmp.uj6Pgihdhk/temp/project_1.srcs/sources_1/bd/design_1/ip/design_1_v_frmbuf_rd_0_0/src " /tmp/tmp.uj6Pgihdhk/temp/project_1.srcs/sources_1/bd/design_1/ip/design_1_v_frmbuf_rd_0_0/src/v_frmbuf_rd_config.h
add_files -cflags " -I /tmp/tmp.uj6Pgihdhk/temp/project_1.srcs/sources_1/bd/design_1/ip/design_1_v_frmbuf_rd_0_0/src " /tmp/tmp.uj6Pgihdhk/temp/project_1.srcs/sources_1/bd/design_1/ip/design_1_v_frmbuf_rd_0_0/src/v_frmbuf_rd.cpp
add_files -cflags " -I /tmp/tmp.uj6Pgihdhk/temp/project_1.srcs/sources_1/bd/design_1/ip/design_1_v_frmbuf_rd_0_0/src " /tmp/tmp.uj6Pgihdhk/temp/project_1.srcs/sources_1/bd/design_1/ip/design_1_v_frmbuf_rd_0_0/src/v_frmbuf_rd.h

open_solution "prj"
set_part {xczu7ev-ffvc1156-2-e}
create_clock -period 3.019 -name ap_clk

                 
config_rtl -enable_axiFlushable


config_interface -input_reg_mode both -output_reg_mode both -m_axi_addr64


config_rtl -prefix design_1_v_frmbuf_rd_0_0_
csynth_design
export_design -format ip_catalog -vendor xilinx.com -library ip -version 2.1
exit
