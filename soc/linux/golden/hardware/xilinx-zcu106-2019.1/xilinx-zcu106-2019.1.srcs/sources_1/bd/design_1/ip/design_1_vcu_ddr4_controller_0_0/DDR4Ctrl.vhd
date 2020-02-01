
------------------------------------------------------------------------
-- C O M P O N E N T   P A C K A G E
------------------------------------------------------------------------
library work;
library ieee;
use ieee.std_logic_1164.all;
use work.SpdDummyComponent.all;
use work.ToolsPck.all;
use work.BurstGenWrComponent.all;
use work.BurstGenRdComponent.all;
use work.SdramPhyUltrascale_0Component_1.all;
use work.AxiWrPortComponent.all;
use work.SdramCtrlReorderComponent.all;
use work.AxiRdPortComponent.all;
use ieee.std_logic_arith.all;

package DDR4CtrlComponent_1 is
   component DDR4Ctrl_1
      generic(
         g_TestBackpressure   : natural := 0
      );
      port(
		 -- 128B AXI WRITE 0 PORT ------------------------------------------------
		 -- CLOCK AND RESET--------------------------------------------------------
         Wr0sRst              : in  std_ulogic;
         Wr0Clk               : in  std_ulogic;
         -- WRITE ADDRESS CHANNEL---------------------------------------------
         Wr0AWID              : in  std_logic_vector(15 downto 0);
         Wr0AWADDR            : in  std_logic_vector(31 downto 0);
         Wr0AWLEN             : in  std_logic_vector(7 downto 0);
         Wr0AWSIZE            : in  std_logic_vector(2 downto 0);
         Wr0AWBURST           : in  std_logic_vector(1 downto 0);
         Wr0AWVALID           : in  std_ulogic;
         Wr0AWREADY           : out std_ulogic;
         -- WRITE DATA CHANNEL----------------------------------------------
         Wr0WID               : in  std_logic_vector(15 downto 0);
         Wr0WDATA             : in  std_logic_vector(127 downto 0);
         Wr0WSTRB             : in  std_logic_vector(15 downto 0);
         Wr0WLAST             : in  std_ulogic;
         Wr0WVALID            : in  std_ulogic;
         Wr0WREADY            : out std_ulogic;
         -- WRITE RESPONSE CHANNEL--------------------------------------------------
         Wr0BID               : out std_logic_vector(15 downto 0);
         Wr0BRESP             : out std_logic_vector(1 downto 0);
         Wr0BVALID            : out std_ulogic;
         Wr0BREADY            : in  std_ulogic;
		 -- 128B AXI WRITE 1 PORT ------------------------------------------------
		 -- CLOCK AND RESET-----------------------------------------------
         Wr1sRst              : in  std_ulogic;
         Wr1Clk               : in  std_ulogic;
         -- WRITE ADDRESS CHANNEL----------------------------------------------
         Wr1AWID              : in  std_logic_vector(15 downto 0);
         Wr1AWADDR            : in  std_logic_vector(31 downto 0);
         Wr1AWLEN             : in  std_logic_vector(7 downto 0);
         Wr1AWSIZE            : in  std_logic_vector(2 downto 0);
         Wr1AWBURST           : in  std_logic_vector(1 downto 0);
         Wr1AWVALID           : in  std_ulogic;
         Wr1AWREADY           : out std_ulogic;
         -- WRITE DATA CHANNEL----------------------------------------------------
         Wr1WID               : in  std_logic_vector(15 downto 0);
         Wr1WDATA             : in  std_logic_vector(127 downto 0);
         Wr1WSTRB             : in  std_logic_vector(15 downto 0);
         Wr1WLAST             : in  std_ulogic;
         Wr1WVALID            : in  std_ulogic;
         Wr1WREADY            : out std_ulogic;
         -- WRITE RESPONSE CHANNEL---------------------------------------------
         Wr1BID               : out std_logic_vector(15 downto 0);
         Wr1BRESP             : out std_logic_vector(1 downto 0);
         Wr1BVALID            : out std_ulogic;
         Wr1BREADY            : in  std_ulogic;
		 
         --128B AXI WRITE 4 PORT ------------------------------------------------
		 --CLOCK AND RESET-----------------------------------------
         Wr4sRst              : in  std_ulogic;
         Wr4Clk               : in  std_ulogic;
         -- WRITE ADDRESS CHANNEL---------------------------------------
         Wr4AWID              : in  std_logic_vector(15 downto 0);
         Wr4AWADDR            : in  std_logic_vector(31 downto 0);
         Wr4AWLEN             : in  std_logic_vector(7 downto 0);
         Wr4AWSIZE            : in  std_logic_vector(2 downto 0);
         Wr4AWBURST           : in  std_logic_vector(1 downto 0);
         Wr4AWVALID           : in  std_ulogic;
         Wr4AWREADY           : out std_ulogic;
         -- WRITE DATA CHANNEL-----------------------------------------------
         Wr4WID               : in  std_logic_vector(15 downto 0);
         Wr4WDATA             : in  std_logic_vector(127 downto 0);
         Wr4WSTRB             : in  std_logic_vector(15 downto 0);
         Wr4WLAST             : in  std_ulogic;
         Wr4WVALID            : in  std_ulogic;
         Wr4WREADY            : out std_ulogic;
         -- WRITE RESPONSE CHANNEL--------------------------------------------
         Wr4BID               : out std_logic_vector(15 downto 0);
         Wr4BRESP             : out std_logic_vector(1 downto 0);
         Wr4BVALID            : out std_ulogic;
         Wr4BREADY            : in  std_ulogic;
		 -- 128B AXI READ 0 PORT -------------------------------------------------
         
		 -- CLOCK AND RESET-----------------------------------------------------
         Rd0sRst              : in  std_ulogic;
         Rd0Clk               : in  std_ulogic;
         -- READ ADDRESS CHANNEL----------------------------------------------
         Rd0ARID              : in  std_logic_vector(15 downto 0);
         Rd0ARADDR            : in  std_logic_vector(31 downto 0);
         Rd0ARLEN             : in  std_logic_vector(7 downto 0);
         Rd0ARSIZE            : in  std_logic_vector(2 downto 0);
         Rd0ARBURST           : in  std_logic_vector(1 downto 0);
         Rd0ARVALID           : in  std_ulogic;
         Rd0ARREADY           : out std_ulogic;
         -- READ DATA CHANNEL---------------------------------------------------
         Rd0RID               : out std_logic_vector(15 downto 0);
         Rd0RDATA             : out std_logic_vector(127 downto 0);
         Rd0RRESP             : out std_logic_vector(1 downto 0);
         Rd0RLAST             : out std_ulogic;
         Rd0RVALID            : out std_ulogic;
         Rd0RREADY            : in  std_ulogic;
		 -- 128B AXI READ 1 PORT -------------------------------------------------
		 -- CLOCK AND RESET----------------------------------------------------
         Rd1sRst              : in  std_ulogic;
         Rd1Clk               : in  std_ulogic;
         -- READ ADDRESS CHANNEL-----------------------------------------------
         Rd1ARID              : in  std_logic_vector(15 downto 0);
         Rd1ARADDR            : in  std_logic_vector(31 downto 0);
         Rd1ARLEN             : in  std_logic_vector(7 downto 0);
         Rd1ARSIZE            : in  std_logic_vector(2 downto 0);
         Rd1ARBURST           : in  std_logic_vector(1 downto 0);
         Rd1ARVALID           : in  std_ulogic;
         Rd1ARREADY           : out std_ulogic;
         -- READ DATA CHANNEL-------------------------------------------------
         Rd1RID               : out std_logic_vector(15 downto 0);
         Rd1RDATA             : out std_logic_vector(127 downto 0);
         Rd1RRESP             : out std_logic_vector(1 downto 0);
         Rd1RLAST             : out std_ulogic;
         Rd1RVALID            : out std_ulogic;
         Rd1RREADY            : in  std_ulogic;
		 --128B AXI READ 4 PORT -------------------------------------------------
		 --CLOCK AND RESET--------------------------------------
         Rd4sRst              : in  std_ulogic;
         Rd4Clk               : in  std_ulogic;
         -- READ ADDRESS CHANNEL--------------------------------
         Rd4ARID              : in  std_logic_vector(15 downto 0);
         Rd4ARADDR            : in  std_logic_vector(31 downto 0);
         Rd4ARLEN             : in  std_logic_vector(7 downto 0);
         Rd4ARSIZE            : in  std_logic_vector(2 downto 0);
         Rd4ARBURST           : in  std_logic_vector(1 downto 0);
         Rd4ARVALID           : in  std_ulogic;
         Rd4ARREADY           : out std_ulogic;
         -- READ DATA CHANNEL-------------------------------------------
         Rd4RID               : out std_logic_vector(15 downto 0);
         Rd4RDATA             : out std_logic_vector(127 downto 0);
         Rd4RRESP             : out std_logic_vector(1 downto 0);
         Rd4RLAST             : out std_ulogic;
         Rd4RVALID            : out std_ulogic;
         Rd4RREADY            : in  std_ulogic;
		 -- 128B AXI WRITE 2 PORT ------------------------------------------------
		 -- CLOCK AND RESET------------------------------------------------
         Wr2sRst              : in  std_ulogic;
         Wr2Clk               : in  std_ulogic;
         -- WRITE ADDRESS CHANNEL-------------------------------------------
         Wr2AWID              : in  std_logic_vector(15 downto 0);
         Wr2AWADDR            : in  std_logic_vector(31 downto 0);
         Wr2AWLEN             : in  std_logic_vector(7 downto 0);
         Wr2AWSIZE            : in  std_logic_vector(2 downto 0);
         Wr2AWBURST           : in  std_logic_vector(1 downto 0);
         Wr2AWVALID           : in  std_ulogic;
         Wr2AWREADY           : out std_ulogic;
         -- WRITE DATA CHANNEL-----------------------------------------------
         Wr2WID               : in  std_logic_vector(15 downto 0);
         Wr2WDATA             : in  std_logic_vector(127 downto 0);
         Wr2WSTRB             : in  std_logic_vector(15 downto 0);
         Wr2WLAST             : in  std_ulogic;
         Wr2WVALID            : in  std_ulogic;
         Wr2WREADY            : out std_ulogic;
         -- WRITE RESPONSE CHANNEL----------------------------------------------------
         Wr2BID               : out std_logic_vector(15 downto 0);
         Wr2BRESP             : out std_logic_vector(1 downto 0);
         Wr2BVALID            : out std_ulogic;
         Wr2BREADY            : in  std_ulogic;
		 --128B AXI WRITE 3 PORT ------------------------------------------------
		 -- CLOCK AND RESET-------------------------------------------------------
         Wr3sRst              : in  std_ulogic;
         Wr3Clk               : in  std_ulogic;
         -- WRITE ADDRESS CHANNEL----------------------------------------------
         Wr3AWID              : in  std_logic_vector(15 downto 0);
         Wr3AWADDR            : in  std_logic_vector(31 downto 0);
         Wr3AWLEN             : in  std_logic_vector(7 downto 0);
         Wr3AWSIZE            : in  std_logic_vector(2 downto 0);
         Wr3AWBURST           : in  std_logic_vector(1 downto 0);
         Wr3AWVALID           : in  std_ulogic;
         Wr3AWREADY           : out std_ulogic;
         -- WRITE DATA CHANNEL---------------------------------------------
         Wr3WID               : in  std_logic_vector(15 downto 0);
         Wr3WDATA             : in  std_logic_vector(127 downto 0);
         Wr3WSTRB             : in  std_logic_vector(15 downto 0);
         Wr3WLAST             : in  std_ulogic;
         Wr3WVALID            : in  std_ulogic;
         Wr3WREADY            : out std_ulogic;
         -- WRITE RESPONSE CHANNEL-------------------------------------
         Wr3BID               : out std_logic_vector(15 downto 0);
         Wr3BRESP             : out std_logic_vector(1 downto 0);
         Wr3BVALID            : out std_ulogic;
         Wr3BREADY            : in  std_ulogic;

		 -- 128B AXI READ 2 PORT -------------------------------------------------
		 -- CLOCK AND RESET------------------------------------------------
         Rd2sRst              : in  std_ulogic;
         Rd2Clk               : in  std_ulogic;
         -- READ ADDRESS CHANNEL--------------------------------------
         Rd2ARID              : in  std_logic_vector(15 downto 0);
         Rd2ARADDR            : in  std_logic_vector(31 downto 0);
         Rd2ARLEN             : in  std_logic_vector(7 downto 0);
         Rd2ARSIZE            : in  std_logic_vector(2 downto 0);
         Rd2ARBURST           : in  std_logic_vector(1 downto 0);
         Rd2ARVALID           : in  std_ulogic;
         Rd2ARREADY           : out std_ulogic;
         -- READ DATA CHANNEL---------------------------------------------
         Rd2RID               : out std_logic_vector(15 downto 0);
         Rd2RDATA             : out std_logic_vector(127 downto 0);
         Rd2RRESP             : out std_logic_vector(1 downto 0);
         Rd2RLAST             : out std_ulogic;
         Rd2RVALID            : out std_ulogic;
         Rd2RREADY            : in  std_ulogic;
		 --128B AXI READ 3 PORT -------------------------------------------------
		 --CLOCK AND RESET----------------------------------------------------
         Rd3sRst              : in  std_ulogic;
         Rd3Clk               : in  std_ulogic;
         -- READ ADDRESS CHANNEL-----------------------------------------------
         Rd3ARID              : in  std_logic_vector(15 downto 0);
         Rd3ARADDR            : in  std_logic_vector(31 downto 0);
         Rd3ARLEN             : in  std_logic_vector(7 downto 0);
         Rd3ARSIZE            : in  std_logic_vector(2 downto 0);
         Rd3ARBURST           : in  std_logic_vector(1 downto 0);
         Rd3ARVALID           : in  std_ulogic;
         Rd3ARREADY           : out std_ulogic;
         -- READ DATA CHANNEL------------------------------------------------------
         Rd3RID               : out std_logic_vector(15 downto 0);
         Rd3RDATA             : out std_logic_vector(127 downto 0);
         Rd3RRESP             : out std_logic_vector(1 downto 0);
         Rd3RLAST             : out std_ulogic;
         Rd3RVALID            : out std_ulogic;
         Rd3RREADY            : in  std_ulogic;

         -- STATUS -------------------------------------------------------------
         InitDone             : out std_ulogic;
         -- PHYSICAL INTERFACE -------------------------------------------------
         -- RESET OUTPUT
         sRst_o               : out std_ulogic;
         sys_rst              : in  std_ulogic;
         UsrClk               : out std_ulogic;
         Fail_wrDataEn        : out std_logic_vector(0 downto 0);  -- 0-0
         c0_ddr4_ba           : out std_logic_vector(1 downto 0);
         c0_ddr4_dqs_c        : inout std_logic_vector(7 downto 0); -- 7-0
         c0_ddr4_dqs_t        : inout std_logic_vector(7 downto 0); -- 7-0
         c0_ddr4_ck_c         : out std_logic_vector(0 downto 0); -- 0-0
         c0_ddr4_ck_t         : out std_logic_vector(0 downto 0); -- 0-0
         c0_ddr4_adr          : out std_logic_vector(16 downto 0);
         c0_ddr4_cke          : out std_logic_vector(0 downto 0); -- 0-0
         c0_ddr4_odt          : out std_logic_vector(0 downto 0); -- 0-0
         c0_ddr4_reset_n      : out std_ulogic;
         c0_ddr4_dq           : inout std_logic_vector(63 downto 0); -- 63-0
         c0_ddr4_act_n        : out std_ulogic;
         c0_ddr4_bg           : out std_logic_vector(0 downto 0);
         c0_ddr4_dm_dbi_n     : inout std_logic_vector(7 downto 0); -- 7-0
         c0_ddr4_cs_n         : out std_logic_vector(0 downto 0); -- 0-0
         -- CLOCK (INPUT TO PHY)
         c0_sys_clk_p         : in  std_ulogic;
         c0_sys_clk_n         : in  std_ulogic
      );
   end component;
end package;

------------------------------------------------------------------------
-- E N T I T Y
------------------------------------------------------------------------
library work;
library ieee;
use ieee.std_logic_1164.all;
use work.SpdDummyComponent.all;
use work.ToolsPck.all;
use work.BurstGenWrComponent.all;
use work.BurstGenRdComponent.all;
use work.SdramPhyUltrascale_0Component_1.all;
use work.AxiWrPortComponent.all;
use work.SdramCtrlReorderComponent.all;
use work.AxiRdPortComponent.all;
use ieee.std_logic_arith.all;

