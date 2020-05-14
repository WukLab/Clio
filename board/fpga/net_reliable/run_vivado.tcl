# 
# Note:
#
# This TCL will export the relnet BD design to the shared IP folder.
# All other stuff are here for simulation/testing purpose.
#

# Set the reference directory for source file relative paths (by default the value is script directory path)
set origin_dir "."

# Use origin directory path location variable, if specified in the tcl shell
if { [info exists ::origin_dir_loc] } {
  set origin_dir $::origin_dir_loc
}

# Set the project name
set _xil_proj_name_ "generated_vivado_project"

# Use project name variable, if specified in the tcl shell
if { [info exists ::user_project_name] } {
  set _xil_proj_name_ $::user_project_name
}

variable script_file
set script_file "run_vivado.tcl"

if { $::argc > 0 } {
  for {set i 0} {$i < $::argc} {incr i} {
    set option [string trim [lindex $::argv $i]]
    switch -regexp -- $option {
      "--origin_dir"   { incr i; set origin_dir [lindex $::argv $i] }
      "--project_name" { incr i; set _xil_proj_name_ [lindex $::argv $i] }
      "--help"         { print_help }
      default {
        if { [regexp {^-} $option] } {
          puts "ERROR: Unknown option '$option' specified, please type '$script_file -tclargs --help' for usage info.\n"
          return 1
        }
      }
    }
  }
}

# Set the directory path for the original project from where this script was exported
set orig_proj_dir "[file normalize "$origin_dir/generated_vivado_project"]"

# Create project
create_project -f ${_xil_proj_name_} "./generated_vivado_project" -part xczu7ev-ffvc1156-2-e

# Set the directory path for the new project
set proj_dir [get_property directory [current_project]]

# Set project properties
set obj [current_project]
set_property -name "board_part" -value "xilinx.com:zcu106:part0:2.4" -objects $obj
set_property -name "default_lib" -value "xil_defaultlib" -objects $obj
set_property -name "dsa.accelerator_binary_content" -value "bitstream" -objects $obj
set_property -name "dsa.accelerator_binary_format" -value "xclbin2" -objects $obj
set_property -name "dsa.board_id" -value "zcu106" -objects $obj
set_property -name "dsa.description" -value "Vivado generated DSA" -objects $obj
set_property -name "dsa.dr_bd_base_address" -value "0" -objects $obj
set_property -name "dsa.emu_dir" -value "emu" -objects $obj
set_property -name "dsa.flash_interface_type" -value "bpix16" -objects $obj
set_property -name "dsa.flash_offset_address" -value "0" -objects $obj
set_property -name "dsa.flash_size" -value "1024" -objects $obj
set_property -name "dsa.host_architecture" -value "x86_64" -objects $obj
set_property -name "dsa.host_interface" -value "pcie" -objects $obj
set_property -name "dsa.num_compute_units" -value "60" -objects $obj
set_property -name "dsa.platform_state" -value "pre_synth" -objects $obj
set_property -name "dsa.vendor" -value "xilinx" -objects $obj
set_property -name "dsa.version" -value "0.0" -objects $obj
set_property -name "enable_vhdl_2008" -value "1" -objects $obj
set_property -name "ip_cache_permissions" -value "read write" -objects $obj
set_property -name "ip_output_repo" -value "$proj_dir/${_xil_proj_name_}.cache/ip" -objects $obj
set_property -name "mem.enable_memory_map_generation" -value "1" -objects $obj
set_property -name "sim.central_dir" -value "$proj_dir/${_xil_proj_name_}.ip_user_files" -objects $obj
set_property -name "sim.ip.auto_export_scripts" -value "1" -objects $obj
set_property -name "simulator_language" -value "Mixed" -objects $obj
set_property -name "xpm_libraries" -value "XPM_CDC XPM_FIFO XPM_MEMORY" -objects $obj

# Create 'sources_1' fileset (if not found)
if {[string equal [get_filesets -quiet sources_1] ""]} {
  create_fileset -srcset sources_1
}

# Set IP repository paths
set obj [get_filesets sources_1]
set_property "ip_repo_paths" "[file normalize "$origin_dir/../generated_ip"]" $obj
update_ip_catalog -rebuild

# Set 'sources_1' fileset object
set obj [get_filesets sources_1]
set files [list \
 [file normalize "${origin_dir}/lib/axis/arbiter.v"] \
 [file normalize "${origin_dir}/lib/arp_64.v"] \
 [file normalize "${origin_dir}/lib/arp_cache.v"] \
 [file normalize "${origin_dir}/lib/arp_eth_rx_64.v"] \
 [file normalize "${origin_dir}/lib/arp_eth_tx_64.v"] \
 [file normalize "${origin_dir}/lib/axis/axis_async_fifo.v"] \
 [file normalize "${origin_dir}/lib/axis/axis_async_fifo_adapter.v"] \
 [file normalize "${origin_dir}/lib/axis/axis_fifo.v"] \
 [file normalize "${origin_dir}/lib/axis_xgmii_rx_64.v"] \
 [file normalize "${origin_dir}/lib/axis_xgmii_tx_64.v"] \
 [file normalize "${origin_dir}/lib/eth_arb_mux.v"] \
 [file normalize "${origin_dir}/lib/eth_axis_rx_64.v"] \
 [file normalize "${origin_dir}/lib/eth_axis_tx_64.v"] \
 [file normalize "${origin_dir}/lib/eth_mac_10g.v"] \
 [file normalize "${origin_dir}/lib/eth_mac_10g_fifo.v"] \
 [file normalize "${origin_dir}/lib/eth_phy_10g.v"] \
 [file normalize "${origin_dir}/lib/eth_phy_10g_rx.v"] \
 [file normalize "${origin_dir}/lib/eth_phy_10g_rx_ber_mon.v"] \
 [file normalize "${origin_dir}/lib/eth_phy_10g_rx_frame_sync.v"] \
 [file normalize "${origin_dir}/lib/eth_phy_10g_rx_if.v"] \
 [file normalize "${origin_dir}/lib/eth_phy_10g_tx.v"] \
 [file normalize "${origin_dir}/lib/eth_phy_10g_tx_if.v"] \
 [file normalize "${origin_dir}/rtl/fpga_core.v"] \
 [file normalize "${origin_dir}/lib/ip_64.v"] \
 [file normalize "${origin_dir}/lib/ip_arb_mux.v"] \
 [file normalize "${origin_dir}/lib/ip_complete_64.v"] \
 [file normalize "${origin_dir}/lib/ip_eth_rx_64.v"] \
 [file normalize "${origin_dir}/lib/ip_eth_tx_64.v"] \
 [file normalize "${origin_dir}/lib/lfsr.v"] \
 [file normalize "${origin_dir}/lib/axis/priority_encoder.v"] \
 [file normalize "${origin_dir}/rtl/sync_reset.v"] \
 [file normalize "${origin_dir}/lib/udp_64.v"] \
 [file normalize "${origin_dir}/lib/udp_checksum_gen_64.v"] \
 [file normalize "${origin_dir}/lib/udp_complete_64.v"] \
 [file normalize "${origin_dir}/lib/udp_ip_rx_64.v"] \
 [file normalize "${origin_dir}/lib/udp_ip_tx_64.v"] \
 [file normalize "${origin_dir}/lib/xgmii_baser_dec_64.v"] \
 [file normalize "${origin_dir}/lib/xgmii_baser_enc_64.v"] \
 [file normalize "${origin_dir}/rtl/fpga.v"] \
 [file normalize "${origin_dir}/rtl/sync_signal.v"] \
]
add_files -norecurse -fileset $obj $files

