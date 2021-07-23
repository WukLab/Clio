set IP_FOLDER generated_ip
set PROJECT_NAME package_[lindex $argv 0]
set PROJECT_FOLDER ${IP_FOLDER}/$PROJECT_NAME

# Script for packaging IP from RTL sources
create_project ${PROJECT_NAME} ${PROJECT_FOLDER} -part xczu7ev-ffvc1156-2-e -force

foreach f [lassign $argv _] {
  add_files -norecurse $f
}

import_files -force -norecurse
update_compile_order -fileset sources_1
update_compile_order -fileset sim_1

ipx::package_project  -root_dir ${PROJECT_FOLDER}
set_property library {user} [ipx::current_core]
set_property vendor_display_name {Wuklab} [ipx::current_core]
set_property company_url {http://www.wuklab.io} [ipx::current_core]
set_property vendor {wuklab.io} [ipx::current_core]
set_property taxonomy {{/LegoMem}} [ipx::current_core]
ipx::check_integrity [ipx::current_core]
close_project
exit