entity DDR4Ctrl_1 is
   generic(
      g_TestBackpressure   : natural := 0
   );
   port(
      
      -- 128B AXI WRITE PORT ------------------------------------------------
      -- CLOCK AND RESET
      Wr0sRst              : in  std_ulogic;
      Wr0Clk               : in  std_ulogic;
      -- WRITE ADDRESS CHANNEL
      Wr0AWID              : in  std_logic_vector(15 downto 0);
      Wr0AWADDR            : in  std_logic_vector(31 downto 0);
      Wr0AWLEN             : in  std_logic_vector(7 downto 0);
      Wr0AWSIZE            : in  std_logic_vector(2 downto 0);
      Wr0AWBURST           : in  std_logic_vector(1 downto 0);
      Wr0AWVALID           : in  std_ulogic;
      Wr0AWREADY           : out std_ulogic;
      -- WRITE DATA CHANNEL
      Wr0WID               : in  std_logic_vector(15 downto 0);
      Wr0WDATA             : in  std_logic_vector(127 downto 0);
      Wr0WSTRB             : in  std_logic_vector(15 downto 0);
      Wr0WLAST             : in  std_ulogic;
      Wr0WVALID            : in  std_ulogic;
      Wr0WREADY            : out std_ulogic;
      -- WRITE RESPONSE CHANNEL
      Wr0BID               : out std_logic_vector(15 downto 0);
      Wr0BRESP             : out std_logic_vector(1 downto 0);
      Wr0BVALID            : out std_ulogic;
      Wr0BREADY            : in  std_ulogic;
      
      -- 128B AXI WRITE PORT ------------------------------------------------
      -- CLOCK AND RESET
      Wr1sRst              : in  std_ulogic;
      Wr1Clk               : in  std_ulogic;
      -- WRITE ADDRESS CHANNEL
      Wr1AWID              : in  std_logic_vector(15 downto 0);
      Wr1AWADDR            : in  std_logic_vector(31 downto 0);
      Wr1AWLEN             : in  std_logic_vector(7 downto 0);
      Wr1AWSIZE            : in  std_logic_vector(2 downto 0);
      Wr1AWBURST           : in  std_logic_vector(1 downto 0);
      Wr1AWVALID           : in  std_ulogic;
      Wr1AWREADY           : out std_ulogic;
      -- WRITE DATA CHANNEL
      Wr1WID               : in  std_logic_vector(15 downto 0);
      Wr1WDATA             : in  std_logic_vector(127 downto 0);
      Wr1WSTRB             : in  std_logic_vector(15 downto 0);
      Wr1WLAST             : in  std_ulogic;
      Wr1WVALID            : in  std_ulogic;
      Wr1WREADY            : out std_ulogic;
      -- WRITE RESPONSE CHANNEL
      Wr1BID               : out std_logic_vector(15 downto 0);
      Wr1BRESP             : out std_logic_vector(1 downto 0);
      Wr1BVALID            : out std_ulogic;
      Wr1BREADY            : in  std_ulogic;
      -- 128B AXI WRITE PORT ------------------------------------------------
      -- CLOCK AND RESET
      Wr4sRst              : in  std_ulogic;
      Wr4Clk               : in  std_ulogic;
      -- WRITE ADDRESS CHANNEL
      Wr4AWID              : in  std_logic_vector(15 downto 0);
      Wr4AWADDR            : in  std_logic_vector(31 downto 0);
      Wr4AWLEN             : in  std_logic_vector(7 downto 0);
      Wr4AWSIZE            : in  std_logic_vector(2 downto 0);
      Wr4AWBURST           : in  std_logic_vector(1 downto 0);
      Wr4AWVALID           : in  std_ulogic;
      Wr4AWREADY           : out std_ulogic;
      -- WRITE DATA CHANNEL
      Wr4WID               : in  std_logic_vector(15 downto 0);
      Wr4WDATA             : in  std_logic_vector(127 downto 0);
      Wr4WSTRB             : in  std_logic_vector(15 downto 0);
      Wr4WLAST             : in  std_ulogic;
      Wr4WVALID            : in  std_ulogic;
      Wr4WREADY            : out std_ulogic;
      -- WRITE RESPONSE CHANNEL
      Wr4BID               : out std_logic_vector(15 downto 0);
      Wr4BRESP             : out std_logic_vector(1 downto 0);
      Wr4BVALID            : out std_ulogic;
      Wr4BREADY            : in  std_ulogic;
  
      -- 128B AXI READ PORT -------------------------------------------------
      -- CLOCK AND RESET
      Rd0sRst              : in  std_ulogic;
      Rd0Clk               : in  std_ulogic;
      -- READ ADDRESS CHANNEL
      Rd0ARID              : in  std_logic_vector(15 downto 0);
      Rd0ARADDR            : in  std_logic_vector(31 downto 0);
      Rd0ARLEN             : in  std_logic_vector(7 downto 0);
      Rd0ARSIZE            : in  std_logic_vector(2 downto 0);
      Rd0ARBURST           : in  std_logic_vector(1 downto 0);
      Rd0ARVALID           : in  std_ulogic;
      Rd0ARREADY           : out std_ulogic;
      -- READ DATA CHANNEL
      Rd0RID               : out std_logic_vector(15 downto 0);
      Rd0RDATA             : out std_logic_vector(127 downto 0);
      Rd0RRESP             : out std_logic_vector(1 downto 0);
      Rd0RLAST             : out std_ulogic;
      Rd0RVALID            : out std_ulogic;
      Rd0RREADY            : in  std_ulogic;
      
      -- 128B AXI READ PORT -------------------------------------------------
      -- CLOCK AND RESET
      Rd1sRst              : in  std_ulogic;
      Rd1Clk               : in  std_ulogic;
      -- READ ADDRESS CHANNEL
      Rd1ARID              : in  std_logic_vector(15 downto 0);
      Rd1ARADDR            : in  std_logic_vector(31 downto 0);
      Rd1ARLEN             : in  std_logic_vector(7 downto 0);
      Rd1ARSIZE            : in  std_logic_vector(2 downto 0);
      Rd1ARBURST           : in  std_logic_vector(1 downto 0);
      Rd1ARVALID           : in  std_ulogic;
      Rd1ARREADY           : out std_ulogic;
      -- READ DATA CHANNEL
      Rd1RID               : out std_logic_vector(15 downto 0);
      Rd1RDATA             : out std_logic_vector(127 downto 0);
      Rd1RRESP             : out std_logic_vector(1 downto 0);
      Rd1RLAST             : out std_ulogic;
      Rd1RVALID            : out std_ulogic;
      Rd1RREADY            : in  std_ulogic;
      -- 128B AXI READ PORT -------------------------------------------------
      -- CLOCK AND RESET
      Rd4sRst              : in  std_ulogic;
      Rd4Clk               : in  std_ulogic;
      -- READ ADDRESS CHANNEL
      Rd4ARID              : in  std_logic_vector(15 downto 0);
      Rd4ARADDR            : in  std_logic_vector(31 downto 0);
      Rd4ARLEN             : in  std_logic_vector(7 downto 0);
      Rd4ARSIZE            : in  std_logic_vector(2 downto 0);
      Rd4ARBURST           : in  std_logic_vector(1 downto 0);
      Rd4ARVALID           : in  std_ulogic;
      Rd4ARREADY           : out std_ulogic;
      -- READ DATA CHANNEL
      Rd4RID               : out std_logic_vector(15 downto 0);
      Rd4RDATA             : out std_logic_vector(127 downto 0);
      Rd4RRESP             : out std_logic_vector(1 downto 0);
      Rd4RLAST             : out std_ulogic;
      Rd4RVALID            : out std_ulogic;
      Rd4RREADY            : in  std_ulogic;
      -- 128B AXI WRITE PORT ------------------------------------------------
      -- CLOCK AND RESET
      Wr2sRst              : in  std_ulogic;
      Wr2Clk               : in  std_ulogic;
      -- WRITE ADDRESS CHANNEL
      Wr2AWID              : in  std_logic_vector(15 downto 0);
      Wr2AWADDR            : in  std_logic_vector(31 downto 0);
      Wr2AWLEN             : in  std_logic_vector(7 downto 0);
      Wr2AWSIZE            : in  std_logic_vector(2 downto 0);
      Wr2AWBURST           : in  std_logic_vector(1 downto 0);
      Wr2AWVALID           : in  std_ulogic;
      Wr2AWREADY           : out std_ulogic;
      -- WRITE DATA CHANNEL
      Wr2WID               : in  std_logic_vector(15 downto 0);
      Wr2WDATA             : in  std_logic_vector(127 downto 0);
      Wr2WSTRB             : in  std_logic_vector(15 downto 0);
      Wr2WLAST             : in  std_ulogic;
      Wr2WVALID            : in  std_ulogic;
      Wr2WREADY            : out std_ulogic;
      -- WRITE RESPONSE CHANNEL
      Wr2BID               : out std_logic_vector(15 downto 0);
      Wr2BRESP             : out std_logic_vector(1 downto 0);
      Wr2BVALID            : out std_ulogic;
      Wr2BREADY            : in  std_ulogic;
      -- 128B AXI WRITE PORT ------------------------------------------------
      -- CLOCK AND RESET
      Wr3sRst              : in  std_ulogic;
      Wr3Clk               : in  std_ulogic;
      -- WRITE ADDRESS CHANNEL
      Wr3AWID              : in  std_logic_vector(15 downto 0);
      Wr3AWADDR            : in  std_logic_vector(31 downto 0);
      Wr3AWLEN             : in  std_logic_vector(7 downto 0);
      Wr3AWSIZE            : in  std_logic_vector(2 downto 0);
      Wr3AWBURST           : in  std_logic_vector(1 downto 0);
      Wr3AWVALID           : in  std_ulogic;
      Wr3AWREADY           : out std_ulogic;
      -- WRITE DATA CHANNEL
      Wr3WID               : in  std_logic_vector(15 downto 0);
      Wr3WDATA             : in  std_logic_vector(127 downto 0);
      Wr3WSTRB             : in  std_logic_vector(15 downto 0);
      Wr3WLAST             : in  std_ulogic;
      Wr3WVALID            : in  std_ulogic;
      Wr3WREADY            : out std_ulogic;
      -- WRITE RESPONSE CHANNEL
      Wr3BID               : out std_logic_vector(15 downto 0);
      Wr3BRESP             : out std_logic_vector(1 downto 0);
      Wr3BVALID            : out std_ulogic;
      Wr3BREADY            : in  std_ulogic;
      -- 128B AXI READ PORT -------------------------------------------------
      -- CLOCK AND RESET
      Rd2sRst              : in  std_ulogic;
      Rd2Clk               : in  std_ulogic;
      -- READ ADDRESS CHANNEL
      Rd2ARID              : in  std_logic_vector(15 downto 0);
      Rd2ARADDR            : in  std_logic_vector(31 downto 0);
      Rd2ARLEN             : in  std_logic_vector(7 downto 0);
      Rd2ARSIZE            : in  std_logic_vector(2 downto 0);
      Rd2ARBURST           : in  std_logic_vector(1 downto 0);
      Rd2ARVALID           : in  std_ulogic;
      Rd2ARREADY           : out std_ulogic;
      -- READ DATA CHANNEL
      Rd2RID               : out std_logic_vector(15 downto 0);
      Rd2RDATA             : out std_logic_vector(127 downto 0);
      Rd2RRESP             : out std_logic_vector(1 downto 0);
      Rd2RLAST             : out std_ulogic;
      Rd2RVALID            : out std_ulogic;
      Rd2RREADY            : in  std_ulogic;
      -- 128B AXI READ PORT -------------------------------------------------
      -- CLOCK AND RESET
      Rd3sRst              : in  std_ulogic;
      Rd3Clk               : in  std_ulogic;
      -- READ ADDRESS CHANNEL
      Rd3ARID              : in  std_logic_vector(15 downto 0);
      Rd3ARADDR            : in  std_logic_vector(31 downto 0);
      Rd3ARLEN             : in  std_logic_vector(7 downto 0);
      Rd3ARSIZE            : in  std_logic_vector(2 downto 0);
      Rd3ARBURST           : in  std_logic_vector(1 downto 0);
      Rd3ARVALID           : in  std_ulogic;
      Rd3ARREADY           : out std_ulogic;
      -- READ DATA CHANNEL
      Rd3RID               : out std_logic_vector(15 downto 0);
      Rd3RDATA             : out std_logic_vector(127 downto 0);
      Rd3RRESP             : out std_logic_vector(1 downto 0);
      Rd3RLAST             : out std_ulogic;
      Rd3RVALID            : out std_ulogic;
      Rd3RREADY            : in  std_ulogic;
      
      -- STATUS -------------------------------------------------------------
      InitDone             : out std_ulogic;
      
      -- PHYSICAL INTERFACE -------------------------------------------------
      -- RESET OUTPUT
      sRst_o               : out std_ulogic;
      sys_rst              : in  std_ulogic;
      UsrClk               : out std_ulogic;
      Fail_wrDataEn        : out std_logic_vector(0 downto 0); -- 0-0
      c0_ddr4_ba           : out std_logic_vector(1 downto 0);
      c0_ddr4_dqs_c        : inout std_logic_vector(7 downto 0); -- 7-0
      c0_ddr4_dqs_t        : inout std_logic_vector(7 downto 0); -- 7-0
      c0_ddr4_ck_c         : out std_logic_vector(0 downto 0); -- 0-0
      c0_ddr4_ck_t         : out std_logic_vector(0 downto 0); -- 0-0
      c0_ddr4_adr          : out std_logic_vector(16 downto 0);
      c0_ddr4_cke          : out std_logic_vector(0 downto 0); -- 0-0
      c0_ddr4_odt          : out std_logic_vector(0 downto 0); -- 0-0
      c0_ddr4_reset_n      : out std_ulogic;
      c0_ddr4_dq           : inout std_logic_vector(63 downto 0); -- 63-0
      c0_ddr4_act_n        : out std_ulogic;
      c0_ddr4_bg           : out std_logic_vector(0 downto 0);
      c0_ddr4_dm_dbi_n     : inout std_logic_vector(7 downto 0); -- 7-0
      c0_ddr4_cs_n         : out std_logic_vector(0 downto 0); -- 0-0
      -- CLOCK (INPUT TO PHY)
      c0_sys_clk_p         : in  std_ulogic;
      c0_sys_clk_n         : in  std_ulogic
   );
   -- attribute secure_config : string;
   -- attribute secure_config of DDR4Ctrl : entity is "PROTECT";   -- "OFF"/"PROTECT" Prevents CLB programming from being viewed or edited through FPGA Editor
   -- attribute secure_bitstream : string;
   -- attribute secure_bitstream of DDR4Ctrl : entity is "OFF";    -- "OFF"/"PROHIBIT" Prohibits bitstream generation
   -- attribute secure_netlist : string;
   -- attribute secure_netlist of DDR4Ctrl : entity is "ENCRYPT";  -- "OFF"/"ENCRYPT" Directs NGC2EDIF to encrypt the EDIF output
end DDR4Ctrl_1;

