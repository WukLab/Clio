set IP_FOLDER generated_ip
set PROJECT_FOLDER ${IP_FOLDER}/package_core_mem

set SOURCE {
    LegoMemSystemLib.v
    generated_rtl/LegoMemSystem.v
}

create_project package_core_mem ${PROJECT_FOLDER} -part xczu7ev-ffvc1156-2-e -force

add_files -norecurse ${SOURCE}
import_files -force -norecurse

update_compile_order -fileset sources_1
update_compile_order -fileset sim_1

ipx::package_project -root_dir ${IP_FOLDER}
set_property library {user} [ipx::current_core]
set_property vendor_display_name {Wuklab} [ipx::current_core]
set_property company_url {http://www.wuklab.io} [ipx::current_core]
set_property vendor {wuklab.io} [ipx::current_core]
set_property taxonomy {{/LegoMem}} [ipx::current_core]
ipx::check_integrity [ipx::current_core]

close_project
exit
