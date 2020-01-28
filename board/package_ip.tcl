create_project package_axi_lite_slave package_axi_lite_slave -part xczu7ev-ffvc1156-2-e -force
add_files -norecurse hdl/top_level.v
import_files -force -norecurse
update_compile_order -fileset sources_1
update_compile_order -fileset sim_1
ipx::package_project
set_property library {user} [ipx::current_core]
set_property vendor_display_name {Wuklab} [ipx::current_core]
set_property company_url {http://wuklab.io} [ipx::current_core]
set_property vendor {wuklab.io} [ipx::current_core]
set_property taxonomy {{/LegoMem}} [ipx::current_core]
ipx::check_integrity [ipx::current_core]
close_project