------------------------------------------------------------------------
-- A R C H I T E C T U R E
------------------------------------------------------------------------
architecture rtl of DDR4Ctrl_1 is

   -- Clock and reset
   signal sRst                 : std_ulogic;
   signal Clk                  : std_ulogic;
   
   -- 128b AXI write port ------------------------------------------------
   -- Write address channel
   signal Wr0_DW_AWID          : std_logic_vector(15 downto 0);
   signal Wr0_DW_AWADDR        : std_logic_vector(31 downto 0);
   signal Wr0_DW_AWLEN         : std_logic_vector(7 downto 0);
   signal Wr0_DW_AWSIZE        : std_logic_vector(2 downto 0);
   signal Wr0_DW_AWBURST       : std_logic_vector(1 downto 0);
   signal Wr0_DW_AWVALID       : std_ulogic;
   signal Wr0_DW_AWREADY       : std_ulogic;
   -- Write data channel
   signal Wr0_DW_WID           : std_logic_vector(15 downto 0);
   signal Wr0_DW_WDATA         : std_logic_vector(127 downto 0);
   signal Wr0_DW_WSTRB         : std_logic_vector(15 downto 0);
   signal Wr0_DW_WLAST         : std_ulogic;
   signal Wr0_DW_WVALID        : std_ulogic;
   signal Wr0_DW_WREADY        : std_ulogic;
   -- Write response channel
   signal Wr0_DW_BID           : std_logic_vector(15 downto 0);
   signal Wr0_DW_BRESP         : std_logic_vector(1 downto 0);
   signal Wr0_DW_BVALID        : std_ulogic;
   signal Wr0_DW_BREADY        : std_ulogic;
   -- Input channel
   signal Wr0_BW_WrData        : std_logic_vector(127 downto 0);
   signal Wr0_BW_WrMask        : std_logic_vector(15 downto 0);
   signal Wr0_BW_Strobe        : std_ulogic;
   signal Wr0_BW_Wait          : std_ulogic;
   -- Control signals
   signal Wr0_BW_EndOfBurst    : std_ulogic;
   signal Wr0_BW_WrAck         : std_ulogic;
   signal Wr0_BW_WrAckWait     : std_ulogic;
   signal Wr0_BW_WrPending     : std_ulogic;
   signal Wr0_BW_CmdPending    : std_ulogic;
   signal Wr0_BW_AddrDiff      : std_ulogic;
   -- Command Strobe channel
   signal Wr0_RW_CmdStrobe     : std_logic_vector(7 downto 0);
   signal Wr0_RW_CmdFlag       : std_logic_vector(7 downto 0);
   signal Wr0_RW_CmdIndex      : std_logic_vector(63 downto 0);
   signal Wr0_RW_CmdWait       : std_logic_vector(7 downto 0);
   -- Input channel
   signal Wr0_BW_Addr          : std_logic_vector(26 downto 0);
   -- Command Get channel
   signal Wr0_RW_CmdGet        : std_logic_vector(0 downto 0);
   signal Wr0_RW_CmdGetIndex   : std_logic_vector(7 downto 0);
   signal Wr0_RW_CmdGetAddr    : std_logic_vector(21 downto 0);
   -- Write data channel
   signal Wr0_RW_Data          : std_logic_vector(511 downto 0);
   signal Wr0_RW_Mask          : std_logic_vector(63 downto 0);
   signal Wr0_RW_GetData       : std_logic_vector(0 downto 0);
   signal Wr0_RW_DataIndex     : std_logic_vector(7 downto 0);
   
   -- 128b AXI write port ------------------------------------------------
   -- Write address channel
   signal Wr1_DW_AWID          : std_logic_vector(15 downto 0);
   signal Wr1_DW_AWADDR        : std_logic_vector(31 downto 0);
   signal Wr1_DW_AWLEN         : std_logic_vector(7 downto 0);
   signal Wr1_DW_AWSIZE        : std_logic_vector(2 downto 0);
   signal Wr1_DW_AWBURST       : std_logic_vector(1 downto 0);
   signal Wr1_DW_AWVALID       : std_ulogic;
   signal Wr1_DW_AWREADY       : std_ulogic;
   -- Write data channel
   signal Wr1_DW_WID           : std_logic_vector(15 downto 0);
   signal Wr1_DW_WDATA         : std_logic_vector(127 downto 0);
   signal Wr1_DW_WSTRB         : std_logic_vector(15 downto 0);
   signal Wr1_DW_WLAST         : std_ulogic;
   signal Wr1_DW_WVALID        : std_ulogic;
   signal Wr1_DW_WREADY        : std_ulogic;
   -- Write response channel
   signal Wr1_DW_BID           : std_logic_vector(15 downto 0);
   signal Wr1_DW_BRESP         : std_logic_vector(1 downto 0);
   signal Wr1_DW_BVALID        : std_ulogic;
   signal Wr1_DW_BREADY        : std_ulogic;
   -- Input channel
   signal Wr1_BW_WrData        : std_logic_vector(127 downto 0);
   signal Wr1_BW_WrMask        : std_logic_vector(15 downto 0);
   signal Wr1_BW_Strobe        : std_ulogic;
   signal Wr1_BW_Wait          : std_ulogic;
   -- Control signals
   signal Wr1_BW_EndOfBurst    : std_ulogic;
   signal Wr1_BW_WrAck         : std_ulogic;
   signal Wr1_BW_WrAckWait     : std_ulogic;
   signal Wr1_BW_WrPending     : std_ulogic;
   signal Wr1_BW_CmdPending    : std_ulogic;
   signal Wr1_BW_AddrDiff      : std_ulogic;
   -- Command Strobe channel
   signal Wr1_RW_CmdStrobe     : std_logic_vector(7 downto 0);
   signal Wr1_RW_CmdFlag       : std_logic_vector(7 downto 0);
   signal Wr1_RW_CmdIndex      : std_logic_vector(63 downto 0);
   signal Wr1_RW_CmdWait       : std_logic_vector(7 downto 0);
   -- Input channel
   signal Wr1_BW_Addr          : std_logic_vector(26 downto 0);
   -- Command Get channel
   signal Wr1_RW_CmdGet        : std_logic_vector(0 downto 0);
   signal Wr1_RW_CmdGetIndex   : std_logic_vector(7 downto 0);
   signal Wr1_RW_CmdGetAddr    : std_logic_vector(21 downto 0);
   -- Write data channel
   signal Wr1_RW_Data          : std_logic_vector(511 downto 0);
   signal Wr1_RW_Mask          : std_logic_vector(63 downto 0);
   signal Wr1_RW_GetData       : std_logic_vector(0 downto 0);
   signal Wr1_RW_DataIndex     : std_logic_vector(7 downto 0);
   -- 128b AXI write port ------------------------------------------------
   -- Write address channel
   signal Wr4_DW_AWID          : std_logic_vector(15 downto 0);
   signal Wr4_DW_AWADDR        : std_logic_vector(31 downto 0);
   signal Wr4_DW_AWLEN         : std_logic_vector(7 downto 0);
   signal Wr4_DW_AWSIZE        : std_logic_vector(2 downto 0);
   signal Wr4_DW_AWBURST       : std_logic_vector(1 downto 0);
   signal Wr4_DW_AWVALID       : std_ulogic;
   signal Wr4_DW_AWREADY       : std_ulogic;
   -- Write data channel
   signal Wr4_DW_WID           : std_logic_vector(15 downto 0);
   signal Wr4_DW_WDATA         : std_logic_vector(127 downto 0);
   signal Wr4_DW_WSTRB         : std_logic_vector(15 downto 0);
   signal Wr4_DW_WLAST         : std_ulogic;
   signal Wr4_DW_WVALID        : std_ulogic;
   signal Wr4_DW_WREADY        : std_ulogic;
   -- Write response channel
   signal Wr4_DW_BID           : std_logic_vector(15 downto 0);
   signal Wr4_DW_BRESP         : std_logic_vector(1 downto 0);
   signal Wr4_DW_BVALID        : std_ulogic;
   signal Wr4_DW_BREADY        : std_ulogic;
   -- Input channel
   signal Wr4_BW_WrData        : std_logic_vector(127 downto 0);
   signal Wr4_BW_WrMask        : std_logic_vector(15 downto 0);
   signal Wr4_BW_Strobe        : std_ulogic;
   signal Wr4_BW_Wait          : std_ulogic;
   -- Control signals
   signal Wr4_BW_EndOfBurst    : std_ulogic;
   signal Wr4_BW_WrAck         : std_ulogic;
   signal Wr4_BW_WrAckWait     : std_ulogic;
   signal Wr4_BW_WrPending     : std_ulogic;
   signal Wr4_BW_CmdPending    : std_ulogic;
   signal Wr4_BW_AddrDiff      : std_ulogic;
   -- Command Strobe channel
   signal Wr4_RW_CmdStrobe     : std_logic_vector(7 downto 0);
   signal Wr4_RW_CmdFlag       : std_logic_vector(7 downto 0);
   signal Wr4_RW_CmdIndex      : std_logic_vector(63 downto 0);
   signal Wr4_RW_CmdWait       : std_logic_vector(7 downto 0);
   -- Input channel
   signal Wr4_BW_Addr          : std_logic_vector(26 downto 0);
   -- Command Get channel
   signal Wr4_RW_CmdGet        : std_logic_vector(0 downto 0);
   signal Wr4_RW_CmdGetIndex   : std_logic_vector(7 downto 0);
   signal Wr4_RW_CmdGetAddr    : std_logic_vector(21 downto 0);
   -- Write data channel
   signal Wr4_RW_Data          : std_logic_vector(511 downto 0);
   signal Wr4_RW_Mask          : std_logic_vector(63 downto 0);
   signal Wr4_RW_GetData       : std_logic_vector(0 downto 0);
   signal Wr4_RW_DataIndex     : std_logic_vector(7 downto 0);
   
   -- 128b AXI read port -------------------------------------------------
   -- Read address channel
   signal Rd0_DR_ARID          : std_logic_vector(15 downto 0);
   signal Rd0_DR_ARADDR        : std_logic_vector(31 downto 0);
   signal Rd0_DR_ARLEN         : std_logic_vector(7 downto 0);
   signal Rd0_DR_ARSIZE        : std_logic_vector(2 downto 0);
   signal Rd0_DR_ARBURST       : std_logic_vector(1 downto 0);
   signal Rd0_DR_ARVALID       : std_ulogic;
   signal Rd0_DR_ARREADY       : std_ulogic;
   -- Read data channel
   signal Rd0_DR_RID           : std_logic_vector(15 downto 0);
   signal Rd0_DR_RDATA         : std_logic_vector(127 downto 0);
   signal Rd0_DR_RRESP         : std_logic_vector(1 downto 0);
   signal Rd0_DR_RLAST         : std_ulogic;
   signal Rd0_DR_RVALID        : std_ulogic;
   signal Rd0_DR_RREADY        : std_ulogic;
   -- Input channel
   signal Rd0_BR_Len           : std_logic_vector(7 downto 0);
   signal Rd0_BR_UsrFlagIn     : std_logic_vector(16 downto 0);
   signal Rd0_BR_Strobe        : std_ulogic;
   signal Rd0_BR_Wait          : std_ulogic;
   -- Control signals
   signal Rd0_BR_EndOfBurst    : std_ulogic;
   signal Rd0_BR_RdPending     : std_ulogic;
   signal Rd0_BR_CmdPending    : std_ulogic;
   -- Output channel
   signal Rd0_BR_UsrFlagOut    : std_logic_vector(16 downto 0);
   signal Rd0_BR_RdData        : std_logic_vector(127 downto 0);
   signal Rd0_BR_RdDataStrobe  : std_ulogic;
   signal Rd0_BR_RdDataWait    : std_ulogic;
   -- Command Strobe channel
   signal Rd0_RR_CmdStrobe     : std_logic_vector(7 downto 0);
   signal Rd0_RR_CmdFlag       : std_logic_vector(7 downto 0);
   signal Rd0_RR_CmdIndex      : std_logic_vector(63 downto 0);
   signal Rd0_RR_CmdWait       : std_logic_vector(7 downto 0);
   -- Input channel
   signal Rd0_BR_Addr          : std_logic_vector(26 downto 0);
   -- Command Get channel
   signal Rd0_RR_CmdGet        : std_logic_vector(0 downto 0);
   signal Rd0_RR_CmdGetIndex   : std_logic_vector(7 downto 0);
   signal Rd0_RR_CmdGetAddr    : std_logic_vector(21 downto 0);
   signal Rd0_RR_PortDummyRead : std_logic_vector(0 downto 0);
   -- Read data channel
   signal Rd0_RR_Data          : std_logic_vector(511 downto 0);
   signal Rd0_RR_DataPut       : std_logic_vector(0 downto 0);
   signal Rd0_RR_DataIndex     : std_logic_vector(7 downto 0);
   
   -- 128b AXI read port -------------------------------------------------
   -- Read address channel
   signal Rd1_DR_ARID          : std_logic_vector(15 downto 0);
   signal Rd1_DR_ARADDR        : std_logic_vector(31 downto 0);
   signal Rd1_DR_ARLEN         : std_logic_vector(7 downto 0);
   signal Rd1_DR_ARSIZE        : std_logic_vector(2 downto 0);
   signal Rd1_DR_ARBURST       : std_logic_vector(1 downto 0);
   signal Rd1_DR_ARVALID       : std_ulogic;
   signal Rd1_DR_ARREADY       : std_ulogic;
   -- Read data channel
   signal Rd1_DR_RID           : std_logic_vector(15 downto 0);
   signal Rd1_DR_RDATA         : std_logic_vector(127 downto 0);
   signal Rd1_DR_RRESP         : std_logic_vector(1 downto 0);
   signal Rd1_DR_RLAST         : std_ulogic;
   signal Rd1_DR_RVALID        : std_ulogic;
   signal Rd1_DR_RREADY        : std_ulogic;
   -- Input channel
   signal Rd1_BR_Len           : std_logic_vector(7 downto 0);
   signal Rd1_BR_UsrFlagIn     : std_logic_vector(16 downto 0);
   signal Rd1_BR_Strobe        : std_ulogic;
   signal Rd1_BR_Wait          : std_ulogic;
   -- Control signals
   signal Rd1_BR_EndOfBurst    : std_ulogic;
   signal Rd1_BR_RdPending     : std_ulogic;
   signal Rd1_BR_CmdPending    : std_ulogic;
   -- Output channel
   signal Rd1_BR_UsrFlagOut    : std_logic_vector(16 downto 0);
   signal Rd1_BR_RdData        : std_logic_vector(127 downto 0);
   signal Rd1_BR_RdDataStrobe  : std_ulogic;
   signal Rd1_BR_RdDataWait    : std_ulogic;
   -- Command Strobe channel
   signal Rd1_RR_CmdStrobe     : std_logic_vector(7 downto 0);
   signal Rd1_RR_CmdFlag       : std_logic_vector(7 downto 0);
   signal Rd1_RR_CmdIndex      : std_logic_vector(63 downto 0);
   signal Rd1_RR_CmdWait       : std_logic_vector(7 downto 0);
   -- Input channel
   signal Rd1_BR_Addr          : std_logic_vector(26 downto 0);
   -- Command Get channel
   signal Rd1_RR_CmdGet        : std_logic_vector(0 downto 0);
   signal Rd1_RR_CmdGetIndex   : std_logic_vector(7 downto 0);

   signal Rd1_RR_CmdGetAddr    : std_logic_vector(21 downto 0);
   -- Read data channel
   signal Rd1_RR_Data          : std_logic_vector(511 downto 0);
   signal Rd1_RR_DataPut       : std_logic_vector(0 downto 0);
   signal Rd1_RR_DataIndex     : std_logic_vector(7 downto 0);
   -- 128b AXI read port -------------------------------------------------
   -- Read address channel
   signal Rd4_DR_ARID          : std_logic_vector(15 downto 0);
   signal Rd4_DR_ARADDR        : std_logic_vector(31 downto 0);
   signal Rd4_DR_ARLEN         : std_logic_vector(7 downto 0);
   signal Rd4_DR_ARSIZE        : std_logic_vector(2 downto 0);
   signal Rd4_DR_ARBURST       : std_logic_vector(1 downto 0);
   signal Rd4_DR_ARVALID       : std_ulogic;
   signal Rd4_DR_ARREADY       : std_ulogic;
   -- Read data channel
   signal Rd4_DR_RID           : std_logic_vector(15 downto 0);
   signal Rd4_DR_RDATA         : std_logic_vector(127 downto 0);
   signal Rd4_DR_RRESP         : std_logic_vector(1 downto 0);
   signal Rd4_DR_RLAST         : std_ulogic;
   signal Rd4_DR_RVALID        : std_ulogic;
   signal Rd4_DR_RREADY        : std_ulogic;
   -- Input channel
   signal Rd4_BR_Len           : std_logic_vector(7 downto 0);
   signal Rd4_BR_UsrFlagIn     : std_logic_vector(16 downto 0);
   signal Rd4_BR_Strobe        : std_ulogic;
   signal Rd4_BR_Wait          : std_ulogic;
   -- Control signals
   signal Rd4_BR_EndOfBurst    : std_ulogic;
   signal Rd4_BR_RdPending     : std_ulogic;
   signal Rd4_BR_CmdPending    : std_ulogic;
   -- Output channel
   signal Rd4_BR_UsrFlagOut    : std_logic_vector(16 downto 0);
   signal Rd4_BR_RdData        : std_logic_vector(127 downto 0);
   signal Rd4_BR_RdDataStrobe  : std_ulogic;
   signal Rd4_BR_RdDataWait    : std_ulogic;
   -- Command Strobe channel
   signal Rd4_RR_CmdStrobe     : std_logic_vector(7 downto 0);
   signal Rd4_RR_CmdFlag       : std_logic_vector(7 downto 0);
   signal Rd4_RR_CmdIndex      : std_logic_vector(63 downto 0);
   signal Rd4_RR_CmdWait       : std_logic_vector(7 downto 0);
   -- Input channel
   signal Rd4_BR_Addr          : std_logic_vector(26 downto 0);
   -- Command Get channel
   signal Rd4_RR_CmdGet        : std_logic_vector(0 downto 0);
   signal Rd4_RR_CmdGetIndex   : std_logic_vector(7 downto 0);
   signal Rd4_RR_CmdGetAddr    : std_logic_vector(21 downto 0);
   -- Read data channel
   signal Rd4_RR_Data          : std_logic_vector(511 downto 0);
   signal Rd4_RR_DataPut       : std_logic_vector(0 downto 0);
   signal Rd4_RR_DataIndex     : std_logic_vector(7 downto 0);
   
   -- 128b AXI write port ------------------------------------------------
   -- Write address channel
   signal Wr2_DW_AWID          : std_logic_vector(15 downto 0);
   signal Wr2_DW_AWADDR        : std_logic_vector(31 downto 0);
   signal Wr2_DW_AWLEN         : std_logic_vector(7 downto 0);
   signal Wr2_DW_AWSIZE        : std_logic_vector(2 downto 0);
   signal Wr2_DW_AWBURST       : std_logic_vector(1 downto 0);
   signal Wr2_DW_AWVALID       : std_ulogic;
   signal Wr2_DW_AWREADY       : std_ulogic;
   -- Write data channel
   signal Wr2_DW_WID           : std_logic_vector(15 downto 0);
   signal Wr2_DW_WDATA         : std_logic_vector(127 downto 0);
   signal Wr2_DW_WSTRB         : std_logic_vector(15 downto 0);
   signal Wr2_DW_WLAST         : std_ulogic;
   signal Wr2_DW_WVALID        : std_ulogic;
   signal Wr2_DW_WREADY        : std_ulogic;
   -- Write response channel
   signal Wr2_DW_BID           : std_logic_vector(15 downto 0);
   signal Wr2_DW_BRESP         : std_logic_vector(1 downto 0);
   signal Wr2_DW_BVALID        : std_ulogic;
   signal Wr2_DW_BREADY        : std_ulogic;
   -- Input channel
   signal Wr2_BW_WrData        : std_logic_vector(127 downto 0);
   signal Wr2_BW_WrMask        : std_logic_vector(15 downto 0);
   signal Wr2_BW_Strobe        : std_ulogic;
   signal Wr2_BW_Wait          : std_ulogic;
   -- Control signals
   signal Wr2_BW_EndOfBurst    : std_ulogic;
   signal Wr2_BW_WrAck         : std_ulogic;
   signal Wr2_BW_WrAckWait     : std_ulogic;
   signal Wr2_BW_WrPending     : std_ulogic;
   signal Wr2_BW_CmdPending    : std_ulogic;
   signal Wr2_BW_AddrDiff      : std_ulogic;
   -- Command Strobe channel
   signal Wr2_RW_CmdStrobe     : std_logic_vector(7 downto 0);
   signal Wr2_RW_CmdFlag       : std_logic_vector(7 downto 0);
   signal Wr2_RW_CmdIndex      : std_logic_vector(63 downto 0);
   signal Wr2_RW_CmdWait       : std_logic_vector(7 downto 0);
   -- Input channel
   signal Wr2_BW_Addr          : std_logic_vector(26 downto 0);
   -- Command Get channel
   signal Wr2_RW_CmdGet        : std_logic_vector(0 downto 0);
   signal Wr2_RW_CmdGetIndex   : std_logic_vector(7 downto 0);
   signal Wr2_RW_CmdGetAddr    : std_logic_vector(21 downto 0);
   -- Write data channel
   signal Wr2_RW_Data          : std_logic_vector(511 downto 0);
   signal Wr2_RW_Mask          : std_logic_vector(63 downto 0);
   signal Wr2_RW_GetData       : std_logic_vector(0 downto 0);
   signal Wr2_RW_DataIndex     : std_logic_vector(7 downto 0);
   -- 128b AXI write port ------------------------------------------------
   -- Write address channel
   signal Wr3_DW_AWID          : std_logic_vector(15 downto 0);
   signal Wr3_DW_AWADDR        : std_logic_vector(31 downto 0);
   signal Wr3_DW_AWLEN         : std_logic_vector(7 downto 0);
   signal Wr3_DW_AWSIZE        : std_logic_vector(2 downto 0);
   signal Wr3_DW_AWBURST       : std_logic_vector(1 downto 0);
   signal Wr3_DW_AWVALID       : std_ulogic;
   signal Wr3_DW_AWREADY       : std_ulogic;
   -- Write data channel
   signal Wr3_DW_WID           : std_logic_vector(15 downto 0);
   signal Wr3_DW_WDATA         : std_logic_vector(127 downto 0);
   signal Wr3_DW_WSTRB         : std_logic_vector(15 downto 0);
   signal Wr3_DW_WLAST         : std_ulogic;
   signal Wr3_DW_WVALID        : std_ulogic;
   signal Wr3_DW_WREADY        : std_ulogic;
   -- Write response channel
   signal Wr3_DW_BID           : std_logic_vector(15 downto 0);
   signal Wr3_DW_BRESP         : std_logic_vector(1 downto 0);
   signal Wr3_DW_BVALID        : std_ulogic;
   signal Wr3_DW_BREADY        : std_ulogic;
   -- Input channel
   signal Wr3_BW_WrData        : std_logic_vector(127 downto 0);
   signal Wr3_BW_WrMask        : std_logic_vector(15 downto 0);
   signal Wr3_BW_Strobe        : std_ulogic;
   signal Wr3_BW_Wait          : std_ulogic;
   -- Control signals
   signal Wr3_BW_EndOfBurst    : std_ulogic;
   signal Wr3_BW_WrAck         : std_ulogic;
   signal Wr3_BW_WrAckWait     : std_ulogic;
   signal Wr3_BW_WrPending     : std_ulogic;
   signal Wr3_BW_CmdPending    : std_ulogic;
   signal Wr3_BW_AddrDiff      : std_ulogic;
   -- Command Strobe channel
   signal Wr3_RW_CmdStrobe     : std_logic_vector(7 downto 0);
   signal Wr3_RW_CmdFlag       : std_logic_vector(7 downto 0);
   signal Wr3_RW_CmdIndex      : std_logic_vector(63 downto 0);
   signal Wr3_RW_CmdWait       : std_logic_vector(7 downto 0);
   -- Input channel
   signal Wr3_BW_Addr          : std_logic_vector(26 downto 0);
   -- Command Get channel
   signal Wr3_RW_CmdGet        : std_logic_vector(0 downto 0);
   signal Wr3_RW_CmdGetIndex   : std_logic_vector(7 downto 0);
   signal Wr3_RW_CmdGetAddr    : std_logic_vector(21 downto 0);
   -- Write data channel
   signal Wr3_RW_Data          : std_logic_vector(511 downto 0);
   signal Wr3_RW_Mask          : std_logic_vector(63 downto 0);
   signal Wr3_RW_GetData       : std_logic_vector(0 downto 0);
   signal Wr3_RW_DataIndex     : std_logic_vector(7 downto 0);
   -- 128b AXI read port -------------------------------------------------
   -- Read address channel
   signal Rd2_DR_ARID          : std_logic_vector(15 downto 0);
   signal Rd2_DR_ARADDR        : std_logic_vector(31 downto 0);
   signal Rd2_DR_ARLEN         : std_logic_vector(7 downto 0);
   signal Rd2_DR_ARSIZE        : std_logic_vector(2 downto 0);
   signal Rd2_DR_ARBURST       : std_logic_vector(1 downto 0);
   signal Rd2_DR_ARVALID       : std_ulogic;
   signal Rd2_DR_ARREADY       : std_ulogic;
   -- Read data channel
   signal Rd2_DR_RID           : std_logic_vector(15 downto 0);
   signal Rd2_DR_RDATA         : std_logic_vector(127 downto 0);
   signal Rd2_DR_RRESP         : std_logic_vector(1 downto 0);
   signal Rd2_DR_RLAST         : std_ulogic;
   signal Rd2_DR_RVALID        : std_ulogic;
   signal Rd2_DR_RREADY        : std_ulogic;
   -- Input channel
   signal Rd2_BR_Len           : std_logic_vector(7 downto 0);
   signal Rd2_BR_UsrFlagIn     : std_logic_vector(16 downto 0);
   signal Rd2_BR_Strobe        : std_ulogic;
   signal Rd2_BR_Wait          : std_ulogic;
   -- Control signals
   signal Rd2_BR_EndOfBurst    : std_ulogic;
   signal Rd2_BR_RdPending     : std_ulogic;
   signal Rd2_BR_CmdPending    : std_ulogic;
   -- Output channel
   signal Rd2_BR_UsrFlagOut    : std_logic_vector(16 downto 0);
   signal Rd2_BR_RdData        : std_logic_vector(127 downto 0);
   signal Rd2_BR_RdDataStrobe  : std_ulogic;
   signal Rd2_BR_RdDataWait    : std_ulogic;
   -- Command Strobe channel
   signal Rd2_RR_CmdStrobe     : std_logic_vector(7 downto 0);
   signal Rd2_RR_CmdFlag       : std_logic_vector(7 downto 0);
   signal Rd2_RR_CmdIndex      : std_logic_vector(63 downto 0);
   signal Rd2_RR_CmdWait       : std_logic_vector(7 downto 0);
   -- Input channel
   signal Rd2_BR_Addr          : std_logic_vector(26 downto 0);
   -- Command Get channel
   signal Rd2_RR_CmdGet        : std_logic_vector(0 downto 0);
   signal Rd2_RR_CmdGetIndex   : std_logic_vector(7 downto 0);
   signal Rd2_RR_CmdGetAddr    : std_logic_vector(21 downto 0);
   -- Read data channel
   signal Rd2_RR_Data          : std_logic_vector(511 downto 0);
   signal Rd2_RR_DataPut       : std_logic_vector(0 downto 0);
   signal Rd2_RR_DataIndex     : std_logic_vector(7 downto 0);
   -- 128b AXI read port -------------------------------------------------
   -- Read address channel
   signal Rd3_DR_ARID          : std_logic_vector(15 downto 0);
   signal Rd3_DR_ARADDR        : std_logic_vector(31 downto 0);
   signal Rd3_DR_ARLEN         : std_logic_vector(7 downto 0);
   signal Rd3_DR_ARSIZE        : std_logic_vector(2 downto 0);
   signal Rd3_DR_ARBURST       : std_logic_vector(1 downto 0);
   signal Rd3_DR_ARVALID       : std_ulogic;
   signal Rd3_DR_ARREADY       : std_ulogic;
   -- Read data channel
   signal Rd3_DR_RID           : std_logic_vector(15 downto 0);
   signal Rd3_DR_RDATA         : std_logic_vector(127 downto 0);
   signal Rd3_DR_RRESP         : std_logic_vector(1 downto 0);
   signal Rd3_DR_RLAST         : std_ulogic;
   signal Rd3_DR_RVALID        : std_ulogic;
   signal Rd3_DR_RREADY        : std_ulogic;
   -- Input channel
   signal Rd3_BR_Len           : std_logic_vector(7 downto 0);
   signal Rd3_BR_UsrFlagIn     : std_logic_vector(16 downto 0);
   signal Rd3_BR_Strobe        : std_ulogic;
   signal Rd3_BR_Wait          : std_ulogic;
   -- Control signals
   signal Rd3_BR_EndOfBurst    : std_ulogic;
   signal Rd3_BR_RdPending     : std_ulogic;
   signal Rd3_BR_CmdPending    : std_ulogic;
   -- Output channel
   signal Rd3_BR_UsrFlagOut    : std_logic_vector(16 downto 0);
   signal Rd3_BR_RdData        : std_logic_vector(127 downto 0);
   signal Rd3_BR_RdDataStrobe  : std_ulogic;
   signal Rd3_BR_RdDataWait    : std_ulogic;
   -- Command Strobe channel
   signal Rd3_RR_CmdStrobe     : std_logic_vector(7 downto 0);
   signal Rd3_RR_CmdFlag       : std_logic_vector(7 downto 0);
   signal Rd3_RR_CmdIndex      : std_logic_vector(63 downto 0);
   signal Rd3_RR_CmdWait       : std_logic_vector(7 downto 0);
   -- Input channel
   signal Rd3_BR_Addr          : std_logic_vector(26 downto 0);
   -- Command Get channel
   signal Rd3_RR_CmdGet        : std_logic_vector(0 downto 0);
   signal Rd3_RR_CmdGetIndex   : std_logic_vector(7 downto 0);
   signal Rd3_RR_CmdGetAddr    : std_logic_vector(21 downto 0);
   -- Read data channel
   signal Rd3_RR_Data          : std_logic_vector(511 downto 0);
   signal Rd3_RR_DataPut       : std_logic_vector(0 downto 0);
   signal Rd3_RR_DataIndex     : std_logic_vector(7 downto 0);
   
   -- Serial Presence Detect ---------------------------------------------
   -- SPD values
   signal Int_NUM_COL          : std_logic_vector(7 downto 0);
   signal Int_NUM_ROW          : std_logic_vector(7 downto 0);
   signal Int_NUM_BANK         : std_logic_vector(7 downto 0);
   signal Int_NUM_RANK         : std_logic_vector(7 downto 0);
   signal Int_SPD_DONE         : std_ulogic;
   -- Map Config
   signal MAP_ShiftCol         : std_logic_vector(0 downto 0);
   signal MAP_ShiftRow         : std_logic_vector(1 downto 0);
   signal MAP_ShiftCs          : std_logic_vector(2 downto 0);

