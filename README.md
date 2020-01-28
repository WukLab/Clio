LegoMem
============

Setup the Spinal Environment
------------

Import this project into the IDEA

Ip Generation
-----------

To generate a vivado IP, you have to follow the naming convention of the AXI Interface. Your module need to extend the trait AXI4TopLevel and add the rename task at the end of the module. then You can use `package_ip.tcl` to package ip in the project mode.

Simulation with Vivado.
-----------

To simulate the code into the 

## Repo Layout

- board
    - soc
    - fpga
- host: normal server client side library, including network stack, vRegion, and so on.
- monitor: global memory monitor, including network stack, handlers, and so on.