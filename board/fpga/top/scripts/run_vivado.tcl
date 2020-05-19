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
set_property "ip_repo_paths" "
	[file normalize "$origin_dir/../generated_ip"]
	[file normalize "/data/ip/legomem"]
	" $obj
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

source scripts/legomem_system_release.tcl

exit