-- Controller core ----------------------------------------------------
 
   -- Write address channel
   signal WrCmdStrobe          : std_logic_vector(39 downto 0);
   signal WrCmdFlag            : std_logic_vector(39 downto 0);
   signal WrCmdIndex           : std_logic_vector(319 downto 0);
   signal WrCmdWait            : std_logic_vector(39 downto 0);
 
   signal WrCmdGet             : std_logic_vector(4 downto 0);
   signal WrCmdGetIndex        : std_logic_vector(7 downto 0);
   signal WrCmdGetAddr         : std_logic_vector(109 downto 0);
   -- Write data channel
   signal WrData               : std_logic_vector(2559 downto 0);
   signal WrMask               : std_logic_vector(319 downto 0);
   signal WrGetData            : std_logic_vector(4 downto 0);
   signal WrDataIndex          : std_logic_vector(7 downto 0);
   signal WrDataIndexLsb       : std_logic_vector(0 downto 0);
   signal WrDataIndex_2x       : std_logic_vector(7 downto 0);
 
   -- Read address channel
   signal RdCmdStrobe          : std_logic_vector(39 downto 0);
   signal RdCmdFlag            : std_logic_vector(39 downto 0);
   signal RdCmdIndex           : std_logic_vector(319 downto 0);
   signal RdCmdWait            : std_logic_vector(39 downto 0);
--- Physical interface ----
   signal SdramBank            : std_logic_vector(11 downto 0);
 
   signal RdCmdGet             : std_logic_vector(4 downto 0);
   signal RdCmdGetIndex        : std_logic_vector(7 downto 0);
   signal RdCmdGetAddr         : std_logic_vector(109 downto 0);
   -- Read data channel
   signal RdData               : std_logic_vector(511 downto 0);
   signal RdDataPut            : std_logic_vector(4 downto 0);
   signal RdDataIndex          : std_logic_vector(7 downto 0);
   signal RdDataIndexLsb       : std_logic_vector(0 downto 0);
   signal PortDummyRead        : std_ulogic;
   
   -- Physical interface -------------------------------------------------
   -- Calibration
   signal CalibDone            : std_ulogic;
   signal DoRIU                : std_ulogic;
   -- Command output
   signal SdramACTn            : std_logic_vector(3 downto 0);
   signal SdramCSn             : std_logic_vector(3 downto 0);
   signal SdramRASn            : std_logic_vector(3 downto 0);
   signal SdramCASn            : std_logic_vector(3 downto 0);
   signal SdramWEn             : std_logic_vector(3 downto 0);
   signal SdramODT             : std_logic_vector(3 downto 0);
   signal SdramCKE             : std_logic_vector(3 downto 0);
   signal SdramAddr            : std_logic_vector(67 downto 0);
   signal SdramCmd             : std_logic_vector(2 downto 0);
   signal SdramSlot            : std_logic_vector(1 downto 0);
   -- Write data control
   signal SdramWrEn            : std_ulogic;
   signal SdramWrData          : std_logic_vector(511 downto 0);
   signal SdramWrMask          : std_logic_vector(63 downto 0);
   -- Read data control
   signal SdramRdEn            : std_ulogic;
   signal SdramRdData          : std_logic_vector(511 downto 0);
   signal RdDataValid          : std_ulogic;

begin
   
   ------------------------------------------------------------------------
   -- 128b AXI write port
   ------------------------------------------------------------------------
   
   -- Connection to ports ------------------------------------------------
   -- Write address channel
   Wr0_DW_AWID          <= Wr0AWID;
   Wr0_DW_AWADDR        <= Wr0AWADDR;
   Wr0_DW_AWLEN         <= Wr0AWLEN;
   Wr0_DW_AWSIZE        <= Wr0AWSIZE;
   Wr0_DW_AWBURST       <= Wr0AWBURST;
   Wr0_DW_AWVALID       <= Wr0AWVALID;
   Wr0AWREADY           <= Wr0_DW_AWREADY;
   -- Write data channel
   Wr0_DW_WID           <= Wr0WID;
   Wr0_DW_WDATA         <= Wr0WDATA;
   Wr0_DW_WSTRB         <= Wr0WSTRB;
   Wr0_DW_WLAST         <= Wr0WLAST;
   Wr0_DW_WVALID        <= Wr0WVALID;
   Wr0WREADY            <= Wr0_DW_WREADY;
   -- Write response channel
   Wr0BID               <= Wr0_DW_BID;
   Wr0BRESP             <= Wr0_DW_BRESP;
   Wr0BVALID            <= Wr0_DW_BVALID;
   Wr0_DW_BREADY        <= Wr0BREADY;
   
   i_Wr0_AxiWrPort : AxiWrPort
   generic map(
      c_PortName           => "Wr0",
      c_MaxNbCmd           => 256, 
      c_DataWidth          => 128,
      c_AddrWidth          => 27, 
      c_LenWidth           => 8,
      c_LogEnable          => 0, 
      c_UsrFlagWidth       => 16, 
      c_Cmd_rep            => 0,
      c_CombineAxiBurst    => 1, 
      c_DDRAxiBurstRatio   => 2,
      c_ResetLevel         => '1',
      c_ResetAsync         => 0
	  
	  )
   port map(
      -- Clock and reset
      sRst                 => Wr0sRst,
      Clk                  => Wr0Clk,
      -- Write address channel
      U_AWID               => Wr0_DW_AWID,
      U_AWADDR             => Wr0_DW_AWADDR,
      U_AWLEN              => Wr0_DW_AWLEN,
      U_AWSIZE             => Wr0_DW_AWSIZE,
      U_AWBURST            => Wr0_DW_AWBURST,
      U_AWVALID            => Wr0_DW_AWVALID,
      U_AWREADY            => Wr0_DW_AWREADY,
      -- Write data channel
      U_WID                => Wr0_DW_WID,
      U_WDATA              => Wr0_DW_WDATA,
      U_WSTRB              => Wr0_DW_WSTRB,
      U_WLAST              => Wr0_DW_WLAST,
      U_WVALID             => Wr0_DW_WVALID,
      U_WREADY             => Wr0_DW_WREADY,
      -- Write response channel
      U_BID                => Wr0_DW_BID,
      U_BRESP              => Wr0_DW_BRESP,
      U_BVALID             => Wr0_DW_BVALID,
      U_BREADY             => Wr0_DW_BREADY,
      -- Input channel
      M_Addr               => Wr0_BW_Addr,
      M_WrData             => Wr0_BW_WrData,
      M_WrMask             => Wr0_BW_WrMask,
      M_Strobe             => Wr0_BW_Strobe,
      M_Wait               => Wr0_BW_Wait,
      -- Control signals
      M_EndOfBurst         => Wr0_BW_EndOfBurst,
      M_WrAck              => Wr0_BW_WrAck,
      M_WrAckWait          => Wr0_BW_WrAckWait,
      M_WrPending          => Wr0_BW_WrPending,
      M_CmdPending         => Wr0_BW_CmdPending,
      M_AddrDiff           => Wr0_BW_AddrDiff
   );
   
   i_Wr0_BurstGenWr : BurstGenWr
   generic map(
      c_PortName           => "Wr0",
      c_BurstLength        => 8,
      c_MBurstLength       => 1,
      c_UBurstLength       => 4,
      c_MaxNbCmd           => 256,
      c_MaxBurstGrp        => 4,
	  c_OptBurstGrp        => 4,
      c_NbSeq              => 1,
      c_SmallFifoDepth     => 64,
      c_AlmostFullLevel    => 0,
      c_MultFreq           => 1,
      c_UDataWidth         => 128,
      c_UAddrWidth         => 27,
      c_MDataWidth         => 512,
      c_ReplicateFract     => 1,
      c_MAddrWidth         => 25,
      c_IAddrWidth         => 22,
	  c_BankWidth          => 3,
      c_NbBank             => 8,
      c_NbFifo             => 8,
      c_WrPipe             => 1, 
      c_ByteSize           => 8, 
      c_DualClockUM        => 1,
      c_Incr               => 0,
      c_WrAck              => 1,
      c_UsrWrPipe          => 1,
      c_NbChipSelect       => 1, 
      c_RowWidth           => 15,
      c_ColWidth           => 10,
      c_XorBank            => 1,
      c_LogEnable          => 0,
      c_WaitFast           => 0,
      c_Cmd_Rep            => 0,
      c_DualPhy            => 0,
      c_DbgMon             => True,
      c_ResetLevel         => '1',
      c_ResetAsync         => 0
   )
   port map(
      -- Map Config
      MAP_ShiftCol         => MAP_ShiftCol,
      MAP_ShiftRow         => MAP_ShiftRow,
      MAP_ShiftCs          => MAP_ShiftCs,
      -- Clock and reset
      U_sRst               => Wr0sRst,
      U_Clk                => Wr0Clk,
      -- Input channel
      U_Addr               => Wr0_BW_Addr,
      U_WrData             => Wr0_BW_WrData,
      U_WrMask             => Wr0_BW_WrMask,
      U_Strobe             => Wr0_BW_Strobe,
      U_Wait               => Wr0_BW_Wait,
      -- Control signals
      U_EndOfBurst         => Wr0_BW_EndOfBurst,
      U_WrAck              => Wr0_BW_WrAck,
      U_WrAckWait          => Wr0_BW_WrAckWait,
      U_WrPending          => Wr0_BW_WrPending,
      U_CmdPending         => Wr0_BW_CmdPending,
      U_AddrDiff           => Wr0_BW_AddrDiff,
      -- Clock and reset
      M_sRst               => sRst,
      M_Clk                => Clk,
      -- Command Strobe channel
      M_CmdStrobe          => Wr0_RW_CmdStrobe,
      M_CmdFlag            => Wr0_RW_CmdFlag,
      M_CmdIndex           => Wr0_RW_CmdIndex,
      M_CmdWait            => Wr0_RW_CmdWait,
      -- Command Get channel
      M_CmdGet             => Wr0_RW_CmdGet,
      M_CmdGetIndex        => Wr0_RW_CmdGetIndex,
      M_CmdGetAddr         => Wr0_RW_CmdGetAddr,
      -- Write data channel
      M_WrData             => Wr0_RW_Data,
      M_WrMask             => Wr0_RW_Mask,
      M_WrGetData          => Wr0_RW_GetData,
      M_WrDataIndex        => Wr0_RW_DataIndex
   );
 
-- WRITE COMMAND STROBE CHANNEL------------------------------------------------------------------
   WrCmdStrobe(7 downto 0) <= Wr0_RW_CmdStrobe;
   WrCmdFlag(7 downto 0) <= Wr0_RW_CmdFlag;
   WrCmdIndex(63 downto 0) <= extSig(Wr0_RW_CmdIndex,8,8,8);
   Wr0_RW_CmdWait       <= WrCmdWait(7 downto 0);
 
      -- WRITE COMMAND GET CHANNEL----------------------------------------------------------
   Wr0_RW_CmdGet        <= WrCmdGet(0 downto 0);
   Wr0_RW_CmdGetIndex   <= WrCmdGetIndex(7 downto 0);
   WrCmdGetAddr(21 downto 0) <= Wr0_RW_CmdGetAddr;
   
   -- WRITE DATA CHANNEL-----------------------------------------------------------------
   WrData(511 downto 0) <= Wr0_RW_Data;
   WrMask(63 downto 0)  <= Wr0_RW_Mask;
   Wr0_RW_DataIndex     <= WrDataIndex(7 downto 0);
   Wr0_RW_GetData       <= WrGetData(0 downto 0);
   -- Connection to ports ------------------------------------------------
   -- Write address channel
   Wr1_DW_AWID          <= Wr1AWID;
   Wr1_DW_AWADDR        <= Wr1AWADDR;
   Wr1_DW_AWLEN         <= Wr1AWLEN;
   Wr1_DW_AWSIZE        <= Wr1AWSIZE;
   Wr1_DW_AWBURST       <= Wr1AWBURST;
   Wr1_DW_AWVALID       <= Wr1AWVALID;
   Wr1AWREADY           <= Wr1_DW_AWREADY;
   -- Write data channel
   Wr1_DW_WID           <= Wr1WID;
   Wr1_DW_WDATA         <= Wr1WDATA;
   Wr1_DW_WSTRB         <= Wr1WSTRB;
   Wr1_DW_WLAST         <= Wr1WLAST;
   Wr1_DW_WVALID        <= Wr1WVALID;
   Wr1WREADY            <= Wr1_DW_WREADY;
   -- Write response channel
   Wr1BID               <= Wr1_DW_BID;
   Wr1BRESP             <= Wr1_DW_BRESP;
   Wr1BVALID            <= Wr1_DW_BVALID;
   Wr1_DW_BREADY        <= Wr1BREADY;
   
   i_Wr1_AxiWrPort : AxiWrPort
   generic map(
      c_PortName           => "Wr1",
      c_MaxNbCmd           => 256, 
      c_DataWidth          => 128,
      c_AddrWidth          => 27,
      c_LenWidth           => 8,
      c_LogEnable          => 0, 
      c_UsrFlagWidth       => 16,
      c_Cmd_rep            => 0,
      c_CombineAxiBurst    => 1,
      c_DDRAxiBurstRatio   => 2,
      c_ResetLevel         => '1',
      c_ResetAsync         => 0
   )
   port map(
      -- Clock and reset
      sRst                 => Wr1sRst,
      Clk                  => Wr1Clk,
      -- Write address channel
      U_AWID               => Wr1_DW_AWID,
      U_AWADDR             => Wr1_DW_AWADDR,
      U_AWLEN              => Wr1_DW_AWLEN,
      U_AWSIZE             => Wr1_DW_AWSIZE,
      U_AWBURST            => Wr1_DW_AWBURST,
      U_AWVALID            => Wr1_DW_AWVALID,
      U_AWREADY            => Wr1_DW_AWREADY,
      -- Write data channel
      U_WID                => Wr1_DW_WID,
      U_WDATA              => Wr1_DW_WDATA,
      U_WSTRB              => Wr1_DW_WSTRB,
      U_WLAST              => Wr1_DW_WLAST,
      U_WVALID             => Wr1_DW_WVALID,
      U_WREADY             => Wr1_DW_WREADY,
      -- Write response channel
      U_BID                => Wr1_DW_BID,
      U_BRESP              => Wr1_DW_BRESP,
      U_BVALID             => Wr1_DW_BVALID,
      U_BREADY             => Wr1_DW_BREADY,
      -- Input channel
      M_Addr               => Wr1_BW_Addr,
      M_WrData             => Wr1_BW_WrData,
      M_WrMask             => Wr1_BW_WrMask,
      M_Strobe             => Wr1_BW_Strobe,
      M_Wait               => Wr1_BW_Wait,
      -- Control signals
      M_EndOfBurst         => Wr1_BW_EndOfBurst,
      M_WrAck              => Wr1_BW_WrAck,
      M_WrAckWait          => Wr1_BW_WrAckWait,
      M_WrPending          => Wr1_BW_WrPending,
      M_CmdPending         => Wr1_BW_CmdPending,
      M_AddrDiff           => Wr1_BW_AddrDiff
   );
   
   i_Wr1_BurstGenWr : BurstGenWr
   generic map(
      c_PortName           => "Wr1",
      c_BurstLength        => 8,
      c_MBurstLength       => 1,
      c_UBurstLength       => 4, 
      c_MaxNbCmd           => 256,
      c_MaxBurstGrp        => 4,
	  c_OptBurstGrp        => 4,
      c_NbSeq              => 1, 
      c_SmallFifoDepth     => 64, 
      c_AlmostFullLevel    => 0,
      c_MultFreq           => 1,
      c_UDataWidth         => 128,
      c_UAddrWidth         => 27,
      c_MDataWidth         => 512, 
      c_ReplicateFract     => 1,
      c_MAddrWidth         => 25,
      c_IAddrWidth         => 22, 
	  c_BankWidth          => 3,
      c_NbBank             => 8,
      c_NbFifo             => 8,
      c_WrPipe             => 1, 
      c_ByteSize           => 8,
      c_DualClockUM        => 1,
      c_Incr               => 0, 
      c_WrAck              => 1, 
      c_UsrWrPipe          => 1, 
      c_NbChipSelect       => 1, 
      c_RowWidth           => 15, 
      c_ColWidth           => 10, 
      c_XorBank            => 1, 
      c_LogEnable          => 0, 
      c_WaitFast           => 0, 
      c_Cmd_Rep            => 0, 
      c_DualPhy            => 0,  
      c_DbgMon             => True,
      c_ResetLevel         => '1', 
      c_ResetAsync         => 0 
   )
   port map(
      -- Map Config
      MAP_ShiftCol         => MAP_ShiftCol,
      MAP_ShiftRow         => MAP_ShiftRow,
      MAP_ShiftCs          => MAP_ShiftCs,
      -- Clock and reset
      U_sRst               => Wr1sRst,
      U_Clk                => Wr1Clk,
      -- Input channel
      U_Addr               => Wr1_BW_Addr,
      U_WrData             => Wr1_BW_WrData,
      U_WrMask             => Wr1_BW_WrMask,
      U_Strobe             => Wr1_BW_Strobe,
      U_Wait               => Wr1_BW_Wait,
      -- Control signals
      U_EndOfBurst         => Wr1_BW_EndOfBurst,
      U_WrAck              => Wr1_BW_WrAck,
      U_WrAckWait          => Wr1_BW_WrAckWait,
      U_WrPending          => Wr1_BW_WrPending,
      U_CmdPending         => Wr1_BW_CmdPending,
      U_AddrDiff           => Wr1_BW_AddrDiff,
      -- Clock and reset
      M_sRst               => sRst,
      M_Clk                => Clk,
      -- Command Strobe channel
      M_CmdStrobe          => Wr1_RW_CmdStrobe,
      M_CmdFlag            => Wr1_RW_CmdFlag,
      M_CmdIndex           => Wr1_RW_CmdIndex,
      M_CmdWait            => Wr1_RW_CmdWait,
      -- Command Get channel
      M_CmdGet             => Wr1_RW_CmdGet,
      M_CmdGetIndex        => Wr1_RW_CmdGetIndex,
      M_CmdGetAddr         => Wr1_RW_CmdGetAddr,
      -- Write data channel
      M_WrData             => Wr1_RW_Data,
      M_WrMask             => Wr1_RW_Mask,
      M_WrGetData          => Wr1_RW_GetData,
      M_WrDataIndex        => Wr1_RW_DataIndex
   );
-- CONNECTION TO CORE  ------------------------------------------------
 
