###############################################################################################################
# Core-Level Timing Constraints for axi_clock_converter Component "design_1_auto_cc_5"
###############################################################################################################
#
# This component is configured to perform asynchronous clock-domain-crossing.
# Constraints will be handled by XPM_CDC.
create_waiver -internal -scope -type CDC -id CDC-10 -user axi_clock_converter -tags "1024161" -to [get_pins -quiet *gen_clock_conv.gen_async_conv.asyncfifo_axi/inst_fifo_gen/gaxi_full_lite.g*_ch.g*ch2.axi_*/grf.rf/rstblk/ngwrdrst.grst.g7serrst.gnsckt_*_reg2_inst/arststages_ff_reg[0]/PRE]\
-description {Waiving CDC-10 Although there is combo logic going into FIFO Gen reset, the expectation/rule is that the reset signal will be held for 1 clk cycles on the slowest clock.  Hence there should not be any issues cause by this logic}

create_waiver -internal -scope -type CDC -id CDC-11 -user axi_clock_converter -tags "1024161" -to [get_pins -quiet *gen_clock_conv.gen_async_conv.asyncfifo_axi/inst_fifo_gen/gaxi_full_lite.g*_ch.g*ch2.axi_*/grf.rf/rstblk/ngwrdrst.grst.g7serrst.gnsckt_*_reg2_inst/arststages_ff_reg[0]/PRE]\
-description {Waiving CDC-11 Although there is combo logic going into FIFO Gen reset, the expectation/rule is that the reset signal will be held for 1 clk cycles on the slowest clock.  Hence there should not be any issues cause by this logic}

create_waiver -internal -scope -type CDC -id CDC-15 -user axi_clock_converter -tags "1024442" -from [get_pins -quiet *gen_clock_conv.gen_async_conv.asyncfifo_axi/inst_fifo_gen/gaxi_full_lite.g*_ch.g*ch2.axi_*/grf.rf/gntv_or_sync_fifo.mem/gdm.dm_gen.dm/RAM_reg_0_15_*/RAM*/CLK]\ 
-to [get_pins -quiet *gen_clock_conv.gen_async_conv.asyncfifo_axi/inst_fifo_gen/gaxi_full_lite.g*_ch.g*ch2.axi_*/grf.rf/gntv_or_sync_fifo.mem/gdm.dm_gen.dm/gpr1.dout_i_reg*/D]\
-description {Waiving CDC-15 Timing constraints are processed during implementation, not synthesis. The xdc is marked only to be used during implementation, as advised by the XDC folks at the time.}
