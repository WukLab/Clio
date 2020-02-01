

#PROJECT_PARAM.ARCHITECTURE        
#PROJECT_PARAM.DEVICE               
#PROJECT_PARAM.PACKAGE      
#PROJECT_PARAM.SPEED_GRADE  
#PROJECT_PARAM.TEMPERATURE_GRADE     
#PROJECT_PARAM.SILICON_REVISION      
#PROJECT_PARAM.FAMILY                
#PROJECT_PARAM.PART                  
#PROJECT_PARAM.IO_STANDARDS          
#PROJECT_PARAM.FAMILY_ISE            
#PROJECT_PARAM.PART_ISE              
#PROJECT_PARAM.PREFHDL               
#PROJECT_PARAM.USE_RDI_CUSTOMIZATION 
#PROJECT_PARAM.USE_RDI_GENERATION    
#PROJECT_PARAM.BOARD                 
#PROJECT_PARAM.SIMLANGUAGE
##


#expr {[expr {[get_nets W_Clk]=={}}]? [set w_clk_net [get_nets R_Clk]]: [set w_clk_net [get_nets W_Clk]]}
#expr {[expr {[get_nets R_Clk]=={}}]? [set r_clk_net [get_nets W_Clk]]: [set r_clk_net [get_nets R_Clk]]}
#
#set w_clk_obj [get_clocks -of_objects $w_clk_net]
#set r_clk_obj [get_clocks -of_objects $r_clk_net]
#
#set w_clk_period [get_property PERIOD $w_clk_obj]
#set r_clk_period [get_property PERIOD $r_clk_obj]
#
##If clock are the same make the set_max_delay command fail
#
#expr {[expr {$w_clk_obj==$r_clk_obj}]? "" : [set_max_delay -datapath_only [expr {$w_clk_period-0.1}] -from $w_clk_obj -to $r_clk_obj]}
#expr {[expr {$w_clk_obj==$r_clk_obj}]? "" : [set_max_delay -datapath_only [expr {$r_clk_period-0.1}] -to $w_clk_obj -from $r_clk_obj]}