-- WRITE COMMAND STROBE CHANNEL------------------------------------------------------------------
   WrCmdStrobe(15 downto 8) <= Wr1_RW_CmdStrobe;
   WrCmdFlag(15 downto 8) <= Wr1_RW_CmdFlag;
   WrCmdIndex(127 downto 64) <= extSig(Wr1_RW_CmdIndex,8,8,8);
   Wr1_RW_CmdWait       <= WrCmdWait(15 downto 8);
  
      -- Write command get channel
   Wr1_RW_CmdGet        <= WrCmdGet(1 downto 1);
   Wr1_RW_CmdGetIndex   <= WrCmdGetIndex(7 downto 0);
   WrCmdGetAddr(43 downto 22) <= Wr1_RW_CmdGetAddr;
 
   -- Write data channel
   WrData(1023 downto 512) <= Wr1_RW_Data;
   WrMask(127 downto 64)  <= Wr1_RW_Mask;
   Wr1_RW_DataIndex     <= WrDataIndex(7 downto 0);
   Wr1_RW_GetData       <= WrGetData(1 downto 1);
   
--   -- Connection to ports ------------------------------------------------
--   -- Write address channel
   Wr4_DW_AWID          <= Wr4AWID;
   Wr4_DW_AWADDR        <= Wr4AWADDR;
   Wr4_DW_AWLEN         <= Wr4AWLEN;
   Wr4_DW_AWSIZE        <= Wr4AWSIZE;
   Wr4_DW_AWBURST       <= Wr4AWBURST;
   Wr4_DW_AWVALID       <= Wr4AWVALID;
   Wr4AWREADY           <= Wr4_DW_AWREADY;
   -- Write data channel
   Wr4_DW_WID           <= Wr4WID;
   Wr4_DW_WDATA         <= Wr4WDATA;
   Wr4_DW_WSTRB         <= Wr4WSTRB;
   Wr4_DW_WLAST         <= Wr4WLAST;
   Wr4_DW_WVALID        <= Wr4WVALID;
   Wr4WREADY            <= Wr4_DW_WREADY;
   -- Write response channel
   Wr4BID               <= Wr4_DW_BID;
   Wr4BRESP             <= Wr4_DW_BRESP;
   Wr4BVALID            <= Wr4_DW_BVALID;
   Wr4_DW_BREADY        <= Wr4BREADY;
   
   i_Wr4_AxiWrPort : AxiWrPort
   generic map(
      c_PortName           => "Wr4", 
      c_MaxNbCmd           => 256,
      c_DataWidth          => 128,
      c_AddrWidth          => 27,
      c_LenWidth           => 8, 
      c_LogEnable          => 0,
      c_UsrFlagWidth       => 16, 
      c_Cmd_rep            => 0, 
      c_CombineAxiBurst    => 1,
      c_DDRAxiBurstRatio   => 2, 
      c_ResetLevel         => '1', 
      c_ResetAsync         => 0 
     )
   port map(
      -- Clock and reset
      sRst                 => Wr4sRst,
      Clk                  => Wr4Clk,
      -- Write address channel
      U_AWID               => Wr4_DW_AWID,
      U_AWADDR             => Wr4_DW_AWADDR,
      U_AWLEN              => Wr4_DW_AWLEN,
      U_AWSIZE             => Wr4_DW_AWSIZE,
      U_AWBURST            => Wr4_DW_AWBURST,
      U_AWVALID            => Wr4_DW_AWVALID,
      U_AWREADY            => Wr4_DW_AWREADY,
      -- Write data channel
      U_WID                => Wr4_DW_WID,
      U_WDATA              => Wr4_DW_WDATA,
      U_WSTRB              => Wr4_DW_WSTRB,
      U_WLAST              => Wr4_DW_WLAST,
      U_WVALID             => Wr4_DW_WVALID,
      U_WREADY             => Wr4_DW_WREADY,
      -- Write response channel
      U_BID                => Wr4_DW_BID,
      U_BRESP              => Wr4_DW_BRESP,
      U_BVALID             => Wr4_DW_BVALID,
      U_BREADY             => Wr4_DW_BREADY,
      -- Input channel
      M_Addr               => Wr4_BW_Addr,
      M_WrData             => Wr4_BW_WrData,
      M_WrMask             => Wr4_BW_WrMask,
      M_Strobe             => Wr4_BW_Strobe,
      M_Wait               => Wr4_BW_Wait,
      -- Control signals
      M_EndOfBurst         => Wr4_BW_EndOfBurst,
      M_WrAck              => Wr4_BW_WrAck,
      M_WrAckWait          => Wr4_BW_WrAckWait,
      M_WrPending          => Wr4_BW_WrPending,
      M_CmdPending         => Wr4_BW_CmdPending,
      M_AddrDiff           => Wr4_BW_AddrDiff
   );
   
   i_Wr4_BurstGenWr : BurstGenWr
   generic map(
      c_PortName           => "Wr4",
      c_BurstLength        => 8, 
      c_MBurstLength       => 1,
      c_UBurstLength       => 4, 
      c_MaxNbCmd           => 256, 
      c_MaxBurstGrp        => 4, 
	  c_OptBurstGrp        => 4,
      c_NbSeq              => 1,
      c_SmallFifoDepth     => 64, 
      c_AlmostFullLevel    => 0, 
      c_MultFreq           => 1, 
      c_UDataWidth         => 128, 
      c_UAddrWidth         => 27,
      c_MDataWidth         => 512, 
      c_ReplicateFract     => 1,
      c_MAddrWidth         => 25,
      c_IAddrWidth         => 22, 
	  c_BankWidth          => 3,
      c_NbBank             => 8,
      c_NbFifo             => 8,
      c_WrPipe             => 1, 
      c_ByteSize           => 8,
      c_DualClockUM        => 1,
      c_Incr               => 0, 
      c_WrAck              => 1, 
      c_UsrWrPipe          => 1, 
      c_NbChipSelect       => 1, 
      c_RowWidth           => 15, 
      c_ColWidth           => 10, 
      c_XorBank            => 1, 
      c_LogEnable          => 0, 
      c_WaitFast           => 0, 
      c_Cmd_Rep            => 0, 
      c_DualPhy            => 0, 
      c_DbgMon             => False, 
      c_ResetLevel         => '1', 
      c_ResetAsync         => 0 
   )
   port map(
      -- Map Config
      MAP_ShiftCol         => MAP_ShiftCol,
      MAP_ShiftRow         => MAP_ShiftRow,
      MAP_ShiftCs          => MAP_ShiftCs,
      -- Clock and reset
      U_sRst               => Wr4sRst,
      U_Clk                => Wr4Clk,
      -- Input channel
      U_Addr               => Wr4_BW_Addr,
      U_WrData             => Wr4_BW_WrData,
      U_WrMask             => Wr4_BW_WrMask,
      U_Strobe             => Wr4_BW_Strobe,
      U_Wait               => Wr4_BW_Wait,
      -- Control signals
      U_EndOfBurst         => Wr4_BW_EndOfBurst,
      U_WrAck              => Wr4_BW_WrAck,
      U_WrAckWait          => Wr4_BW_WrAckWait,
      U_WrPending          => Wr4_BW_WrPending,
      U_CmdPending         => Wr4_BW_CmdPending,
      U_AddrDiff           => Wr4_BW_AddrDiff,
      -- Clock and reset
      M_sRst               => sRst,
      M_Clk                => Clk,
      -- Command Strobe channel
      M_CmdStrobe          => Wr4_RW_CmdStrobe,
      M_CmdFlag            => Wr4_RW_CmdFlag,
      M_CmdIndex           => Wr4_RW_CmdIndex,
      M_CmdWait            => Wr4_RW_CmdWait,
      -- Command Get channel
      M_CmdGet             => Wr4_RW_CmdGet,
      M_CmdGetIndex        => Wr4_RW_CmdGetIndex,
      M_CmdGetAddr         => Wr4_RW_CmdGetAddr,
      -- Write data channel
      M_WrData             => Wr4_RW_Data,
      M_WrMask             => Wr4_RW_Mask,
      M_WrGetData          => Wr4_RW_GetData,
      M_WrDataIndex        => Wr4_RW_DataIndex
   );
-- CONNECTION TO CORE  ------------------------------------------------
   -- Connection to core  ------------------------------------------------
   -- Write command strobe channel
   WrCmdStrobe(39 downto 32) <= Wr4_RW_CmdStrobe;
   WrCmdFlag(39 downto 32) <= Wr4_RW_CmdFlag;
   WrCmdIndex(319 downto 256) <= extSig(Wr4_RW_CmdIndex,8,8,8);
   Wr4_RW_CmdWait       <= WrCmdWait(39 downto 32);
   -- Write command get channel
   Wr4_RW_CmdGet        <= WrCmdGet(4 downto 4);
   Wr4_RW_CmdGetIndex   <= WrCmdGetIndex(7 downto 0);
   WrCmdGetAddr(109 downto 88) <= Wr4_RW_CmdGetAddr;
   -- Write data channel
   WrData(2559 downto 2048) <= Wr4_RW_Data;
   WrMask(319 downto 256) <= Wr4_RW_Mask;
   Wr4_RW_DataIndex     <= WrDataIndex(7 downto 0);
   Wr4_RW_GetData       <= WrGetData(4 downto 4);
--  
-- WRITE COMMAND STROBE CHANNEL------------------------------------------------------------------
 --  WrCmdStrobe(23 downto 16) <= Wr4_RW_CmdStrobe;
 --  WrCmdFlag(23 downto 16) <= Wr4_RW_CmdFlag;
 --  WrCmdIndex(191 downto 128) <= extSig(Wr4_RW_CmdIndex,8,8,8);
 --  Wr4_RW_CmdWait       <= WrCmdWait(23 downto 16);
-- 
 --    -- WRITE COMMAND GET CHANNEL------------------------------------------------------------------------------------------
 --  Wr4_RW_CmdGet        <= WrCmdGet(2 downto 2);
 --  Wr4_RW_CmdGetIndex   <= WrCmdGetIndex(7 downto 0);
--
  -- WrCmdGetAddr(65 downto 44) <= Wr4_RW_CmdGetAddr;
-- 
   -- WRITE DATA CHANNEL--------------------------------------------------------------------------------------
  -- WrData(1535 downto 1024) <= Wr4_RW_Data;
  -- WrMask(191 downto 128) <= Wr4_RW_Mask;
  -- Wr4_RW_DataIndex     <= WrDataIndex(7 downto 0);
  -- Wr4_RW_GetData       <= WrGetData(2 downto 2);
   -- Connection to ports ------------------------------------------------
   -- Read address channel
   Rd0_DR_ARID          <= Rd0ARID;
   Rd0_DR_ARADDR        <= Rd0ARADDR;
   Rd0_DR_ARLEN         <= Rd0ARLEN;
   Rd0_DR_ARSIZE        <= Rd0ARSIZE;
   Rd0_DR_ARBURST       <= Rd0ARBURST;
   Rd0_DR_ARVALID       <= Rd0ARVALID;
   Rd0ARREADY           <= Rd0_DR_ARREADY;
   -- Read data channel
   Rd0RID               <= Rd0_DR_RID;
   Rd0RDATA             <= Rd0_DR_RDATA;
   Rd0RRESP             <= Rd0_DR_RRESP;
   Rd0RLAST             <= Rd0_DR_RLAST;
   Rd0RVALID            <= Rd0_DR_RVALID;
   Rd0_DR_RREADY        <= Rd0RREADY;
   
   i_Rd0_AxiRdPort : AxiRdPort
   generic map(
      c_PortName           => "Rd0", --NC  user 
      c_DataWidth          => 128, 
      c_AddrWidth          => 27, 
      c_LenWidth           => 8,
      c_LogEnable          => 0, 
      c_UsrFlagWidth       => 16, 
      c_CombineAxiBurst    => 1, 
      c_ResetLevel         => '1',
      c_ResetAsync         => 0
      )
   port map(
      -- Clock and reset
      sRst                 => Rd0sRst,
      Clk                  => Rd0Clk,
      -- Read address channel
      U_ARID               => Rd0_DR_ARID,
      U_ARADDR             => Rd0_DR_ARADDR,
      U_ARLEN              => Rd0_DR_ARLEN,
      U_ARSIZE             => Rd0_DR_ARSIZE,
      U_ARBURST            => Rd0_DR_ARBURST,
      U_ARVALID            => Rd0_DR_ARVALID,
      U_ARREADY            => Rd0_DR_ARREADY,
      -- Read data channel
      U_RID                => Rd0_DR_RID,
      U_RDATA              => Rd0_DR_RDATA,
      U_RRESP              => Rd0_DR_RRESP,
      U_RLAST              => Rd0_DR_RLAST,
      U_RVALID             => Rd0_DR_RVALID,
      U_RREADY             => Rd0_DR_RREADY,
      -- Input channel
      M_Addr               => Rd0_BR_Addr,
      M_Len                => Rd0_BR_Len,
      M_UsrFlagIn          => Rd0_BR_UsrFlagIn,
      M_Strobe             => Rd0_BR_Strobe,
      M_Wait               => Rd0_BR_Wait,
      -- Control signals
      M_EndOfBurst         => Rd0_BR_EndOfBurst,
      M_RdPending          => Rd0_BR_RdPending,
      M_CmdPending         => Rd0_BR_CmdPending,
      -- Output channel
      M_UsrFlagOut         => Rd0_BR_UsrFlagOut,
      M_RdData             => Rd0_BR_RdData,
      M_RdDataStrobe       => Rd0_BR_RdDataStrobe,
      M_RdDataWait         => Rd0_BR_RdDataWait
   );
   
   i_Rd0_BurstGenRd : BurstGenRd
   generic map(
      c_PortName           => "Rd0",
      c_UDataWidth         => 128, 
      c_ULenWidth          => 8,
      c_UAddrWidth         => 27,
      c_MDataWidth         => 512, 
      c_MAddrWidth         => 25, 
      c_IAddrWidth         => 22, 
	  c_BankWidth          => 3,
      c_NbBank             => 8,
      c_NbFifo             => 8,
      c_RdPipe             => 0, 
      c_BurstLength        => 8,
      c_MBurstLength       => 1,
      c_UBurstLength       => 4, 
      c_MaxNbCmd           => 256,
      c_MaxBurstGrp        => 4,
	  c_OptBurstGrp        => 4, 
      c_NbSeq              => 1, 
      c_ThrottlePort       => 1,  
      c_SmallFifoDepth     => 64,
      c_MultFreq           => 1, 
      c_MFWrPipe           => 0,
      c_RegCmdAddrIncr     => 0, 
      c_LsbFifoDepth       => 1024,
      c_DualClockUM        => 1, 
      c_DualClockRU        => 1, 
      c_Incr               => 0, 
      c_UsrFlagWidth       => 17, 
      c_NbChipSelect       => 1, 
      c_RowWidth           => 15,
      c_ColWidth           => 10, 
      c_XorBank            => 1, 
      c_LogEnable          => 0, 
      c_Cmd_Rep            => 0, 
      c_DualPhy            => 0, 
      c_DbgMon             => True,
      c_ResetLevel         => '1',
      c_ResetAsync         => 0, 
      c_PerRd              => 1 
   )
   port map(
      -- Map Config
      MAP_ShiftCol         => MAP_ShiftCol,
      MAP_ShiftRow         => MAP_ShiftRow,
      MAP_ShiftCs          => MAP_ShiftCs,
      -- Clock and reset
      U_sRst               => Rd0sRst,
      U_Clk                => Rd0Clk,
      -- Input channel
      U_Addr               => Rd0_BR_Addr,
      U_Len                => Rd0_BR_Len,
      U_UsrFlagIn          => Rd0_BR_UsrFlagIn,
      U_Strobe             => Rd0_BR_Strobe,
      U_Wait               => Rd0_BR_Wait,
      -- Control signals
      U_EndOfBurst         => Rd0_BR_EndOfBurst,
      U_RdPending          => Rd0_BR_RdPending,
      U_CmdPending         => Rd0_BR_CmdPending,
      -- Output channel
      U_UsrFlagOut         => Rd0_BR_UsrFlagOut,
      U_RdData             => Rd0_BR_RdData,
      U_RdDataStrobe       => Rd0_BR_RdDataStrobe,
      U_RdDataWait         => Rd0_BR_RdDataWait,
      -- Clock and reset
      M_sRst               => sRst,
      M_Clk                => Clk,
      -- Command Strobe channel
      M_CmdStrobe          => Rd0_RR_CmdStrobe,
      M_CmdFlag            => Rd0_RR_CmdFlag,
      M_CmdIndex           => Rd0_RR_CmdIndex,
      M_CmdWait            => Rd0_RR_CmdWait,
      -- Command Get channel
      M_CmdGet             => Rd0_RR_CmdGet,
      M_CmdGetIndex        => Rd0_RR_CmdGetIndex,
      M_CmdGetAddr         => Rd0_RR_CmdGetAddr,
      M_PortDummyRead      => Rd0_RR_PortDummyRead,
      -- Clock and reset
      R_sRst               => sRst,
      R_Clk                => Clk,
      -- Read data channel
      R_RdData             => Rd0_RR_Data,
      R_RdDataPut          => Rd0_RR_DataPut,
      R_RdDataIndex        => Rd0_RR_DataIndex
   );
 -- READ 0:
   