# Set 'sources_1' fileset file properties for remote files
# None

# Set 'sources_1' fileset file properties for local files
# None

# Set 'sources_1' fileset properties
set obj [get_filesets sources_1]
set_property -name "top" -value "fpga" -objects $obj
set_property -name "top_auto_set" -value "0" -objects $obj

# Set 'sources_1' fileset object
set obj [get_filesets sources_1]
# Import local files from the original project
set files [list \
 [file normalize "${origin_dir}/rtl/ip/gtwizard_ultrascale_0.xci"]\
]
set imported_files [import_files -fileset sources_1 $files]

# Set 'sources_1' fileset file properties for remote files
# None

# Set 'sources_1' fileset file properties for local files
set file "gtwizard_ultrascale_0/gtwizard_ultrascale_0.xci"
set file_obj [get_files -of_objects [get_filesets sources_1] [list "*$file"]]
set_property -name "generate_files_for_reference" -value "0" -objects $file_obj
set_property -name "registered_with_manager" -value "1" -objects $file_obj
if { ![get_property "is_locked" $file_obj] } {
  set_property -name "synth_checkpoint_mode" -value "Singular" -objects $file_obj
}

# Create 'constrs_1' fileset (if not found)
if {[string equal [get_filesets -quiet constrs_1] ""]} {
  create_fileset -constrset constrs_1
}

# Set 'constrs_1' fileset object
set obj [get_filesets constrs_1]

# Add/Import constrs file and set constrs file properties
set file "[file normalize "$origin_dir/rtl/fpga.xdc"]"
set file_added [add_files -norecurse -fileset $obj [list $file]]
set file "$origin_dir/rtl/fpga.xdc"
set file [file normalize $file]
set file_obj [get_files -of_objects [get_filesets constrs_1] [list "*$file"]]
set_property -name "file_type" -value "XDC" -objects $file_obj

# Add/Import constrs file and set constrs file properties
set file "[file normalize "$origin_dir/lib/syn/eth_mac_fifo.tcl"]"
set file_added [add_files -norecurse -fileset $obj [list $file]]
set file "$origin_dir/lib/syn/eth_mac_fifo.tcl"
set file [file normalize $file]
set file_obj [get_files -of_objects [get_filesets constrs_1] [list "*$file"]]
set_property -name "file_type" -value "TCL" -objects $file_obj

# Add/Import constrs file and set constrs file properties
set file "[file normalize "$origin_dir/lib/axis/syn/axis_async_fifo.tcl"]"
set file_added [add_files -norecurse -fileset $obj [list $file]]
set file "$origin_dir/lib/axis/syn/axis_async_fifo.tcl"
set file [file normalize $file]
set file_obj [get_files -of_objects [get_filesets constrs_1] [list "*$file"]]
set_property -name "file_type" -value "TCL" -objects $file_obj

# Set 'constrs_1' fileset properties
set obj [get_filesets constrs_1]
set_property -name "target_constrs_file" -value "[file normalize "$origin_dir/rtl/fpga.xdc"]" -objects $obj
set_property -name "target_ucf" -value "[file normalize "$origin_dir/rtl/fpga.xdc"]" -objects $obj

# Create 'sim_1' fileset (if not found)
if {[string equal [get_filesets -quiet sim_1] ""]} {
  create_fileset -simset sim_1
}

# Set 'sim_1' fileset object
set obj [get_filesets sim_1]
set files [list \
 [file normalize "${origin_dir}/rtl/tb/tb_rt.sv"] \
 [file normalize "${origin_dir}/rtl/tb/tb_intg.sv"] \
 [file normalize "${origin_dir}/rtl/tb/host_stack.v"] \
 [file normalize "${origin_dir}/rtl/tb/tb_rx.sv"] \
]
add_files -norecurse -fileset $obj $files

# Set 'sim_1' fileset file properties for remote files
set file "$origin_dir/rtl/tb/tb_rt.sv"
set file [file normalize $file]
set file_obj [get_files -of_objects [get_filesets sim_1] [list "*$file"]]
set_property -name "file_type" -value "SystemVerilog" -objects $file_obj

set file "$origin_dir/rtl/tb/tb_intg.sv"
set file [file normalize $file]
set file_obj [get_files -of_objects [get_filesets sim_1] [list "*$file"]]
set_property -name "file_type" -value "SystemVerilog" -objects $file_obj

set file "$origin_dir/rtl/tb/tb_rx.sv"
set file [file normalize $file]
set file_obj [get_files -of_objects [get_filesets sim_1] [list "*$file"]]
set_property -name "file_type" -value "SystemVerilog" -objects $file_obj


# Set 'sim_1' fileset file properties for local files
# None

# Set 'sim_1' fileset properties
set obj [get_filesets sim_1]
set_property -name "nl.mode" -value "funcsim" -objects $obj
set_property -name "top" -value "testbench_netstack_3" -objects $obj
set_property -name "top_auto_set" -value "0" -objects $obj
set_property -name "top_lib" -value "xil_defaultlib" -objects $obj

# Set 'utils_1' fileset object
set obj [get_filesets utils_1]
# Empty (no sources present)

# Set 'utils_1' fileset properties
set obj [get_filesets utils_1]

