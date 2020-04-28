# XDC constraints for the ZCU106
# part: xcku3p-ffvb676-2-e

# General configuration, comment out unsupported methods
# set_property CFGBVS GND                                [current_design]
# set_property CONFIG_VOLTAGE 1.8                        [current_design]
set_property BITSTREAM.GENERAL.COMPRESS true           [current_design]
# set_property BITSTREAM.CONFIG.EXTMASTERCCLK_EN {DIV-1} [current_design]
# set_property BITSTREAM.CONFIG.SPI_32BIT_ADDR YES       [current_design]
# set_property BITSTREAM.CONFIG.SPI_BUSWIDTH 8           [current_design]
# set_property BITSTREAM.CONFIG.SPI_FALL_EDGE YES        [current_design]

# 125 MHz System clock
set_property -dict {LOC H9 IOSTANDARD  LVDS} [get_ports clk_125mhz_p] ;# Bank  68 VCCO - VADJ_FMC - IO_L11P_T1U_N8_GC_68
set_property -dict {LOC G9 IOSTANDARD  LVDS} [get_ports clk_125mhz_n] ;# Bank  68 VCCO - VADJ_FMC - IO_L11N_T1U_N9_GC_68
create_clock -period 8.000 -name clk_125mhz [get_ports clk_125mhz_p]

# System reset
set_property -dict {LOC G13 IOSTANDARD LVCMOS18} [get_ports reset] ;# Bank  68 VCCO - VADJ_FMC - IO_T2U_N12_68

# LEDs
# Top of the board
# |  O   O   O   O   O   O   O   O   |
#   sma sma  block_lock     block_lock|
# | LED7         <---           LED0 | 

set_property -dict {LOC AL11 IOSTANDARD LVCMOS12 SLEW SLOW} [get_ports {led[0]}]; # GPIO1
set_property -dict {LOC AL13 IOSTANDARD LVCMOS12 SLEW SLOW} [get_ports {led[1]}]; # GPIO2
set_property -dict {LOC AK13 IOSTANDARD LVCMOS12 SLEW SLOW} [get_ports {led[2]}]; # GPIO3
set_property -dict {LOC AE15 IOSTANDARD LVCMOS12 SLEW SLOW} [get_ports {led[3]}]; # GPIO4
set_property -dict {LOC AM8  IOSTANDARD LVCMOS12 SLEW SLOW} [get_ports {led[4]}];  # GPIO4
set_property -dict {LOC AM9  IOSTANDARD LVCMOS12 SLEW SLOW} [get_ports {led[5]}];  # GPIO5
set_property -dict {LOC AM10 IOSTANDARD LVCMOS12 SLEW SLOW} [get_ports {led[6]}];   # GPIO6
set_property -dict {LOC AM11 IOSTANDARD LVCMOS12 SLEW SLOW} [get_ports {led[7]}];   # GPIO7


# 156.25 MHz reference clock from SI750
set_property -dict {LOC U10 } [get_ports sfp_mgt_refclk_p] ;# MGTREFCLK0P_227 from X2
set_property -dict {LOC U9  } [get_ports sfp_mgt_refclk_n] ;# MGTREFCLK0P_227 from X2
create_clock -period 6.400 -name sfp_mgt_refclk [get_ports sfp_mgt_refclk_p]

# SFP28 Interfaces
set_property -dict {LOC AA2 } [get_ports sfp_1_rx_p] ;# Bank 225
set_property -dict {LOC AA1 } [get_ports sfp_1_rx_n] ;# Bank 225

set_property -dict {LOC W2  } [get_ports sfp_2_rx_p] ;# Bank 225
set_property -dict {LOC W1  } [get_ports sfp_2_rx_n] ;# Bank 225

set_property -dict {LOC Y4  } [get_ports sfp_1_tx_p] ;# Bank 225
set_property -dict {LOC Y3  } [get_ports sfp_1_tx_n] ;# Bank 225

set_property -dict {LOC W6  } [get_ports sfp_2_tx_p] ;# Bank 225
set_property -dict {LOC W5  } [get_ports sfp_2_tx_n] ;# Bank 225

set_property -dict {LOC AE22 IOSTANDARD LVCMOS12 SLEW SLOW} [get_ports sfp_1_tx_disable]
set_property -dict {LOC AF20 IOSTANDARD LVCMOS12 SLEW SLOW} [get_ports sfp_2_tx_disable]


# I2C interface
#set_property -dict {LOC B9 IOSTANDARD LVCMOS33 SLEW SLOW DRIVE 12 PULLUP true} [get_ports eeprom_i2c_scl]
#set_property -dict {LOC A9 IOSTANDARD LVCMOS33 SLEW SLOW DRIVE 12 PULLUP true} [get_ports eeprom_i2c_sda]