-- READ COMMAND STROBE CHANNEL ---------------------------------------------------------
   RdCmdStrobe(7 downto 0) <= Rd0_RR_CmdStrobe;
   RdCmdFlag(7 downto 0) <= Rd0_RR_CmdFlag;
   RdCmdIndex(63 downto 0) <= extSig(Rd0_RR_CmdIndex,8,8,8);
   Rd0_RR_CmdWait       <= RdCmdWait(7 downto 0);
    
   Rd0_RR_PortDummyRead(0) <= PortDummyRead;
   -- READ COMMAND GET CHANNEL----------------------------------------------
   Rd0_RR_CmdGet        <= RdCmdGet(0 downto 0);
   Rd0_RR_CmdGetIndex   <= RdCmdGetIndex(7 downto 0);
   RdCmdGetAddr(21 downto 0) <= Rd0_RR_CmdGetAddr;
  
   -- READ DATA CHANNEL---------------------------------------------------
   Rd0_RR_Data          <= RdData(511 downto 0);
   Rd0_RR_DataPut       <= RdDataPut(0 downto 0);
   Rd0_RR_DataIndex     <= RdDataIndex(7 downto 0); 
   -- Connection to ports ------------------------------------------------
   -- Read address channel
   Rd1_DR_ARID          <= Rd1ARID;
   Rd1_DR_ARADDR        <= Rd1ARADDR;
   Rd1_DR_ARLEN         <= Rd1ARLEN;
   Rd1_DR_ARSIZE        <= Rd1ARSIZE;
   Rd1_DR_ARBURST       <= Rd1ARBURST;
   Rd1_DR_ARVALID       <= Rd1ARVALID;
   Rd1ARREADY           <= Rd1_DR_ARREADY;
   -- Read data channel
   Rd1RID               <= Rd1_DR_RID;
   Rd1RDATA             <= Rd1_DR_RDATA;
   Rd1RRESP             <= Rd1_DR_RRESP;
   Rd1RLAST             <= Rd1_DR_RLAST;
   Rd1RVALID            <= Rd1_DR_RVALID;
   Rd1_DR_RREADY        <= Rd1RREADY;
   
   i_Rd1_AxiRdPort : AxiRdPort
   generic map(
      c_PortName           => "Rd1", 
      c_DataWidth          => 128, 
      c_AddrWidth          => 27, 
      c_LenWidth           => 8, 
      c_LogEnable          => 0,
      c_UsrFlagWidth       => 16,
      c_CombineAxiBurst    => 1, 
      c_ResetLevel         => '1', 
      c_ResetAsync         => 0 
        )
   port map(
      -- Clock and reset
      sRst                 => Rd1sRst,
      Clk                  => Rd1Clk,
      -- Read address channel
      U_ARID               => Rd1_DR_ARID,
      U_ARADDR             => Rd1_DR_ARADDR,
      U_ARLEN              => Rd1_DR_ARLEN,
      U_ARSIZE             => Rd1_DR_ARSIZE,
      U_ARBURST            => Rd1_DR_ARBURST,
      U_ARVALID            => Rd1_DR_ARVALID,
      U_ARREADY            => Rd1_DR_ARREADY,
      -- Read data channel
      U_RID                => Rd1_DR_RID,
      U_RDATA              => Rd1_DR_RDATA,
      U_RRESP              => Rd1_DR_RRESP,
      U_RLAST              => Rd1_DR_RLAST,
      U_RVALID             => Rd1_DR_RVALID,
      U_RREADY             => Rd1_DR_RREADY,
      -- Input channel
      M_Addr               => Rd1_BR_Addr,
      M_Len                => Rd1_BR_Len,
      M_UsrFlagIn          => Rd1_BR_UsrFlagIn,
      M_Strobe             => Rd1_BR_Strobe,
      M_Wait               => Rd1_BR_Wait,
      -- Control signals
      M_EndOfBurst         => Rd1_BR_EndOfBurst,
      M_RdPending          => Rd1_BR_RdPending,
      M_CmdPending         => Rd1_BR_CmdPending,
      -- Output channel
      M_UsrFlagOut         => Rd1_BR_UsrFlagOut,
      M_RdData             => Rd1_BR_RdData,
      M_RdDataStrobe       => Rd1_BR_RdDataStrobe,
      M_RdDataWait         => Rd1_BR_RdDataWait
   );
   
   i_Rd1_BurstGenRd : BurstGenRd
   generic map(
      c_PortName           => "Rd1",
      c_UDataWidth         => 128, 
      c_ULenWidth          => 8, 
      c_UAddrWidth         => 27,
      c_MDataWidth         => 512, 
      c_MAddrWidth         => 25, 
      c_IAddrWidth         => 22, 
	  c_BankWidth          => 3,
      c_NbBank             => 8,
      c_NbFifo             => 8,
      c_RdPipe             => 0,
      c_BurstLength        => 8, 
      c_MBurstLength       => 1,
      c_UBurstLength       => 4,
      c_MaxNbCmd           => 256, 
      c_MaxBurstGrp        => 4,
	  c_OptBurstGrp        => 4, 
      c_NbSeq              => 1,
      c_ThrottlePort       => 1, 
      c_SmallFifoDepth     => 64, 
      c_MultFreq           => 1, -- 1, 
      c_MFWrPipe           => 0, 
      c_RegCmdAddrIncr     => 0,
      c_LsbFifoDepth       => 1024, 
      c_DualClockUM        => 1, 
      c_DualClockRU        => 1, 
      c_Incr               => 0,
      c_UsrFlagWidth       => 17, 
      c_NbChipSelect       => 1,
      c_RowWidth           => 15, 
      c_ColWidth           => 10, 
      c_XorBank            => 1, 
      c_LogEnable          => 0, 
      c_Cmd_Rep            => 0, 
      c_DualPhy            => 0,  
      c_DbgMon             => True, 
      c_ResetLevel         => '1', 
      c_ResetAsync         => 0 
   )
   port map(
      -- Map Config
      MAP_ShiftCol         => MAP_ShiftCol,
      MAP_ShiftRow         => MAP_ShiftRow,
      MAP_ShiftCs          => MAP_ShiftCs,
      -- Clock and reset
      U_sRst               => Rd1sRst,
      U_Clk                => Rd1Clk,
      -- Input channel
      U_Addr               => Rd1_BR_Addr,
      U_Len                => Rd1_BR_Len,
      U_UsrFlagIn          => Rd1_BR_UsrFlagIn,
      U_Strobe             => Rd1_BR_Strobe,
      U_Wait               => Rd1_BR_Wait,
      -- Control signals
      U_EndOfBurst         => Rd1_BR_EndOfBurst,
      U_RdPending          => Rd1_BR_RdPending,
      U_CmdPending         => Rd1_BR_CmdPending,
      -- Output channel
      U_UsrFlagOut         => Rd1_BR_UsrFlagOut,
      U_RdData             => Rd1_BR_RdData,
      U_RdDataStrobe       => Rd1_BR_RdDataStrobe,
      U_RdDataWait         => Rd1_BR_RdDataWait,
      -- Clock and reset
      M_sRst               => sRst,
      M_Clk                => Clk,
      -- Command Strobe channel
      M_CmdStrobe          => Rd1_RR_CmdStrobe,
      M_CmdFlag            => Rd1_RR_CmdFlag,
      M_CmdIndex           => Rd1_RR_CmdIndex,
      M_CmdWait            => Rd1_RR_CmdWait,
      -- Command Get channel
      M_CmdGet             => Rd1_RR_CmdGet,
      M_CmdGetIndex        => Rd1_RR_CmdGetIndex,
      M_CmdGetAddr         => Rd1_RR_CmdGetAddr,
      -- Clock and reset
      R_sRst               => sRst,
      R_Clk                => Clk,
      -- Read data channel
      R_RdData             => Rd1_RR_Data,
      R_RdDataPut          => Rd1_RR_DataPut,
      R_RdDataIndex        => Rd1_RR_DataIndex
   );
 -- READ 1:
   
-- READ COMMAND STROBE CHANNEL ---------------------------------------------------------
   RdCmdStrobe(15 downto 8) <= Rd1_RR_CmdStrobe;
   RdCmdFlag(15 downto 8) <= Rd1_RR_CmdFlag;
   RdCmdIndex(127 downto 64) <= extSig(Rd1_RR_CmdIndex,8,8,8);
   Rd1_RR_CmdWait       <= RdCmdWait(15 downto 8);

  
   -- READ COMMAND GET CHANNEL------------------------------------------------------------------------------------
   Rd1_RR_CmdGet        <= RdCmdGet(1 downto 1);
   Rd1_RR_CmdGetIndex   <= RdCmdGetIndex(7 downto 0);
   RdCmdGetAddr(43 downto 22) <= Rd1_RR_CmdGetAddr;
   -- READ DATA CHANNEL-------------------------------------------------------------------------------------------
   Rd1_RR_Data          <= RdData(511 downto 0);
   Rd1_RR_DataPut       <= RdDataPut(1 downto 1);
   Rd1_RR_DataIndex     <= RdDataIndex(7 downto 0);
   
   -- 128b AXI read port
   ------------------------------------------------------------------------
   -- Connection to ports ------------------------------------------------
   -- Read address channel
   Rd4_DR_ARID          <= Rd4ARID;
   Rd4_DR_ARADDR        <= Rd4ARADDR;
   Rd4_DR_ARLEN         <= Rd4ARLEN;
   Rd4_DR_ARSIZE        <= Rd4ARSIZE;
   Rd4_DR_ARBURST       <= Rd4ARBURST;
   Rd4_DR_ARVALID       <= Rd4ARVALID;
   Rd4ARREADY           <= Rd4_DR_ARREADY;
   -- Read data channel
   Rd4RID               <= Rd4_DR_RID;
   Rd4RDATA             <= Rd4_DR_RDATA;
   Rd4RRESP             <= Rd4_DR_RRESP;
   Rd4RLAST             <= Rd4_DR_RLAST;
   Rd4RVALID            <= Rd4_DR_RVALID;
   Rd4_DR_RREADY        <= Rd4RREADY;
   
   i_Rd4_AxiRdPort : AxiRdPort
   generic map(
      c_PortName           => "Rd4", 
      c_DataWidth          => 128, 
      c_AddrWidth          => 27,
      c_LenWidth           => 8, 
      c_LogEnable          => 0, 
      c_UsrFlagWidth       => 16, 
      c_CombineAxiBurst    => 1,
      c_ResetLevel         => '1',
      c_ResetAsync         => 0
        )
   port map(
      -- Clock and reset
      sRst                 => Rd4sRst,
      Clk                  => Rd4Clk,
      -- Read address channel
      U_ARID               => Rd4_DR_ARID,
      U_ARADDR             => Rd4_DR_ARADDR,
      U_ARLEN              => Rd4_DR_ARLEN,
      U_ARSIZE             => Rd4_DR_ARSIZE,
      U_ARBURST            => Rd4_DR_ARBURST,
      U_ARVALID            => Rd4_DR_ARVALID,
      U_ARREADY            => Rd4_DR_ARREADY,
      -- Read data channel
      U_RID                => Rd4_DR_RID,
      U_RDATA              => Rd4_DR_RDATA,
      U_RRESP              => Rd4_DR_RRESP,
      U_RLAST              => Rd4_DR_RLAST,
      U_RVALID             => Rd4_DR_RVALID,
      U_RREADY             => Rd4_DR_RREADY,
      -- Input channel
      M_Addr               => Rd4_BR_Addr,
      M_Len                => Rd4_BR_Len,
      M_UsrFlagIn          => Rd4_BR_UsrFlagIn,
      M_Strobe             => Rd4_BR_Strobe,
      M_Wait               => Rd4_BR_Wait,
      -- Control signals
      M_EndOfBurst         => Rd4_BR_EndOfBurst,
      M_RdPending          => Rd4_BR_RdPending,
      M_CmdPending         => Rd4_BR_CmdPending,
      -- Output channel
      M_UsrFlagOut         => Rd4_BR_UsrFlagOut,
      M_RdData             => Rd4_BR_RdData,
      M_RdDataStrobe       => Rd4_BR_RdDataStrobe,
      M_RdDataWait         => Rd4_BR_RdDataWait
   );
   
   i_Rd4_BurstGenRd : BurstGenRd
   generic map(
      c_PortName           => "Rd4",
      c_UDataWidth         => 128, 
      c_ULenWidth          => 8, 
      c_UAddrWidth         => 27, 
      c_MDataWidth         => 512, 
      c_MAddrWidth         => 25, 
      c_IAddrWidth         => 22, 
	  c_BankWidth          => 3,
      c_NbBank             => 8,
      c_NbFifo             => 8,
      c_RdPipe             => 0, 
      c_BurstLength        => 8, 
      c_MBurstLength       => 1,
      c_UBurstLength       => 4,
      c_MaxNbCmd           => 256, 
      c_MaxBurstGrp        => 4,
	  c_OptBurstGrp        => 4, 
      c_NbSeq              => 1,
      c_ThrottlePort       => 0,
      c_SmallFifoDepth     => 64, 
      c_MultFreq           => 1,
      c_MFWrPipe           => 0, 
      c_RegCmdAddrIncr     => 0, 
      c_LsbFifoDepth       => 1024, 
      c_DualClockUM        => 1,
      c_DualClockRU        => 1,
      c_Incr               => 0, 
      c_UsrFlagWidth       => 17,
      c_NbChipSelect       => 1, 
      c_RowWidth           => 15,
      c_ColWidth           => 10, 
      c_XorBank            => 1, 
      c_LogEnable          => 0, 
      c_Cmd_Rep            => 0, 
      c_DualPhy            => 0,  
      c_DbgMon             => False, 
      c_ResetLevel         => '1', 
      c_ResetAsync         => 0 
   )
   port map(
      -- Map Config
      MAP_ShiftCol         => MAP_ShiftCol,
      MAP_ShiftRow         => MAP_ShiftRow,
      MAP_ShiftCs          => MAP_ShiftCs,
      -- Clock and reset
      U_sRst               => Rd4sRst,
      U_Clk                => Rd4Clk,
      -- Input channel
      U_Addr               => Rd4_BR_Addr,
      U_Len                => Rd4_BR_Len,
      U_UsrFlagIn          => Rd4_BR_UsrFlagIn,
      U_Strobe             => Rd4_BR_Strobe,
      U_Wait               => Rd4_BR_Wait,
      -- Control signals
      U_EndOfBurst         => Rd4_BR_EndOfBurst,
      U_RdPending          => Rd4_BR_RdPending,
      U_CmdPending         => Rd4_BR_CmdPending,
      -- Output channel
      U_UsrFlagOut         => Rd4_BR_UsrFlagOut,
      U_RdData             => Rd4_BR_RdData,
      U_RdDataStrobe       => Rd4_BR_RdDataStrobe,
      U_RdDataWait         => Rd4_BR_RdDataWait,
      -- Clock and reset
      M_sRst               => sRst,
      M_Clk                => Clk,
      -- Command Strobe channel
      M_CmdStrobe          => Rd4_RR_CmdStrobe,
      M_CmdFlag            => Rd4_RR_CmdFlag,
      M_CmdIndex           => Rd4_RR_CmdIndex,
      M_CmdWait            => Rd4_RR_CmdWait,
      -- Command Get channel
      M_CmdGet             => Rd4_RR_CmdGet,
      M_CmdGetIndex        => Rd4_RR_CmdGetIndex,
      M_CmdGetAddr         => Rd4_RR_CmdGetAddr,
      -- Clock and reset
      R_sRst               => sRst,
      R_Clk                => Clk,
      -- Read data channel
      R_RdData             => Rd4_RR_Data,
      R_RdDataPut          => Rd4_RR_DataPut,
      R_RdDataIndex        => Rd4_RR_DataIndex
   );
-- READ 4:
 -- Connection to core  ------------------------------------------------
   -- Read command strobe channel
   RdCmdStrobe(23 downto 16) <= Rd4_RR_CmdStrobe;
   RdCmdFlag(23 downto 16) <= Rd4_RR_CmdFlag;
   RdCmdIndex(191 downto 128) <= extSig(Rd4_RR_CmdIndex,8,8,8);
   Rd4_RR_CmdWait       <= RdCmdWait(23 downto 16);
   -- Read command get channel
   Rd4_RR_CmdGet        <= RdCmdGet(2 downto 2);
   Rd4_RR_CmdGetIndex   <= RdCmdGetIndex(7 downto 0);
   RdCmdGetAddr(65 downto 44) <= Rd4_RR_CmdGetAddr;
   -- Read data channel
   Rd4_RR_Data          <= RdData(511 downto 0);
   Rd4_RR_DataPut       <= RdDataPut(2 downto 2);
   Rd4_RR_DataIndex     <= RdDataIndex(7 downto 0);

--   
-- READ COMMAND STROBE CHANNEL ---------------------------------------------------------
--   RdCmdStrobe(23 downto 16) <= Rd4_RR_CmdStrobe;
--   RdCmdFlag(23 downto 16) <= Rd4_RR_CmdFlag;
 --  RdCmdIndex(191 downto 128) <= extSig(Rd4_RR_CmdIndex,8,8,8);
 --  Rd4_RR_CmdWait       <= RdCmdWait(23 downto 16);
-- 
-- READ COMMAND GET CHANNEL--------------------------------------------------------------------------------------------
--   Rd4_RR_CmdGet        <= RdCmdGet(2 downto 2);
--   Rd4_RR_CmdGetIndex   <= RdCmdGetIndex(7 downto 0);
--
--   RdCmdGetAddr(65 downto 44) <= Rd4_RR_CmdGetAddr;
-- 
--   -- READ DATA CHANNEL------------------------------------------------------------------------------------
--   Rd4_RR_Data          <= RdData(511 downto 0);
--   Rd4_RR_DataPut       <= RdDataPut(2 downto 2);
--   Rd4_RR_DataIndex     <= RdDataIndex(7 downto 0);
--
   -- Connection to ports ------------------------------------------------
   -- Write address channel
   Wr2_DW_AWID          <= Wr2AWID;
   Wr2_DW_AWADDR        <= Wr2AWADDR;
   Wr2_DW_AWLEN         <= Wr2AWLEN;
   Wr2_DW_AWSIZE        <= Wr2AWSIZE;
   Wr2_DW_AWBURST       <= Wr2AWBURST;
   Wr2_DW_AWVALID       <= Wr2AWVALID;
   Wr2AWREADY           <= Wr2_DW_AWREADY;
   -- Write data channel
   Wr2_DW_WID           <= Wr2WID;
   Wr2_DW_WDATA         <= Wr2WDATA;
   Wr2_DW_WSTRB         <= Wr2WSTRB;
   Wr2_DW_WLAST         <= Wr2WLAST;
   Wr2_DW_WVALID        <= Wr2WVALID;
   Wr2WREADY            <= Wr2_DW_WREADY;
   -- Write response channel
   Wr2BID               <= Wr2_DW_BID;
   Wr2BRESP             <= Wr2_DW_BRESP;
   Wr2BVALID            <= Wr2_DW_BVALID;
   Wr2_DW_BREADY        <= Wr2BREADY;
   
   i_Wr2_AxiWrPort : AxiWrPort
   generic map(
      c_PortName           => "Wr2", 
      c_MaxNbCmd           => 256, 
      c_DataWidth          => 128, 
      c_AddrWidth          => 27, 
      c_LenWidth           => 8, 
      c_LogEnable          => 0, 
      c_UsrFlagWidth       => 16, 
      c_Cmd_rep            => 0, 
      c_CombineAxiBurst    => 1,
      c_DDRAxiBurstRatio   => 2, 
      c_ResetLevel         => '1', 
      c_ResetAsync         => 0 
   )
   port map(
      -- Clock and reset
      sRst                 => Wr2sRst,
      Clk                  => Wr2Clk,
      -- Write address channel
      U_AWID               => Wr2_DW_AWID,
      U_AWADDR             => Wr2_DW_AWADDR,
      U_AWLEN              => Wr2_DW_AWLEN,
      U_AWSIZE             => Wr2_DW_AWSIZE,
      U_AWBURST            => Wr2_DW_AWBURST,
      U_AWVALID            => Wr2_DW_AWVALID,
      U_AWREADY            => Wr2_DW_AWREADY,
      -- Write data channel
      U_WID                => Wr2_DW_WID,
      U_WDATA              => Wr2_DW_WDATA,
      U_WSTRB              => Wr2_DW_WSTRB,
      U_WLAST              => Wr2_DW_WLAST,
      U_WVALID             => Wr2_DW_WVALID,
      U_WREADY             => Wr2_DW_WREADY,
      -- Write response channel
      U_BID                => Wr2_DW_BID,
      U_BRESP              => Wr2_DW_BRESP,
      U_BVALID             => Wr2_DW_BVALID,
      U_BREADY             => Wr2_DW_BREADY,
      -- Input channel
      M_Addr               => Wr2_BW_Addr,
      M_WrData             => Wr2_BW_WrData,
      M_WrMask             => Wr2_BW_WrMask,
      M_Strobe             => Wr2_BW_Strobe,
      M_Wait               => Wr2_BW_Wait,
      -- Control signals
      M_EndOfBurst         => Wr2_BW_EndOfBurst,
      M_WrAck              => Wr2_BW_WrAck,
      M_WrAckWait          => Wr2_BW_WrAckWait,
      M_WrPending          => Wr2_BW_WrPending,
      M_CmdPending         => Wr2_BW_CmdPending,
      M_AddrDiff           => Wr2_BW_AddrDiff
   );
   i_Wr2_BurstGenWr : BurstGenWr
   generic map(
      c_PortName           => "Wr2",
      c_BurstLength        => 8, 
      c_MBurstLength       => 1,
      c_UBurstLength       => 4, 
      c_MaxNbCmd           => 256, 
      c_MaxBurstGrp        => 4, 
	  c_OptBurstGrp        => 4, 
      c_NbSeq              => 1, 
      c_SmallFifoDepth     => 64, 
      c_AlmostFullLevel    => 0, 
      c_MultFreq           => 1, 
      c_UDataWidth         => 128, 
      c_UAddrWidth         => 27, 
      c_MDataWidth         => 512, 
      c_ReplicateFract     => 1, 
      c_MAddrWidth         => 25,
      c_IAddrWidth         => 22,
	  c_BankWidth          => 3,
      c_NbBank             => 8,
      c_NbFifo             => 8,
      c_WrPipe             => 1,
      c_ByteSize           => 8, 
      c_DualClockUM        => 1, 
      c_Incr               => 0, 
      c_WrAck              => 1,
      c_UsrWrPipe          => 1,
      c_NbChipSelect       => 1,
      c_RowWidth           => 15, 
      c_ColWidth           => 10, 
      c_XorBank            => 1, 
      c_LogEnable          => 0, 
      c_WaitFast           => 0, 
      c_Cmd_Rep            => 0, 
      c_DualPhy            => 0, 
      c_DbgMon             => False, 
      c_ResetLevel         => '1', 
      c_ResetAsync         => 0 
   )
   port map(
      -- Map Config
      MAP_ShiftCol         => MAP_ShiftCol,
      MAP_ShiftRow         => MAP_ShiftRow,
      MAP_ShiftCs          => MAP_ShiftCs,
      -- Clock and reset
      U_sRst               => Wr2sRst,
      U_Clk                => Wr2Clk,
      -- Input channel
      U_Addr               => Wr2_BW_Addr,
      U_WrData             => Wr2_BW_WrData,
      U_WrMask             => Wr2_BW_WrMask,
      U_Strobe             => Wr2_BW_Strobe,
      U_Wait               => Wr2_BW_Wait,
      -- Control signals
      U_EndOfBurst         => Wr2_BW_EndOfBurst,
      U_WrAck              => Wr2_BW_WrAck,
      U_WrAckWait          => Wr2_BW_WrAckWait,
      U_WrPending          => Wr2_BW_WrPending,
      U_CmdPending         => Wr2_BW_CmdPending,
      U_AddrDiff           => Wr2_BW_AddrDiff,
      -- Clock and reset
      M_sRst               => sRst,
      M_Clk                => Clk,
      -- Command Strobe channel
      M_CmdStrobe          => Wr2_RW_CmdStrobe,
      M_CmdFlag            => Wr2_RW_CmdFlag,
      M_CmdIndex           => Wr2_RW_CmdIndex,
      M_CmdWait            => Wr2_RW_CmdWait,
      -- Command Get channel
      M_CmdGet             => Wr2_RW_CmdGet,
      M_CmdGetIndex        => Wr2_RW_CmdGetIndex,
      M_CmdGetAddr         => Wr2_RW_CmdGetAddr,
      -- Write data channel
      M_WrData             => Wr2_RW_Data,
      M_WrMask             => Wr2_RW_Mask,
      M_WrGetData          => Wr2_RW_GetData,
      M_WrDataIndex        => Wr2_RW_DataIndex
   );
   -- CONNECTION TO CORE  ------------------------------------------------
 -- Connection to core  ------------------------------------------------
   -- Write command strobe channel
   WrCmdStrobe(23 downto 16) <= Wr2_RW_CmdStrobe;
   WrCmdFlag(23 downto 16) <= Wr2_RW_CmdFlag;
   WrCmdIndex(191 downto 128) <= extSig(Wr2_RW_CmdIndex,8,8,8);
   Wr2_RW_CmdWait       <= WrCmdWait(23 downto 16);
   -- Write command get channel
   Wr2_RW_CmdGet        <= WrCmdGet(2 downto 2);
   Wr2_RW_CmdGetIndex   <= WrCmdGetIndex(7 downto 0);
   WrCmdGetAddr(65 downto 44) <= Wr2_RW_CmdGetAddr;
   -- Write data channel
   WrData(1535 downto 1024) <= Wr2_RW_Data;
   WrMask(191 downto 128) <= Wr2_RW_Mask;
   Wr2_RW_DataIndex     <= WrDataIndex(7 downto 0);
   Wr2_RW_GetData       <= WrGetData(2 downto 2);
 