# Proc to create BD relnet
proc cr_bd_relnet { parentCell } {

  # CHANGE DESIGN NAME HERE
  set design_name relnet

  common::send_msg_id "BD_TCL-003" "INFO" "Currently there is no design <$design_name> in project, so creating one..."

  create_bd_design $design_name

  set bCheckIPsPassed 1
  ##################################################################
  # CHECK IPs
  ##################################################################
  set bCheckIPs 1
  if { $bCheckIPs == 1 } {
     set list_check_ips "\ 
  Wuklab.UCSD:hls:arbiter_64:1.0\
  xilinx.com:ip:axis_data_fifo:2.0\
  Wuklab.UCSD:hls:rx_64:1.0\
  Wuklab.UCSD:hls:tx_64:1.0\
  Wuklab.UCSD:hls:retrans_timer:1.0\
  Wuklab.UCSD:hls:setup_manager:1.0\
  Wuklab.UCSD:hls:state_table_64:1.0\
  xilinx.com:ip:axi_datamover:5.1\
  Wuklab.UCSD:hls:unacked_buffer:1.0\
  "

   set list_ips_missing ""
   common::send_msg_id "BD_TCL-006" "INFO" "Checking if the following IPs exist in the project's IP catalog: $list_check_ips ."

   foreach ip_vlnv $list_check_ips {
      set ip_obj [get_ipdefs -all $ip_vlnv]
      if { $ip_obj eq "" } {
         lappend list_ips_missing $ip_vlnv
      }
   }

   if { $list_ips_missing ne "" } {
      catch {common::send_msg_id "BD_TCL-115" "ERROR" "The following IPs are not found in the IP Catalog:\n  $list_ips_missing\n\nResolution: Please add the repository containing the IP(s) to the project." }
      set bCheckIPsPassed 0
   }

  }

  if { $bCheckIPsPassed != 1 } {
    common::send_msg_id "BD_TCL-1003" "WARNING" "Will not continue with creation of design due to the error(s) above."
    return 3
  }

  
# Hierarchical cell: unacked_buffer_controller
proc create_hier_cell_unacked_buffer_controller { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" "create_hier_cell_unacked_buffer_controller() - Empty argument(s)!"}
     return
  }

  # Get object for parentCell
  set parentObj [get_bd_cells $parentCell]
  if { $parentObj == "" } {
     catch {common::send_msg_id "BD_TCL-100" "ERROR" "Unable to find parent cell <$parentCell>!"}
     return
  }

  # Make sure parentObj is hier blk
  set parentType [get_property TYPE $parentObj]
  if { $parentType ne "hier" } {
     catch {common::send_msg_id "BD_TCL-101" "ERROR" "Parent <$parentObj> has TYPE = <$parentType>. Expected to be <hier>."}
     return
  }

  # Save current instance; Restore later
  set oldCurInst [current_bd_instance .]

  # Set parent object as current
  current_bd_instance $parentObj

  # Create cell and set as current instance
  set hier_obj [create_bd_cell -type hier $nameHier]
  current_bd_instance $hier_obj

  # Create interface pins
  create_bd_intf_pin -mode Master -vlnv xilinx.com:interface:aximm_rtl:1.0 M_AXI_0

  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:axis_rtl:1.0 gbn_retrans_req_V

  create_bd_intf_pin -mode Master -vlnv xilinx.com:interface:axis_rtl:1.0 rt_header_V

  create_bd_intf_pin -mode Master -vlnv xilinx.com:interface:axis_rtl:1.0 rt_payload

  create_bd_intf_pin -mode Master -vlnv xilinx.com:interface:axis_rtl:1.0 timer_rst_req_V

  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:axis_rtl:1.0 tx_buff_payload

  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:axis_rtl:1.0 tx_buff_route_info_V


  # Create pins
  create_bd_pin -dir I -type clk ap_clk
  create_bd_pin -dir I -type rst ap_rst_n

  # Create instance: axi_datamover_0, and set properties
  set axi_datamover_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_datamover:5.1 axi_datamover_0 ]
  set_property -dict [ list \
   CONFIG.c_addr_width {40} \
   CONFIG.c_dummy {1} \
   CONFIG.c_m_axi_mm2s_data_width {64} \
   CONFIG.c_m_axi_s2mm_data_width {64} \
   CONFIG.c_m_axis_mm2s_tdata_width {64} \
   CONFIG.c_mm2s_btt_used {23} \
   CONFIG.c_mm2s_burst_size {8} \
   CONFIG.c_s2mm_btt_used {23} \
   CONFIG.c_s2mm_burst_size {8} \
   CONFIG.c_s2mm_support_indet_btt {false} \
   CONFIG.c_s_axis_s2mm_tdata_width {64} \
   CONFIG.c_single_interface {1} \
 ] $axi_datamover_0

  # Create instance: axis_interconnect_0, and set properties
  set axis_interconnect_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_interconnect:2.1 axis_interconnect_0 ]
  set_property -dict [ list \
   CONFIG.NUM_MI {1} \
   CONFIG.NUM_SI {2} \
 ] $axis_interconnect_0

  # Create instance: unacked_buffer_0, and set properties
  set unacked_buffer_0 [ create_bd_cell -type ip -vlnv Wuklab.UCSD:hls:unacked_buffer:1.0 unacked_buffer_0 ]

  # Create interface connections
  connect_bd_intf_net -intf_net Conn1 [get_bd_intf_pins M_AXI_0] [get_bd_intf_pins axi_datamover_0/M_AXI]
  connect_bd_intf_net -intf_net Conn3 [get_bd_intf_pins rt_payload] [get_bd_intf_pins unacked_buffer_0/rt_payload]
  connect_bd_intf_net -intf_net Conn4 [get_bd_intf_pins rt_header_V] [get_bd_intf_pins unacked_buffer_0/rt_header_V]
  connect_bd_intf_net -intf_net axi_datamover_0_M_AXIS_MM2S [get_bd_intf_pins axi_datamover_0/M_AXIS_MM2S] [get_bd_intf_pins unacked_buffer_0/dm_rd_data]
  connect_bd_intf_net -intf_net axis_interconnect_0_M00_AXIS [get_bd_intf_pins axi_datamover_0/S_AXIS_MM2S_CMD] [get_bd_intf_pins axis_interconnect_0/M00_AXIS]
  connect_bd_intf_net -intf_net state_table_gbn_retrans_req_V [get_bd_intf_pins gbn_retrans_req_V] [get_bd_intf_pins unacked_buffer_0/gbn_retrans_req_V]
  connect_bd_intf_net -intf_net tx_64_0_tx_buff_payload [get_bd_intf_pins tx_buff_payload] [get_bd_intf_pins unacked_buffer_0/tx_buff_payload]
  connect_bd_intf_net -intf_net tx_64_0_tx_buff_route_info_V [get_bd_intf_pins tx_buff_route_info_V] [get_bd_intf_pins unacked_buffer_0/tx_buff_route_info_V]
  connect_bd_intf_net -intf_net unacked_buffer_0_dm_rd_cmd_1_V [get_bd_intf_pins axis_interconnect_0/S00_AXIS] [get_bd_intf_pins unacked_buffer_0/dm_rd_cmd_1_V]
  connect_bd_intf_net -intf_net unacked_buffer_0_dm_rd_cmd_2_V [get_bd_intf_pins axis_interconnect_0/S01_AXIS] [get_bd_intf_pins unacked_buffer_0/dm_rd_cmd_2_V]
  connect_bd_intf_net -intf_net unacked_buffer_0_dm_wr_cmd_V [get_bd_intf_pins axi_datamover_0/S_AXIS_S2MM_CMD] [get_bd_intf_pins unacked_buffer_0/dm_wr_cmd_V]
  connect_bd_intf_net -intf_net unacked_buffer_0_dm_wr_data [get_bd_intf_pins axi_datamover_0/S_AXIS_S2MM] [get_bd_intf_pins unacked_buffer_0/dm_wr_data]
  connect_bd_intf_net -intf_net unacked_buffer_0_timer_rst_req_V [get_bd_intf_pins timer_rst_req_V] [get_bd_intf_pins unacked_buffer_0/timer_rst_req_V]

  # Create port connections
  connect_bd_net -net ap_clk_0_1 [get_bd_pins ap_clk] [get_bd_pins axi_datamover_0/m_axi_mm2s_aclk] [get_bd_pins axi_datamover_0/m_axi_s2mm_aclk] [get_bd_pins axi_datamover_0/m_axis_mm2s_cmdsts_aclk] [get_bd_pins axi_datamover_0/m_axis_s2mm_cmdsts_awclk] [get_bd_pins axis_interconnect_0/ACLK] [get_bd_pins axis_interconnect_0/M00_AXIS_ACLK] [get_bd_pins axis_interconnect_0/S00_AXIS_ACLK] [get_bd_pins axis_interconnect_0/S01_AXIS_ACLK] [get_bd_pins unacked_buffer_0/ap_clk]
  connect_bd_net -net ap_rst_n_1 [get_bd_pins ap_rst_n] [get_bd_pins axi_datamover_0/m_axi_mm2s_aresetn] [get_bd_pins axi_datamover_0/m_axi_s2mm_aresetn] [get_bd_pins axi_datamover_0/m_axis_mm2s_cmdsts_aresetn] [get_bd_pins axi_datamover_0/m_axis_s2mm_cmdsts_aresetn] [get_bd_pins axis_interconnect_0/ARESETN] [get_bd_pins axis_interconnect_0/M00_AXIS_ARESETN] [get_bd_pins axis_interconnect_0/S00_AXIS_ARESETN] [get_bd_pins axis_interconnect_0/S01_AXIS_ARESETN] [get_bd_pins unacked_buffer_0/ap_rst_n]

  # Restore current instance
  current_bd_instance $oldCurInst
}
  