-- 
-- WRITE COMMAND STROBE CHANNEL------------------------------------------------------------------
 --  WrCmdStrobe(31 downto 24) <= Wr2_RW_CmdStrobe;
  -- WrCmdFlag(31 downto 24) <= Wr2_RW_CmdFlag;
  -- WrCmdIndex(255 downto 192) <= extSig(Wr2_RW_CmdIndex,8,8,8);
  -- Wr2_RW_CmdWait       <= WrCmdWait(31 downto 24);
 
     -- WRITE COMMAND GET CHANNEL--------------------------------------------------------------------------
 --  Wr2_RW_CmdGet        <= WrCmdGet(3 downto 3);
 --  Wr2_RW_CmdGetIndex   <= WrCmdGetIndex(7 downto 0);
 -- 
  -- WrCmdGetAddr(87 downto 66) <= Wr2_RW_CmdGetAddr;
-- 
  -- -- WRITE DATA CHANNEL------------------------------------------------------------------------------------------
 --  WrData(2047 downto 1536) <= Wr2_RW_Data;
 --  WrMask(255 downto 192) <= Wr2_RW_Mask;
 --  Wr2_RW_DataIndex     <= WrDataIndex(7 downto 0);
 --  Wr2_RW_GetData       <= WrGetData(3 downto 3);
   -- Connection to ports ------------------------------------------------
   -- Write address channel
   Wr3_DW_AWID          <= Wr3AWID;
   Wr3_DW_AWADDR        <= Wr3AWADDR;
   Wr3_DW_AWLEN         <= Wr3AWLEN;
   Wr3_DW_AWSIZE        <= Wr3AWSIZE;
   Wr3_DW_AWBURST       <= Wr3AWBURST;
   Wr3_DW_AWVALID       <= Wr3AWVALID;
   Wr3AWREADY           <= Wr3_DW_AWREADY;
   -- Write data channel
   Wr3_DW_WID           <= Wr3WID;
   Wr3_DW_WDATA         <= Wr3WDATA;
   Wr3_DW_WSTRB         <= Wr3WSTRB;
   Wr3_DW_WLAST         <= Wr3WLAST;
   Wr3_DW_WVALID        <= Wr3WVALID;
   Wr3WREADY            <= Wr3_DW_WREADY;
   -- Write response channel
   Wr3BID               <= Wr3_DW_BID;
   Wr3BRESP             <= Wr3_DW_BRESP;
   Wr3BVALID            <= Wr3_DW_BVALID;
   Wr3_DW_BREADY        <= Wr3BREADY;
   
   i_Wr3_AxiWrPort : AxiWrPort
   generic map(
      c_PortName           => "Wr3", 
      c_MaxNbCmd           => 256,
      c_DataWidth          => 128, 
      c_AddrWidth          => 27, 
      c_LenWidth           => 8, 
      c_LogEnable          => 0, 
      c_UsrFlagWidth       => 16, 
      c_Cmd_rep            => 0, 
      c_CombineAxiBurst    => 1, 
      c_DDRAxiBurstRatio   => 2, 
      c_ResetLevel         => '1',
	  c_ResetAsync         => 0 
	  )
   port map(
      -- Clock and reset
      sRst                 => Wr3sRst,
      Clk                  => Wr3Clk,
      -- Write address channel
      U_AWID               => Wr3_DW_AWID,
      U_AWADDR             => Wr3_DW_AWADDR,
      U_AWLEN              => Wr3_DW_AWLEN,
      U_AWSIZE             => Wr3_DW_AWSIZE,
      U_AWBURST            => Wr3_DW_AWBURST,
      U_AWVALID            => Wr3_DW_AWVALID,
      U_AWREADY            => Wr3_DW_AWREADY,
      -- Write data channel
      U_WID                => Wr3_DW_WID,
      U_WDATA              => Wr3_DW_WDATA,
      U_WSTRB              => Wr3_DW_WSTRB,
      U_WLAST              => Wr3_DW_WLAST,
      U_WVALID             => Wr3_DW_WVALID,
      U_WREADY             => Wr3_DW_WREADY,
      -- Write response channel
      U_BID                => Wr3_DW_BID,
      U_BRESP              => Wr3_DW_BRESP,
      U_BVALID             => Wr3_DW_BVALID,
      U_BREADY             => Wr3_DW_BREADY,
      -- Input channel
      M_Addr               => Wr3_BW_Addr,
      M_WrData             => Wr3_BW_WrData,
      M_WrMask             => Wr3_BW_WrMask,
      M_Strobe             => Wr3_BW_Strobe,
      M_Wait               => Wr3_BW_Wait,
      -- Control signals
      M_EndOfBurst         => Wr3_BW_EndOfBurst,
      M_WrAck              => Wr3_BW_WrAck,
      M_WrAckWait          => Wr3_BW_WrAckWait,
      M_WrPending          => Wr3_BW_WrPending,
      M_CmdPending         => Wr3_BW_CmdPending,
      M_AddrDiff           => Wr3_BW_AddrDiff
   );
   
   i_Wr3_BurstGenWr : BurstGenWr
   generic map(
      c_PortName           => "Wr3",
      c_BurstLength        => 8,
      c_MBurstLength       => 1,
      c_UBurstLength       => 4,
      c_MaxNbCmd           => 256,
      c_MaxBurstGrp        => 4, 
	  c_OptBurstGrp        => 4,
      c_NbSeq              => 1, 
      c_SmallFifoDepth     => 64, 
      c_AlmostFullLevel    => 0, 
      c_MultFreq           => 1, 
      c_UDataWidth         => 128, 
      c_UAddrWidth         => 27, 
      c_MDataWidth         => 512, 
      c_ReplicateFract     => 1, 
      c_MAddrWidth         => 25, 
      c_IAddrWidth         => 22, 
	  c_BankWidth          => 3,
      c_NbBank             => 8,
      c_NbFifo             => 8,
      c_WrPipe             => 1,
      c_ByteSize           => 8, 
      c_DualClockUM        => 1, 
      c_Incr               => 0,
      c_WrAck              => 1, 
      c_UsrWrPipe          => 1, 
      c_NbChipSelect       => 1,
      c_RowWidth           => 15,
      c_ColWidth           => 10, 
      c_XorBank            => 1, 
      c_LogEnable          => 0,
      c_WaitFast           => 0,
      c_Cmd_Rep            => 0, 
      c_DualPhy            => 0,  
      c_DbgMon             => False, 
      c_ResetLevel         => '1', 
      c_ResetAsync         => 0 
   )
   port map(
      -- Map Config
      MAP_ShiftCol         => MAP_ShiftCol,
      MAP_ShiftRow         => MAP_ShiftRow,
      MAP_ShiftCs          => MAP_ShiftCs,
      -- Clock and reset
      U_sRst               => Wr3sRst,
      U_Clk                => Wr3Clk,
      -- Input channel
      U_Addr               => Wr3_BW_Addr,
      U_WrData             => Wr3_BW_WrData,
      U_WrMask             => Wr3_BW_WrMask,
      U_Strobe             => Wr3_BW_Strobe,
      U_Wait               => Wr3_BW_Wait,
      -- Control signals
      U_EndOfBurst         => Wr3_BW_EndOfBurst,
      U_WrAck              => Wr3_BW_WrAck,
      U_WrAckWait          => Wr3_BW_WrAckWait,
      U_WrPending          => Wr3_BW_WrPending,
      U_CmdPending         => Wr3_BW_CmdPending,
      U_AddrDiff           => Wr3_BW_AddrDiff,
      -- Clock and reset
      M_sRst               => sRst,
      M_Clk                => Clk,
      -- Command Strobe channel
      M_CmdStrobe          => Wr3_RW_CmdStrobe,
      M_CmdFlag            => Wr3_RW_CmdFlag,
      M_CmdIndex           => Wr3_RW_CmdIndex,
      M_CmdWait            => Wr3_RW_CmdWait,
      -- Command Get channel
      M_CmdGet             => Wr3_RW_CmdGet,
      M_CmdGetIndex        => Wr3_RW_CmdGetIndex,
      M_CmdGetAddr         => Wr3_RW_CmdGetAddr,
      -- Write data channel
      M_WrData             => Wr3_RW_Data,
      M_WrMask             => Wr3_RW_Mask,
      M_WrGetData          => Wr3_RW_GetData,
      M_WrDataIndex        => Wr3_RW_DataIndex
   );
-- Connection to core  ------------------------------------------------
   -- Write command strobe channel
   WrCmdStrobe(31 downto 24) <= Wr3_RW_CmdStrobe;
   WrCmdFlag(31 downto 24) <= Wr3_RW_CmdFlag;
   WrCmdIndex(255 downto 192) <= extSig(Wr3_RW_CmdIndex,8,8,8);
   Wr3_RW_CmdWait       <= WrCmdWait(31 downto 24);
   -- Write command get channel
   Wr3_RW_CmdGet        <= WrCmdGet(3 downto 3);
   Wr3_RW_CmdGetIndex   <= WrCmdGetIndex(7 downto 0);
   WrCmdGetAddr(87 downto 66) <= Wr3_RW_CmdGetAddr;
   -- Write data channel
   WrData(2047 downto 1536) <= Wr3_RW_Data;
   WrMask(255 downto 192) <= Wr3_RW_Mask;
   Wr3_RW_DataIndex     <= WrDataIndex(7 downto 0);
   Wr3_RW_GetData       <= WrGetData(3 downto 3);
-- 
-- WRITE COMMAND STROBE CHANNEL------------------------------------------------------------------
--   WrCmdStrobe(39 downto 32) <= Wr3_RW_CmdStrobe;
 --  WrCmdFlag(39 downto 32) <= Wr3_RW_CmdFlag;
  -- WrCmdIndex(319 downto 256) <= extSig(Wr3_RW_CmdIndex,8,8,8);
  -- Wr3_RW_CmdWait       <= WrCmdWait(39 downto 32);
-- 
      -- WRITE COMMAND GET CHANNEL---------------------------------------------------------------------------------
 --  Wr3_RW_CmdGet        <= WrCmdGet( 4 downto 4);
 --  Wr3_RW_CmdGetIndex   <= WrCmdGetIndex(7 downto 0);
--  
 --  WrCmdGetAddr(109 downto 88) <= Wr3_RW_CmdGetAddr;
-- 
   -- WRITE DATA CHANNEL-----------------------------------------------------------------------------------
 --  WrData(2559 downto 2048) <= Wr3_RW_Data;
 --  WrMask(319 downto 256) <= Wr3_RW_Mask;
--   Wr3_RW_DataIndex     <= WrDataIndex(7 downto 0);
 --  Wr3_RW_GetData       <= WrGetData(4 downto 4);
   -- Connection to ports ------------------------------------------------
   -- Read address channel
   Rd2_DR_ARID          <= Rd2ARID;
   Rd2_DR_ARADDR        <= Rd2ARADDR;
   Rd2_DR_ARLEN         <= Rd2ARLEN;
   Rd2_DR_ARSIZE        <= Rd2ARSIZE;
   Rd2_DR_ARBURST       <= Rd2ARBURST;
   Rd2_DR_ARVALID       <= Rd2ARVALID;
   Rd2ARREADY           <= Rd2_DR_ARREADY;
   -- Read data channel
   Rd2RID               <= Rd2_DR_RID;
   Rd2RDATA             <= Rd2_DR_RDATA;
   Rd2RRESP             <= Rd2_DR_RRESP;
   Rd2RLAST             <= Rd2_DR_RLAST;
   Rd2RVALID            <= Rd2_DR_RVALID;
   Rd2_DR_RREADY        <= Rd2RREADY;
   
   i_Rd2_AxiRdPort : AxiRdPort
   generic map(
      c_PortName           => "Rd2", 
      c_DataWidth          => 128, 
      c_AddrWidth          => 27, 
      c_LenWidth           => 8,
      c_LogEnable          => 0, 
      c_UsrFlagWidth       => 16, 
      c_CombineAxiBurst    => 1,
      c_ResetLevel         => '1', 
      c_ResetAsync         => 0 
       )
   port map(
      -- Clock and reset
      sRst                 => Rd2sRst,
      Clk                  => Rd2Clk,
      -- Read address channel
      U_ARID               => Rd2_DR_ARID,
      U_ARADDR             => Rd2_DR_ARADDR,
      U_ARLEN              => Rd2_DR_ARLEN,
      U_ARSIZE             => Rd2_DR_ARSIZE,
      U_ARBURST            => Rd2_DR_ARBURST,
      U_ARVALID            => Rd2_DR_ARVALID,
      U_ARREADY            => Rd2_DR_ARREADY,
      -- Read data channel
      U_RID                => Rd2_DR_RID,
      U_RDATA              => Rd2_DR_RDATA,
      U_RRESP              => Rd2_DR_RRESP,
      U_RLAST              => Rd2_DR_RLAST,
      U_RVALID             => Rd2_DR_RVALID,
      U_RREADY             => Rd2_DR_RREADY,
      -- Input channel
      M_Addr               => Rd2_BR_Addr,
      M_Len                => Rd2_BR_Len,
      M_UsrFlagIn          => Rd2_BR_UsrFlagIn,
      M_Strobe             => Rd2_BR_Strobe,
      M_Wait               => Rd2_BR_Wait,
      -- Control signals
      M_EndOfBurst         => Rd2_BR_EndOfBurst,
      M_RdPending          => Rd2_BR_RdPending,
      M_CmdPending         => Rd2_BR_CmdPending,
      -- Output channel
      M_UsrFlagOut         => Rd2_BR_UsrFlagOut,
      M_RdData             => Rd2_BR_RdData,
      M_RdDataStrobe       => Rd2_BR_RdDataStrobe,
      M_RdDataWait         => Rd2_BR_RdDataWait
   );
   
   i_Rd2_BurstGenRd : BurstGenRd
   generic map(
      c_PortName           => "Rd2",
      c_UDataWidth         => 128, 
      c_ULenWidth          => 8, 
      c_UAddrWidth         => 27, 
      c_MDataWidth         => 512,
      c_MAddrWidth         => 25, 
      c_IAddrWidth         => 22,
	  c_BankWidth          => 3,
      c_NbBank             => 8,
      c_NbFifo             => 8,
      c_RdPipe             => 0, 
      c_BurstLength        => 8, 
      c_MBurstLength       => 1,
      c_UBurstLength       => 4, 
      c_MaxNbCmd           => 256, 
      c_MaxBurstGrp        => 4, 
	  c_OptBurstGrp        => 4,
      c_NbSeq              => 1, 
      c_ThrottlePort       => 0,  
      c_SmallFifoDepth     => 64,
      c_MultFreq           => 1, 
      c_MFWrPipe           => 0, 
      c_RegCmdAddrIncr     => 0, 
      c_LsbFifoDepth       => 1024, 
      c_DualClockUM        => 1, 
      c_DualClockRU        => 1,
      c_Incr               => 0, 
      c_UsrFlagWidth       => 17,  
      c_NbChipSelect       => 1,
      c_RowWidth           => 15,
      c_ColWidth           => 10, 
      c_XorBank            => 1,
      c_LogEnable          => 0, 
      c_Cmd_Rep            => 0, 
      c_DualPhy            => 0,
      c_DbgMon             => False, 
      c_ResetLevel         => '1',
      c_ResetAsync         => 0
   )
   port map(
      -- Map Config
      MAP_ShiftCol         => MAP_ShiftCol,
      MAP_ShiftRow         => MAP_ShiftRow,
      MAP_ShiftCs          => MAP_ShiftCs,
      -- Clock and reset
      U_sRst               => Rd2sRst,
      U_Clk                => Rd2Clk,
      -- Input channel
      U_Addr               => Rd2_BR_Addr,
      U_Len                => Rd2_BR_Len,
      U_UsrFlagIn          => Rd2_BR_UsrFlagIn,
      U_Strobe             => Rd2_BR_Strobe,
      U_Wait               => Rd2_BR_Wait,
      -- Control signals
      U_EndOfBurst         => Rd2_BR_EndOfBurst,
      U_RdPending          => Rd2_BR_RdPending,
      U_CmdPending         => Rd2_BR_CmdPending,
      -- Output channel
      U_UsrFlagOut         => Rd2_BR_UsrFlagOut,
      U_RdData             => Rd2_BR_RdData,
      U_RdDataStrobe       => Rd2_BR_RdDataStrobe,
      U_RdDataWait         => Rd2_BR_RdDataWait,
      -- Clock and reset
      M_sRst               => sRst,
      M_Clk                => Clk,
      -- Command Strobe channel
      M_CmdStrobe          => Rd2_RR_CmdStrobe,
      M_CmdFlag            => Rd2_RR_CmdFlag,
      M_CmdIndex           => Rd2_RR_CmdIndex,
      M_CmdWait            => Rd2_RR_CmdWait,
      -- Command Get channel
      M_CmdGet             => Rd2_RR_CmdGet,
      M_CmdGetIndex        => Rd2_RR_CmdGetIndex,
      M_CmdGetAddr         => Rd2_RR_CmdGetAddr,
      -- Clock and reset
      R_sRst               => sRst,
      R_Clk                => Clk,
      -- Read data channel
      R_RdData             => Rd2_RR_Data,
      R_RdDataPut          => Rd2_RR_DataPut,
      R_RdDataIndex        => Rd2_RR_DataIndex
   );

-- READ 2:
   -- Connection to core  ------------------------------------------------
   -- Read command strobe channel
   RdCmdStrobe(31 downto 24) <= Rd2_RR_CmdStrobe;
   RdCmdFlag(31 downto 24) <= Rd2_RR_CmdFlag;
   RdCmdIndex(255 downto 192) <= extSig(Rd2_RR_CmdIndex,8,8,8);
   Rd2_RR_CmdWait       <= RdCmdWait(31 downto 24);
   -- Read command get channel
   Rd2_RR_CmdGet        <= RdCmdGet(3 downto 3);
   Rd2_RR_CmdGetIndex   <= RdCmdGetIndex(7 downto 0);
   RdCmdGetAddr(87 downto 66) <= Rd2_RR_CmdGetAddr;
   -- Read data channel
   Rd2_RR_Data          <= RdData(511 downto 0);
   Rd2_RR_DataPut       <= RdDataPut(3 downto 3);
   Rd2_RR_DataIndex     <= RdDataIndex(7 downto 0);
 
--   
-- READ COMMAND STROBE CHANNEL ---------------------------------------------------------
--   RdCmdStrobe(31 downto 24) <= Rd2_RR_CmdStrobe;
--   RdCmdFlag(31 downto 24) <= Rd2_RR_CmdFlag;
--   RdCmdIndex(255 downto 192) <= extSig(Rd2_RR_CmdIndex,8,8,8);
 --  Rd2_RR_CmdWait       <= RdCmdWait(31 downto 24);