# Hierarchical cell: state_table
proc create_hier_cell_state_table { parentCell nameHier } {

  variable script_folder

  if { $parentCell eq "" || $nameHier eq "" } {
     catch {common::send_msg_id "BD_TCL-102" "ERROR" "create_hier_cell_state_table() - Empty argument(s)!"}
     return
  }

  # Get object for parentCell
  set parentObj [get_bd_cells $parentCell]
  if { $parentObj == "" } {
     catch {common::send_msg_id "BD_TCL-100" "ERROR" "Unable to find parent cell <$parentCell>!"}
     return
  }

  # Make sure parentObj is hier blk
  set parentType [get_property TYPE $parentObj]
  if { $parentType ne "hier" } {
     catch {common::send_msg_id "BD_TCL-101" "ERROR" "Parent <$parentObj> has TYPE = <$parentType>. Expected to be <hier>."}
     return
  }

  # Save current instance; Restore later
  set oldCurInst [current_bd_instance .]

  # Set parent object as current
  current_bd_instance $parentObj

  # Create cell and set as current instance
  set hier_obj [create_bd_cell -type hier $nameHier]
  current_bd_instance $hier_obj

  # Create interface pins
  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:axis_rtl:1.0 S02_AXIS

  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:axis_rtl:1.0 check_full_req_V_V

  create_bd_intf_pin -mode Master -vlnv xilinx.com:interface:axis_rtl:1.0 check_full_rsp_V_V

  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:axis_rtl:1.0 conn_set_req

  create_bd_intf_pin -mode Master -vlnv xilinx.com:interface:axis_rtl:1.0 gbn_retrans_req_V

  create_bd_intf_pin -mode Master -vlnv xilinx.com:interface:axis_rtl:1.0 rsp_header_V

  create_bd_intf_pin -mode Master -vlnv xilinx.com:interface:axis_rtl:1.0 rsp_payload

  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:axis_rtl:1.0 state_query_req_V

  create_bd_intf_pin -mode Master -vlnv xilinx.com:interface:axis_rtl:1.0 state_query_rsp_V

  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:axis_rtl:1.0 tx_finish_sig_V


  # Create pins
  create_bd_pin -dir I -type clk ap_clk
  create_bd_pin -dir I -type rst ap_rst_n

  # Create instance: axis_interconnect_0, and set properties
  set axis_interconnect_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_interconnect:2.1 axis_interconnect_0 ]
  set_property -dict [ list \
   CONFIG.ARB_ALGORITHM {0} \
   CONFIG.ENABLE_ADVANCED_OPTIONS {1} \
   CONFIG.NUM_MI {1} \
   CONFIG.NUM_SI {3} \
   CONFIG.ROUTING_MODE {0} \
 ] $axis_interconnect_0

  # Create instance: retrans_timer_0, and set properties
  set retrans_timer_0 [ create_bd_cell -type ip -vlnv Wuklab.UCSD:hls:retrans_timer:1.0 retrans_timer_0 ]

  # Create instance: setup_manager_0, and set properties
  set setup_manager_0 [ create_bd_cell -type ip -vlnv Wuklab.UCSD:hls:setup_manager:1.0 setup_manager_0 ]

  # Create instance: state_table_64_0, and set properties
  set state_table_64_0 [ create_bd_cell -type ip -vlnv Wuklab.UCSD:hls:state_table_64:1.0 state_table_64_0 ]

  # Create interface connections
  connect_bd_intf_net -intf_net Conn1 [get_bd_intf_pins conn_set_req] [get_bd_intf_pins setup_manager_0/conn_set_req_V]
  connect_bd_intf_net -intf_net Conn2 [get_bd_intf_pins S02_AXIS] [get_bd_intf_pins axis_interconnect_0/S02_AXIS]
  connect_bd_intf_net -intf_net Conn3 [get_bd_intf_pins state_query_req_V] [get_bd_intf_pins state_table_64_0/state_query_req_V]
  connect_bd_intf_net -intf_net Conn4 [get_bd_intf_pins gbn_retrans_req_V] [get_bd_intf_pins state_table_64_0/gbn_retrans_req_V]
  connect_bd_intf_net -intf_net axis_interconnect_0_M00_AXIS [get_bd_intf_pins axis_interconnect_0/M00_AXIS] [get_bd_intf_pins retrans_timer_0/timer_rst_req_V]
  connect_bd_intf_net -intf_net retrans_timer_0_rt_timeout_sig_V_V [get_bd_intf_pins retrans_timer_0/rt_timeout_sig_V_V] [get_bd_intf_pins state_table_64_0/rt_timeout_sig_V_V]
  connect_bd_intf_net -intf_net rx_64_0_rsp_header_V [get_bd_intf_pins rsp_header_V] [get_bd_intf_pins state_table_64_0/rsp_header_V]
  connect_bd_intf_net -intf_net rx_64_0_rsp_payload [get_bd_intf_pins rsp_payload] [get_bd_intf_pins state_table_64_0/rsp_payload]
  connect_bd_intf_net -intf_net setup_manager_0_init_req_V_V [get_bd_intf_pins setup_manager_0/init_req_V] [get_bd_intf_pins state_table_64_0/init_req_V]
  connect_bd_intf_net -intf_net setup_manager_0_timer_rst_req_V [get_bd_intf_pins axis_interconnect_0/S01_AXIS] [get_bd_intf_pins setup_manager_0/timer_rst_req_V]
  connect_bd_intf_net -intf_net state_table_64_0_check_full_rsp_V_V [get_bd_intf_pins check_full_rsp_V_V] [get_bd_intf_pins state_table_64_0/check_full_rsp_V_V]
  connect_bd_intf_net -intf_net state_table_64_0_state_query_rsp_V [get_bd_intf_pins state_query_rsp_V] [get_bd_intf_pins state_table_64_0/state_query_rsp_V]
  connect_bd_intf_net -intf_net state_table_64_0_timer_rst_req_V [get_bd_intf_pins axis_interconnect_0/S00_AXIS] [get_bd_intf_pins state_table_64_0/timer_rst_req_V]
  connect_bd_intf_net -intf_net tx_64_0_check_full_req_V_V [get_bd_intf_pins check_full_req_V_V] [get_bd_intf_pins state_table_64_0/check_full_req_V_V]
  connect_bd_intf_net -intf_net tx_64_0_tx_finish_sig_V [get_bd_intf_pins tx_finish_sig_V] [get_bd_intf_pins state_table_64_0/tx_finish_sig_V]

  # Create port connections
  connect_bd_net -net ap_clk_0_1 [get_bd_pins ap_clk] [get_bd_pins axis_interconnect_0/ACLK] [get_bd_pins axis_interconnect_0/M00_AXIS_ACLK] [get_bd_pins axis_interconnect_0/S00_AXIS_ACLK] [get_bd_pins axis_interconnect_0/S01_AXIS_ACLK] [get_bd_pins axis_interconnect_0/S02_AXIS_ACLK] [get_bd_pins retrans_timer_0/ap_clk] [get_bd_pins setup_manager_0/ap_clk] [get_bd_pins state_table_64_0/ap_clk]
  connect_bd_net -net ap_rst_n_1 [get_bd_pins ap_rst_n] [get_bd_pins axis_interconnect_0/ARESETN] [get_bd_pins axis_interconnect_0/M00_AXIS_ARESETN] [get_bd_pins axis_interconnect_0/S00_AXIS_ARESETN] [get_bd_pins axis_interconnect_0/S01_AXIS_ARESETN] [get_bd_pins axis_interconnect_0/S02_AXIS_ARESETN] [get_bd_pins retrans_timer_0/ap_rst_n] [get_bd_pins setup_manager_0/ap_rst_n] [get_bd_pins state_table_64_0/ap_rst_n]

  # Restore current instance
  current_bd_instance $oldCurInst
}
  variable script_folder

  if { $parentCell eq "" } {
     set parentCell [get_bd_cells /]
  }

  # Get object for parentCell
  set parentObj [get_bd_cells $parentCell]
  if { $parentObj == "" } {
     catch {common::send_msg_id "BD_TCL-100" "ERROR" "Unable to find parent cell <$parentCell>!"}
     return
  }

  # Make sure parentObj is hier blk
  set parentType [get_property TYPE $parentObj]
  if { $parentType ne "hier" } {
     catch {common::send_msg_id "BD_TCL-101" "ERROR" "Parent <$parentObj> has TYPE = <$parentType>. Expected to be <hier>."}
     return
  }

  # Save current instance; Restore later
  set oldCurInst [current_bd_instance .]

  # Set parent object as current
  current_bd_instance $parentObj


  # Create interface ports
  set M_AXI [ create_bd_intf_port -mode Master -vlnv xilinx.com:interface:aximm_rtl:1.0 M_AXI ]
  set_property -dict [ list \
   CONFIG.ADDR_WIDTH {40} \
   CONFIG.DATA_WIDTH {64} \
   CONFIG.HAS_BURST {0} \
   CONFIG.HAS_LOCK {0} \
   CONFIG.HAS_QOS {0} \
   CONFIG.HAS_REGION {0} \
   CONFIG.NUM_READ_OUTSTANDING {2} \
   CONFIG.NUM_WRITE_OUTSTANDING {2} \
   CONFIG.PROTOCOL {AXI4} \
   ] $M_AXI

  set conn_set_req [ create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:axis_rtl:1.0 conn_set_req ]
  set_property -dict [ list \
   CONFIG.HAS_TKEEP {0} \
   CONFIG.HAS_TLAST {0} \
   CONFIG.HAS_TREADY {1} \
   CONFIG.HAS_TSTRB {0} \
   CONFIG.LAYERED_METADATA {undef} \
   CONFIG.TDATA_NUM_BYTES {2} \
   CONFIG.TDEST_WIDTH {0} \
   CONFIG.TID_WIDTH {0} \
   CONFIG.TUSER_WIDTH {0} \
   ] $conn_set_req

  set in_header [ create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:axis_rtl:1.0 in_header ]
  set_property -dict [ list \
   CONFIG.HAS_TKEEP {0} \
   CONFIG.HAS_TLAST {0} \
   CONFIG.HAS_TREADY {1} \
   CONFIG.HAS_TSTRB {0} \
   CONFIG.LAYERED_METADATA {undef} \
   CONFIG.TDATA_NUM_BYTES {14} \
   CONFIG.TDEST_WIDTH {0} \
   CONFIG.TID_WIDTH {0} \
   CONFIG.TUSER_WIDTH {0} \
   ] $in_header

  set in_payload [ create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:axis_rtl:1.0 in_payload ]
  set_property -dict [ list \
   CONFIG.HAS_TKEEP {1} \
   CONFIG.HAS_TLAST {1} \
   CONFIG.HAS_TREADY {1} \
   CONFIG.HAS_TSTRB {0} \
   CONFIG.LAYERED_METADATA {undef} \
   CONFIG.TDATA_NUM_BYTES {8} \
   CONFIG.TDEST_WIDTH {0} \
   CONFIG.TID_WIDTH {0} \
   CONFIG.TUSER_WIDTH {1} \
   ] $in_payload

  set out_header [ create_bd_intf_port -mode Master -vlnv xilinx.com:interface:axis_rtl:1.0 out_header ]

  set out_payload [ create_bd_intf_port -mode Master -vlnv xilinx.com:interface:axis_rtl:1.0 out_payload ]

  set usr_rx_header [ create_bd_intf_port -mode Master -vlnv xilinx.com:interface:axis_rtl:1.0 usr_rx_header ]

  set usr_rx_payload [ create_bd_intf_port -mode Master -vlnv xilinx.com:interface:axis_rtl:1.0 usr_rx_payload ]

  set usr_tx_header [ create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:axis_rtl:1.0 usr_tx_header ]
  set_property -dict [ list \
   CONFIG.HAS_TKEEP {0} \
   CONFIG.HAS_TLAST {0} \
   CONFIG.HAS_TREADY {1} \
   CONFIG.HAS_TSTRB {0} \
   CONFIG.LAYERED_METADATA {undef} \
   CONFIG.TDATA_NUM_BYTES {14} \
   CONFIG.TDEST_WIDTH {0} \
   CONFIG.TID_WIDTH {0} \
   CONFIG.TUSER_WIDTH {0} \
   ] $usr_tx_header

  set usr_tx_payload [ create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:axis_rtl:1.0 usr_tx_payload ]
  set_property -dict [ list \
   CONFIG.HAS_TKEEP {1} \
   CONFIG.HAS_TLAST {1} \
   CONFIG.HAS_TREADY {1} \
   CONFIG.HAS_TSTRB {0} \
   CONFIG.LAYERED_METADATA {undef} \
   CONFIG.TDATA_NUM_BYTES {8} \
   CONFIG.TDEST_WIDTH {0} \
   CONFIG.TID_WIDTH {0} \
   CONFIG.TUSER_WIDTH {1} \
   ] $usr_tx_payload


  # Create ports
  set ap_clk [ create_bd_port -dir I -type clk ap_clk ]
  set_property -dict [ list \
   CONFIG.ASSOCIATED_BUSIF {usr_tx_header:usr_tx_payload:usr_rx_header:usr_rx_payload:in_header:in_payload:out_header:out_payload:conn_set_req:M_AXI} \
 ] $ap_clk
  set ap_rst_n [ create_bd_port -dir I -type rst ap_rst_n ]

  # Create instance: arbiter_64_0, and set properties
  set arbiter_64_0 [ create_bd_cell -type ip -vlnv Wuklab.UCSD:hls:arbiter_64:1.0 arbiter_64_0 ]

  # Create instance: axis_data_fifo_0, and set properties
  set axis_data_fifo_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_data_fifo:2.0 axis_data_fifo_0 ]
  set_property -dict [ list \
   CONFIG.FIFO_DEPTH {2048} \
 ] $axis_data_fifo_0

  # Create instance: axis_data_fifo_1, and set properties
  set axis_data_fifo_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_data_fifo:2.0 axis_data_fifo_1 ]
  set_property -dict [ list \
   CONFIG.FIFO_DEPTH {32} \
 ] $axis_data_fifo_1

  # Create instance: rx_64_0, and set properties
  set rx_64_0 [ create_bd_cell -type ip -vlnv Wuklab.UCSD:hls:rx_64:1.0 rx_64_0 ]

  # Create instance: state_table
  create_hier_cell_state_table [current_bd_instance .] state_table

  # Create instance: tx_64_0, and set properties
  set tx_64_0 [ create_bd_cell -type ip -vlnv Wuklab.UCSD:hls:tx_64:1.0 tx_64_0 ]

  # Create instance: unacked_buffer_controller
  create_hier_cell_unacked_buffer_controller [current_bd_instance .] unacked_buffer_controller

  # Create interface connections
  connect_bd_intf_net -intf_net arbiter_64_0_out_header_V [get_bd_intf_ports out_header] [get_bd_intf_pins arbiter_64_0/out_header_V]
  connect_bd_intf_net -intf_net arbiter_64_0_out_payload [get_bd_intf_ports out_payload] [get_bd_intf_pins arbiter_64_0/out_payload]
  connect_bd_intf_net -intf_net axis_data_fifo_1_M_AXIS [get_bd_intf_pins axis_data_fifo_1/M_AXIS] [get_bd_intf_pins unacked_buffer_controller/tx_buff_route_info_V]
  connect_bd_intf_net -intf_net conn_set_req_V_0_1 [get_bd_intf_ports conn_set_req] [get_bd_intf_pins state_table/conn_set_req]
  connect_bd_intf_net -intf_net rx_64_0_rsp_header_V [get_bd_intf_pins arbiter_64_0/rsp_header_V] [get_bd_intf_pins state_table/rsp_header_V]
  connect_bd_intf_net -intf_net rx_64_0_rsp_payload [get_bd_intf_pins arbiter_64_0/rsp_payload] [get_bd_intf_pins state_table/rsp_payload]
  connect_bd_intf_net -intf_net rx_64_0_state_query_req_V [get_bd_intf_pins rx_64_0/state_query_req_V] [get_bd_intf_pins state_table/state_query_req_V]
  connect_bd_intf_net -intf_net rx_64_0_usr_rx_header_V [get_bd_intf_ports usr_rx_header] [get_bd_intf_pins rx_64_0/usr_rx_header_V]
  connect_bd_intf_net -intf_net rx_64_0_usr_rx_payload [get_bd_intf_ports usr_rx_payload] [get_bd_intf_pins rx_64_0/usr_rx_payload]
  connect_bd_intf_net -intf_net rx_header_V_0_1 [get_bd_intf_ports in_header] [get_bd_intf_pins rx_64_0/rx_header_V]
  connect_bd_intf_net -intf_net rx_payload_0_1 [get_bd_intf_ports in_payload] [get_bd_intf_pins rx_64_0/rx_payload]
  connect_bd_intf_net -intf_net state_table_64_0_check_full_rsp_V_V [get_bd_intf_pins state_table/check_full_rsp_V_V] [get_bd_intf_pins tx_64_0/check_full_rsp_V_V]
  connect_bd_intf_net -intf_net state_table_64_0_state_query_rsp_V [get_bd_intf_pins rx_64_0/state_query_rsp_V] [get_bd_intf_pins state_table/state_query_rsp_V]
  connect_bd_intf_net -intf_net state_table_gbn_retrans_req_V [get_bd_intf_pins state_table/gbn_retrans_req_V] [get_bd_intf_pins unacked_buffer_controller/gbn_retrans_req_V]
  connect_bd_intf_net -intf_net tx_64_0_check_full_req_V_V [get_bd_intf_pins state_table/check_full_req_V_V] [get_bd_intf_pins tx_64_0/check_full_req_V_V]
  connect_bd_intf_net -intf_net tx_64_0_tx_buff_payload [get_bd_intf_pins axis_data_fifo_0/S_AXIS] [get_bd_intf_pins tx_64_0/tx_buff_payload]
  connect_bd_intf_net -intf_net tx_64_0_tx_buff_route_info_V [get_bd_intf_pins axis_data_fifo_1/S_AXIS] [get_bd_intf_pins tx_64_0/tx_buff_route_info_V]
  connect_bd_intf_net -intf_net tx_64_0_tx_finish_sig_V [get_bd_intf_pins state_table/tx_finish_sig_V] [get_bd_intf_pins tx_64_0/tx_finish_sig_V]
  connect_bd_intf_net -intf_net tx_64_0_tx_header_V [get_bd_intf_pins arbiter_64_0/tx_header_V] [get_bd_intf_pins tx_64_0/tx_header_V]
  connect_bd_intf_net -intf_net tx_64_0_tx_payload [get_bd_intf_pins arbiter_64_0/tx_payload] [get_bd_intf_pins tx_64_0/tx_payload]
  connect_bd_intf_net -intf_net tx_buff_payload_1 [get_bd_intf_pins axis_data_fifo_0/M_AXIS] [get_bd_intf_pins unacked_buffer_controller/tx_buff_payload]
  connect_bd_intf_net -intf_net unacked_buffer_0_timer_rst_req_V [get_bd_intf_pins state_table/S02_AXIS] [get_bd_intf_pins unacked_buffer_controller/timer_rst_req_V]
  connect_bd_intf_net -intf_net unacked_buffer_controller_M_AXI_0 [get_bd_intf_ports M_AXI] [get_bd_intf_pins unacked_buffer_controller/M_AXI_0]
  connect_bd_intf_net -intf_net unacked_buffer_controller_rt_header_V [get_bd_intf_pins arbiter_64_0/rt_header_V] [get_bd_intf_pins unacked_buffer_controller/rt_header_V]
  connect_bd_intf_net -intf_net unacked_buffer_controller_rt_payload [get_bd_intf_pins arbiter_64_0/rt_payload] [get_bd_intf_pins unacked_buffer_controller/rt_payload]
  connect_bd_intf_net -intf_net usr_tx_header_V_0_1 [get_bd_intf_ports usr_tx_header] [get_bd_intf_pins tx_64_0/usr_tx_header_V]
  connect_bd_intf_net -intf_net usr_tx_payload_0_1 [get_bd_intf_ports usr_tx_payload] [get_bd_intf_pins tx_64_0/usr_tx_payload]

  # Create port connections
  connect_bd_net -net ap_clk_0_1 [get_bd_ports ap_clk] [get_bd_pins arbiter_64_0/ap_clk] [get_bd_pins axis_data_fifo_0/s_axis_aclk] [get_bd_pins axis_data_fifo_1/s_axis_aclk] [get_bd_pins rx_64_0/ap_clk] [get_bd_pins state_table/ap_clk] [get_bd_pins tx_64_0/ap_clk] [get_bd_pins unacked_buffer_controller/ap_clk]
  connect_bd_net -net ap_rst_n_1 [get_bd_ports ap_rst_n] [get_bd_pins arbiter_64_0/ap_rst_n] [get_bd_pins axis_data_fifo_0/s_axis_aresetn] [get_bd_pins axis_data_fifo_1/s_axis_aresetn] [get_bd_pins rx_64_0/ap_rst_n] [get_bd_pins state_table/ap_rst_n] [get_bd_pins tx_64_0/ap_rst_n] [get_bd_pins unacked_buffer_controller/ap_rst_n]

  # Create address segments
  create_bd_addr_seg -range 0x010000000000 -offset 0x00000000 [get_bd_addr_spaces unacked_buffer_controller/axi_datamover_0/Data] [get_bd_addr_segs M_AXI/Reg] SEG_M_AXI_Reg

  # Restore current instance
  current_bd_instance $oldCurInst

  validate_bd_design
  save_bd_design
  close_bd_design $design_name 
}
# End of cr_bd_relnet()
cr_bd_relnet ""
set_property REGISTERED_WITH_MANAGER "1" [get_files relnet.bd ] 
set_property SYNTH_CHECKPOINT_MODE "Hierarchical" [get_files relnet.bd ] 



# Proc to create BD ex_sim
proc cr_bd_ex_sim { parentCell } {

  # CHANGE DESIGN NAME HERE
  set design_name ex_sim

  common::send_msg_id "BD_TCL-003" "INFO" "Currently there is no design <$design_name> in project, so creating one..."

  create_bd_design $design_name

  set bCheckIPsPassed 1
  ##################################################################
  # CHECK IPs
  ##################################################################
  set bCheckIPs 1
  if { $bCheckIPs == 1 } {
     set list_check_ips "\ 
  xilinx.com:ip:axi4stream_vip:1.1\
  "

   set list_ips_missing ""
   common::send_msg_id "BD_TCL-006" "INFO" "Checking if the following IPs exist in the project's IP catalog: $list_check_ips ."

   foreach ip_vlnv $list_check_ips {
      set ip_obj [get_ipdefs -all $ip_vlnv]
      if { $ip_obj eq "" } {
         lappend list_ips_missing $ip_vlnv
      }
   }

   if { $list_ips_missing ne "" } {
      catch {common::send_msg_id "BD_TCL-115" "ERROR" "The following IPs are not found in the IP Catalog:\n  $list_ips_missing\n\nResolution: Please add the repository containing the IP(s) to the project." }
      set bCheckIPsPassed 0
   }

  }

  if { $bCheckIPsPassed != 1 } {
    common::send_msg_id "BD_TCL-1003" "WARNING" "Will not continue with creation of design due to the error(s) above."
    return 3
  }

  variable script_folder

  if { $parentCell eq "" } {
     set parentCell [get_bd_cells /]
  }

  # Get object for parentCell
  set parentObj [get_bd_cells $parentCell]
  if { $parentObj == "" } {
     catch {common::send_msg_id "BD_TCL-100" "ERROR" "Unable to find parent cell <$parentCell>!"}
     return
  }

  # Make sure parentObj is hier blk
  set parentType [get_property TYPE $parentObj]
  if { $parentType ne "hier" } {
     catch {common::send_msg_id "BD_TCL-101" "ERROR" "Parent <$parentObj> has TYPE = <$parentType>. Expected to be <hier>."}
     return
  }

  # Save current instance; Restore later
  set oldCurInst [current_bd_instance .]

  # Set parent object as current
  current_bd_instance $parentObj


  # Create interface ports
  set M_AXIS_hdr [ create_bd_intf_port -mode Master -vlnv xilinx.com:interface:axis_rtl:1.0 M_AXIS_hdr ]

  set M_AXIS_payload [ create_bd_intf_port -mode Master -vlnv xilinx.com:interface:axis_rtl:1.0 M_AXIS_payload ]

  set M_AXIS_setconn [ create_bd_intf_port -mode Master -vlnv xilinx.com:interface:axis_rtl:1.0 M_AXIS_setconn ]


  # Create ports
  set aclk [ create_bd_port -dir I -type clk aclk ]
  set aresetn [ create_bd_port -dir I -type rst aresetn ]

  # Create instance: axi4stream_vip_tx_hdr, and set properties
  set axi4stream_vip_tx_hdr [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi4stream_vip:1.1 axi4stream_vip_tx_hdr ]
  set_property -dict [ list \
   CONFIG.HAS_TKEEP {0} \
   CONFIG.HAS_TLAST {0} \
   CONFIG.HAS_TREADY {1} \
   CONFIG.HAS_TSTRB {0} \
   CONFIG.INTERFACE_MODE {MASTER} \
   CONFIG.TDATA_NUM_BYTES {14} \
   CONFIG.TDEST_WIDTH {0} \
   CONFIG.TID_WIDTH {0} \
   CONFIG.TUSER_WIDTH {0} \
 ] $axi4stream_vip_tx_hdr

  # Create instance: axi4stream_vip_tx_payload, and set properties
  set axi4stream_vip_tx_payload [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi4stream_vip:1.1 axi4stream_vip_tx_payload ]
  set_property -dict [ list \
   CONFIG.HAS_TKEEP {1} \
   CONFIG.HAS_TLAST {1} \
   CONFIG.HAS_TREADY {1} \
   CONFIG.HAS_TSTRB {0} \
   CONFIG.INTERFACE_MODE {MASTER} \
   CONFIG.TDATA_NUM_BYTES {8} \
   CONFIG.TDEST_WIDTH {0} \
   CONFIG.TID_WIDTH {0} \
   CONFIG.TUSER_WIDTH {1} \
 ] $axi4stream_vip_tx_payload

  # Create instance: axi4stream_vip_tx_setconn, and set properties
  set axi4stream_vip_tx_setconn [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi4stream_vip:1.1 axi4stream_vip_tx_setconn ]
  set_property -dict [ list \
   CONFIG.HAS_TKEEP {0} \
   CONFIG.HAS_TLAST {0} \
   CONFIG.HAS_TREADY {1} \
   CONFIG.HAS_TSTRB {0} \
   CONFIG.INTERFACE_MODE {MASTER} \
   CONFIG.TDATA_NUM_BYTES {2} \
   CONFIG.TDEST_WIDTH {0} \
   CONFIG.TID_WIDTH {0} \
   CONFIG.TUSER_WIDTH {0} \
 ] $axi4stream_vip_tx_setconn

  # Create interface connections
  connect_bd_intf_net -intf_net axi4stream_vip_tx_hdr_M_AXIS [get_bd_intf_ports M_AXIS_hdr] [get_bd_intf_pins axi4stream_vip_tx_hdr/M_AXIS]
  connect_bd_intf_net -intf_net axi4stream_vip_tx_payload_M_AXIS [get_bd_intf_ports M_AXIS_payload] [get_bd_intf_pins axi4stream_vip_tx_payload/M_AXIS]
  connect_bd_intf_net -intf_net axi4stream_vip_tx_setconn_M_AXIS [get_bd_intf_ports M_AXIS_setconn] [get_bd_intf_pins axi4stream_vip_tx_setconn/M_AXIS]

  # Create port connections
  connect_bd_net -net aclk_0_1 [get_bd_ports aclk] [get_bd_pins axi4stream_vip_tx_hdr/aclk] [get_bd_pins axi4stream_vip_tx_payload/aclk] [get_bd_pins axi4stream_vip_tx_setconn/aclk]
  connect_bd_net -net aresetn_0_1 [get_bd_ports aresetn] [get_bd_pins axi4stream_vip_tx_hdr/aresetn] [get_bd_pins axi4stream_vip_tx_payload/aresetn] [get_bd_pins axi4stream_vip_tx_setconn/aresetn]

  # Create address segments


  # Restore current instance
  current_bd_instance $oldCurInst

  validate_bd_design
  save_bd_design
  close_bd_design $design_name 
}
# End of cr_bd_ex_sim()
cr_bd_ex_sim ""
set_property REGISTERED_WITH_MANAGER "1" [get_files ex_sim.bd ] 
set_property SYNTH_CHECKPOINT_MODE "Hierarchical" [get_files ex_sim.bd ] 

#
# Expose our relnet BD
#
ipx::package_project -root_dir ../generated_ip/net_top_relnet_zcu106 -vendor wuklab -library user -taxonomy UserIP -module relnet -import_files
exit