-- 
   -- READ COMMAND GET CHANNEL-------------------------------------------------------------------------
--   Rd2_RR_CmdGet        <= RdCmdGet(3 downto 3);
--   Rd2_RR_CmdGetIndex   <= RdCmdGetIndex(7 downto 0);
--
 --  RdCmdGetAddr(87 downto 66) <= Rd2_RR_CmdGetAddr;
-- 
--   -- READ DATA CHANNEL-------------------------------------------------------------------------------
--   Rd2_RR_Data          <= RdData(511 downto 0);
 --  Rd2_RR_DataPut       <= RdDataPut(3 downto 3);
 --  Rd2_RR_DataIndex     <= RdDataIndex(7 downto 0);
--
   -- Connection to ports ------------------------------------------------
   -- Read address channel
   Rd3_DR_ARID          <= Rd3ARID;
   Rd3_DR_ARADDR        <= Rd3ARADDR;
   Rd3_DR_ARLEN         <= Rd3ARLEN;
   Rd3_DR_ARSIZE        <= Rd3ARSIZE;
   Rd3_DR_ARBURST       <= Rd3ARBURST;
   Rd3_DR_ARVALID       <= Rd3ARVALID;
   Rd3ARREADY           <= Rd3_DR_ARREADY;
   -- Read data channel
   Rd3RID               <= Rd3_DR_RID;
   Rd3RDATA             <= Rd3_DR_RDATA;
   Rd3RRESP             <= Rd3_DR_RRESP;
   Rd3RLAST             <= Rd3_DR_RLAST;
   Rd3RVALID            <= Rd3_DR_RVALID;
   Rd3_DR_RREADY        <= Rd3RREADY;
   
   i_Rd3_AxiRdPort : AxiRdPort
   generic map(
      c_PortName           => "Rd3", 
      c_DataWidth          => 128,
      c_AddrWidth          => 27, 
      c_LenWidth           => 8, 
      c_LogEnable          => 0, 
      c_UsrFlagWidth       => 16, 
      c_CombineAxiBurst    => 1, 
      c_ResetLevel         => '1', 
      c_ResetAsync         => 0 
        )
   port map(
      -- Clock and reset
      sRst                 => Rd3sRst,
      Clk                  => Rd3Clk,
      -- Read address channel
      U_ARID               => Rd3_DR_ARID,
      U_ARADDR             => Rd3_DR_ARADDR,
      U_ARLEN              => Rd3_DR_ARLEN,
      U_ARSIZE             => Rd3_DR_ARSIZE,
      U_ARBURST            => Rd3_DR_ARBURST,
      U_ARVALID            => Rd3_DR_ARVALID,
      U_ARREADY            => Rd3_DR_ARREADY,
      -- Read data channel
      U_RID                => Rd3_DR_RID,
      U_RDATA              => Rd3_DR_RDATA,
      U_RRESP              => Rd3_DR_RRESP,
      U_RLAST              => Rd3_DR_RLAST,
      U_RVALID             => Rd3_DR_RVALID,
      U_RREADY             => Rd3_DR_RREADY,
      -- Input channel
      M_Addr               => Rd3_BR_Addr,
      M_Len                => Rd3_BR_Len,
      M_UsrFlagIn          => Rd3_BR_UsrFlagIn,
      M_Strobe             => Rd3_BR_Strobe,
      M_Wait               => Rd3_BR_Wait,
      -- Control signals
      M_EndOfBurst         => Rd3_BR_EndOfBurst,
      M_RdPending          => Rd3_BR_RdPending,
      M_CmdPending         => Rd3_BR_CmdPending,
      -- Output channel
      M_UsrFlagOut         => Rd3_BR_UsrFlagOut,
      M_RdData             => Rd3_BR_RdData,
      M_RdDataStrobe       => Rd3_BR_RdDataStrobe,
      M_RdDataWait         => Rd3_BR_RdDataWait
   );
   
   i_Rd3_BurstGenRd : BurstGenRd
   generic map(
      c_PortName           => "Rd3",
      c_UDataWidth         => 128, 
      c_ULenWidth          => 8, 
      c_UAddrWidth         => 27, 
      c_MDataWidth         => 512,
      c_MAddrWidth         => 25,
      c_IAddrWidth         => 22, 
	  c_BankWidth          => 3,
      c_NbBank             => 8,
      c_NbFifo             => 8,
      c_RdPipe             => 0, 
      c_BurstLength        => 8, 
      c_MBurstLength       => 1,
      c_UBurstLength       => 4,
      c_MaxNbCmd           => 256, 
      c_MaxBurstGrp        => 4, 
	  c_OptBurstGrp        => 4,
      c_NbSeq              => 1, 
      c_ThrottlePort       => 0, 
      c_SmallFifoDepth     => 64, 
      c_MultFreq           => 1,
      c_MFWrPipe           => 0,
      c_RegCmdAddrIncr     => 0, 
      c_LsbFifoDepth       => 1024, 
      c_DualClockUM        => 1, 
      c_DualClockRU        => 1, 
      c_Incr               => 0, 
      c_UsrFlagWidth       => 17, 
      c_NbChipSelect       => 1,
      c_RowWidth           => 15,
      c_ColWidth           => 10, 
      c_XorBank            => 1, 
      c_LogEnable          => 0, 
      c_Cmd_Rep            => 0, 
      c_DualPhy            => 0,  
      c_DbgMon             => False, 
      c_ResetLevel         => '1', 
      c_ResetAsync         => 0 

   )
   port map(
      -- Map Config
      MAP_ShiftCol         => MAP_ShiftCol,
      MAP_ShiftRow         => MAP_ShiftRow,
      MAP_ShiftCs          => MAP_ShiftCs,
      -- Clock and reset
      U_sRst               => Rd3sRst,
      U_Clk                => Rd3Clk,
      -- Input channel
      U_Addr               => Rd3_BR_Addr,
      U_Len                => Rd3_BR_Len,
      U_UsrFlagIn          => Rd3_BR_UsrFlagIn,
      U_Strobe             => Rd3_BR_Strobe,
      U_Wait               => Rd3_BR_Wait,
      -- Control signals
      U_EndOfBurst         => Rd3_BR_EndOfBurst,
      U_RdPending          => Rd3_BR_RdPending,
      U_CmdPending         => Rd3_BR_CmdPending,
      -- Output channel
      U_UsrFlagOut         => Rd3_BR_UsrFlagOut,
      U_RdData             => Rd3_BR_RdData,
      U_RdDataStrobe       => Rd3_BR_RdDataStrobe,
      U_RdDataWait         => Rd3_BR_RdDataWait,
      -- Clock and reset
      M_sRst               => sRst,
      M_Clk                => Clk,
      -- Command Strobe channel
      M_CmdStrobe          => Rd3_RR_CmdStrobe,
      M_CmdFlag            => Rd3_RR_CmdFlag,
      M_CmdIndex           => Rd3_RR_CmdIndex,
      M_CmdWait            => Rd3_RR_CmdWait,
      -- Command Get channel
      M_CmdGet             => Rd3_RR_CmdGet,
      M_CmdGetIndex        => Rd3_RR_CmdGetIndex,
      M_CmdGetAddr         => Rd3_RR_CmdGetAddr,
      -- Clock and reset
      R_sRst               => sRst,
      R_Clk                => Clk,
      -- Read data channel
      R_RdData             => Rd3_RR_Data,
      R_RdDataPut          => Rd3_RR_DataPut,
      R_RdDataIndex        => Rd3_RR_DataIndex
   );
-- READ 3:
 
   -- Connection to core  ------------------------------------------------
   -- Read command strobe channel
   RdCmdStrobe(39 downto 32) <= Rd3_RR_CmdStrobe;
   RdCmdFlag(39 downto 32) <= Rd3_RR_CmdFlag;
   RdCmdIndex(319 downto 256) <= extSig(Rd3_RR_CmdIndex,8,8,8);
   Rd3_RR_CmdWait       <= RdCmdWait(39 downto 32);
   -- Read command get channel
   Rd3_RR_CmdGet        <= RdCmdGet(4 downto 4);
   Rd3_RR_CmdGetIndex   <= RdCmdGetIndex(7 downto 0);
   RdCmdGetAddr(109 downto 88) <= Rd3_RR_CmdGetAddr;
   -- Read data channel
   Rd3_RR_Data          <= RdData(511 downto 0);
   Rd3_RR_DataPut       <= RdDataPut(4 downto 4);
   Rd3_RR_DataIndex     <= RdDataIndex(7 downto 0);
 

--   
-- READ COMMAND STROBE CHANNEL ---------------------------------------------------------
--   RdCmdStrobe(39 downto 32) <= Rd3_RR_CmdStrobe;
--   RdCmdFlag(39 downto 32) <= Rd3_RR_CmdFlag;
--   RdCmdIndex(319 downto 256) <= extSig(Rd3_RR_CmdIndex,8,8,8);
--   Rd3_RR_CmdWait       <= RdCmdWait(39 downto 32);
--  
   -- READ COMMAND GET CHANNEL--------------------------------------------------------------------------------------
--   Rd3_RR_CmdGet        <= RdCmdGet(4 downto 4);
--   Rd3_RR_CmdGetIndex   <= RdCmdGetIndex(7 downto 0);
--
--   RdCmdGetAddr(109 downto 88) <= Rd3_RR_CmdGetAddr;
-- 
--   -- READ DATA CHANNEL-------------------------------------------------------------------------------------------------
--   Rd3_RR_Data          <= RdData(511 downto 0);
 --  Rd3_RR_DataPut       <= RdDataPut(4 downto 4);
--   Rd3_RR_DataIndex     <= RdDataIndex(7 downto 0);
--
-- ====================================================================================================   
   ------------------------------------------------------------------------
   -- Serial Presence Detect
   ------------------------------------------------------------------------
   
   i_SPD : SpdDummy
   generic map(
	  c_BankWidth          => 3,
      c_RowWidth           => 15, 
      c_ColWidth           => 10, 
      c_NbChipSelect       => 1 
   )
   port map(
      -- SPD values
      NUM_COL              => Int_NUM_COL,
      NUM_ROW              => Int_NUM_ROW,
      NUM_BANK             => Int_NUM_BANK,
      NUM_RANK             => Int_NUM_RANK,
      SPD_DONE             => Int_SPD_DONE
   );
   ------------------------------------------------------------------------
   -- Controller core
   ------------------------------------------------------------------------
   
   i_SdramCtrl : SdramCtrlReorder
   generic map(
      c_DDR                => 4,
	  c_BankWidth          => 3,
      c_BankGroupWidth     => 1, 
      c_RowWidth           => 15, 
      c_ColWidth           => 10, 
      c_NbChipSelect       => 1, -- 1,
      c_NbBank             => 8,
	  c_nRRD_S             => 1,
      c_DisableDQSn        => 1,
      c_NoRefresh          => 0, 
      c_MaxPostedRef       => 1,
      c_AL                 => 0, 
      c_BurstLength        => 8,
      c_DataRate           => 8,
      c_NoDataCmdSlot      => 1, 
      c_DataCmdSlot        => 2, 
	  c_CASLatency         => 17, 
      c_CWLatency          => 12, 
      c_tCK                => 833,
	  c_tRPA               => 14333, 
      c_tWR                => 15000,
      c_tRFC               => 260000, 
      c_tREFI              => 7800000, 
      c_tZQCSI             => 256,
      c_nZQCS              => 16, 
      c_nRTR               => 14,
      c_nWTW               => 18,
      c_nRTW               => 3,
      c_nWTR               => 7, 
	  c_nFAW               => 10,
      c_FastFAW            => 0,
	  c_nRRD               => 1, 
	  c_nCCD               => 1,
      c_ColDelay           => 4,
      c_WriteDelay         => 7, 
      c_WrPipe             => 2, 
      c_WrPipe_BRAM        => 1, 
      c_RdPipe             => 1, 
      c_RdDataFifo         => 0, 
      c_RdDataFifoPipe     => 1, 
      c_RdPipe_BRAM        => 0,
      c_ODTStart           => 4, 
      c_ODTStartSlot       => 2, 
      c_ODTEnd             => 6,
      c_ODTEndSlot         => 1, 
      c_PipeIntervalWaitHigh => 3, 
      c_PipeIntervalWaitLow => 0, 
      c_PipeIntervalValidHigh => 3,
      c_PipeIntervalValidLow => 0, 
      c_ReadGrouping       => 96,
      c_WriteGrouping      => 32,
      c_IndexWidth         => 8, 
      c_AddrPipe           => 0,  
      c_ChipGrouping       => 4, 
      c_NbWrPorts          => 5, 
      c_NbRdPorts          => 5, 
      c_NbHiPriorWrPorts   => 2, --- 3,
      c_NbHiPriorRdPorts   => 3, --- 3,
      c_AddrWidth          => 25, 
      c_PhysAddrWidth      => 17, 
      c_IAddrWidth         => 22, 
      c_DataWidth          => 512,
      c_DualClock          => 0,  
      c_XorBank            => 1,
      c_PhyInit            => 1, 
      c_ODT                => 0,  
      c_DriveStrength      => 0, 
      c_PerRd              => 1, 
      c_CntBits            => 9,
      c_CompBits           => "10010",  
      c_Arch               => "ULTRASCALE",  
      c_TestBackpressure   => g_TestBackpressure, 
      c_DbgMon             => True,
      c_DbgMonCntSize      => 16,
      c_ResetLevel         => '1',
      c_ResetAsync         => 0
   )
   port map(
      -- Clock and reset
      sRst                 => sRst,
      Clk                  => Clk,
      -- Clock and reset
      R_sRst               => sRst,
      R_Clk                => Clk,
      -- SPD values
      NUM_COL              => Int_NUM_COL,
      NUM_ROW              => Int_NUM_ROW,
      NUM_BANK             => Int_NUM_BANK,
      NUM_RANK             => Int_NUM_RANK,
      SPD_DONE             => Int_SPD_DONE,
      -- Map Config
      MAP_ShiftCol         => MAP_ShiftCol,
      MAP_ShiftRow         => MAP_ShiftRow,
      MAP_ShiftCs          => MAP_ShiftCs,
      -- Write address channel
      WrCmdStrobe          => WrCmdStrobe,
      WrCmdFlag            => WrCmdFlag,
      WrCmdIndex           => WrCmdIndex,
      WrCmdWait            => WrCmdWait,
      WrCmdGet             => WrCmdGet,
      WrCmdGetIndex        => WrCmdGetIndex,
      WrCmdGetAddr         => WrCmdGetAddr,
      -- Write data channel
      WrData               => WrData,
      WrMask               => WrMask,
      WrGetData            => WrGetData,
      WrDataIndex          => WrDataIndex,
      WrDataIndexLsb       => WrDataIndexLsb,
      WrDataIndex_2x       => WrDataIndex_2x,
      -- Read address channel
      RdCmdStrobe          => RdCmdStrobe,
      RdCmdFlag            => RdCmdFlag,
      RdCmdIndex           => RdCmdIndex,
      RdCmdWait            => RdCmdWait,
      RdCmdGet             => RdCmdGet,
      RdCmdGetIndex        => RdCmdGetIndex,
      RdCmdGetAddr         => RdCmdGetAddr,
      -- Read data channel
      RdData               => RdData,
      RdDataPut            => RdDataPut,
      RdDataIndex          => RdDataIndex,
      RdDataIndexLsb       => RdDataIndexLsb,
      PortDummyRead        => PortDummyRead,
      InitDone             => InitDone,
      -- Calibration
      CalibDone            => CalibDone,
      DoRIU                => DoRIU,
      -- Command output
      SdramACTn            => SdramACTn,
      SdramCSn             => SdramCSn,
      SdramRASn            => SdramRASn,
      SdramCASn            => SdramCASn,
      SdramWEn             => SdramWEn,
      SdramODT             => SdramODT,
      SdramCKE             => SdramCKE,
      SdramAddr            => SdramAddr,
      SdramBank            => SdramBank,
      SdramCmd             => SdramCmd,
      SdramSlot            => SdramSlot,
      -- Write data control
      SdramWrEn            => SdramWrEn,
      SdramWrData          => SdramWrData,
      SdramWrMask          => SdramWrMask,
      -- Read data control
      SdramRdEn            => SdramRdEn,
      SdramRdData          => SdramRdData,
      RdDataValid          => RdDataValid
   );
   
   ------------------------------------------------------------------------
   -- Physical interface
   ------------------------------------------------------------------------
   
   i_SdramPhyUltrascale_0 : SdramPhyUltrascale_0_1
   generic map(
      c_ResetLevel         => '1', 
	  c_BankWidth          => 3,
      c_BankGroupWidth     => 1,
      c_NbChipSelect       => 1, 
      c_NbCke              => 1, 
      c_DataWidth          => 64,
      c_NbClkPairs         => 1, 
      c_PhysAddrWidth      => 17,
      c_DataRate           => 8, 
      c_PingPongPhy        => 1, 
      c_CkeShared          => 0, 
      c_DataBufAdrWidth    => 5, 
      c_OdtWidth           => 1, 
      c_DmWidth            => 8,
      c_DqsWidth           => 8, 
      c_DataCmdSlot_0      => 2, 
      c_NoDataCmdSlot_0    => 1,
      c_BurstLength        => 8, 
      c_DDR                => 4, 
	  c_Registered         => 0, 
	  c_CASLatency         => 17,
      c_CWLLatency         => 12, 
      c_tCK                => 833,
      c_tWR                => 15000, 
      c_tRFC               => 260000, 
	  c_tRPA               => 14333, 
      c_DisableDQSn        => 1,
      c_ClkinPeriodMmcm    => 7998, 
	  c_ClkfboutMultMmcm   => 12, 
      c_DivclkDivideMmcm   => 1, 
      c_Clkout0DivideMmcm  => 5, 
      c_Clkout1DivideMmcm  => 5, 
	  c_Clkout2DivideMmcm  => 6, 
      c_SysClkType         => "DIFFERENTIAL"  
   )
   port map(
      -- Clock and reset
      sRst                 => sRst,
      Clk                  => Clk,
      -- Calibration
      CalibDone            => CalibDone,
      DoRIU                => DoRIU,
      -- Command output
      SdramACTn            => SdramACTn,
      SdramCSn             => SdramCSn,
      SdramRASn            => SdramRASn,
      SdramCASn            => SdramCASn,
      SdramWEn             => SdramWEn,
      SdramODT             => SdramODT,
      SdramCKE             => SdramCKE,
      SdramAddr            => SdramAddr,
      SdramBank            => SdramBank,
      SdramCmd             => SdramCmd,
      SdramSlot            => SdramSlot,
      -- Write data control
      SdramWrEn(0)         => SdramWrEn,
      SdramWrData          => SdramWrData,
      SdramWrMask          => SdramWrMask,
      -- Read data control
      SdramRdEn(0)         => SdramRdEn,
      SdramRdData          => SdramRdData,
      RdDataValid(0)       => RdDataValid,
      -- DDR interface
      -- Reset output
      sRst_o               => sRst_o,
      sys_rst              => sys_rst,
      UsrClk               => UsrClk,
      Fail_wrDataEn        => Fail_wrDataEn,
      c0_ddr4_ba           => c0_ddr4_ba,
      c0_ddr4_dqs_c        => c0_ddr4_dqs_c,
      c0_ddr4_dqs_t        => c0_ddr4_dqs_t,
      c0_ddr4_ck_c         => c0_ddr4_ck_c,
      c0_ddr4_ck_t         => c0_ddr4_ck_t,
      c0_ddr4_adr          => c0_ddr4_adr,
      c0_ddr4_cke          => c0_ddr4_cke,
      c0_ddr4_odt          => c0_ddr4_odt,
      c0_ddr4_reset_n      => c0_ddr4_reset_n,
      c0_ddr4_dq           => c0_ddr4_dq,
      c0_ddr4_act_n        => c0_ddr4_act_n,
      c0_ddr4_bg           => c0_ddr4_bg,
      c0_ddr4_dm_dbi_n     => c0_ddr4_dm_dbi_n,
      c0_ddr4_cs_n         => c0_ddr4_cs_n,
      -- Clock (input to PHY)
      c0_sys_clk_p         => c0_sys_clk_p,
      c0_sys_clk_n         => c0_sys_clk_n
   );

end rtl;


