# Compilation

This file document the steps required to compile various components in the Clio system.

There are three major parts:
1. FPGA bitstream
2. ARM SoC code running inside FPGA board
3. Host side software

FPGA compilation is the most time-consuming part also the most complicated part. We need to first compile Scala into Verilog, then run Vivado scripts to compile design files into the final bitstream. The last two parts (SoC and Host) are just generic C code hence easy to compile.

Next, we will discuss each part in detail.

## FPGA Bitstream

Software Prerequisite: **Vivado v2020.2 (64-bit)**.
We use this version in our devlopment.
If you use other Vivado versions, the TCL scripts might fail due to mismatched
Vivado IP version numbers. If that happens, you can update the TCL scripts 
to using appropriate versions. Nonetheless, we recommend v2020.2 to compile Clio.

Source code: FPGA related files are under `board/fpga/`.

The overall compilation flow is:
1. Go to `board/fpga/`. Run `make` directly. This step will compile Scala into Verilog, compile the HLS-based network work, and produce the top project. Please check the Makefile for more details
2. Go to `board/fpga/top/`. Run `make g` to open Vivado GUI.
3. Inside Vivado GUI, run Synthesis and Implementation to produce the bitstream.
4. Copy the produced the bitstream into an easy to access folder. **By default**, the bitstream can be found at `board/fpga/top/generated_vivado_project/generated_vivado_project.runs/impl_1/fpga.bit`.
Later on, we will use PetaLinux to load this bitstream into the FPGA board

## ARM SoC Code

Software Prerequisite: **aarch64 cross compilation software**.

Source code: `board/soc/core`.

Compilation is simple, go to `board/soc/core` and run `make` directly.
It produces an aarch64 binary named `core.o`. Later on, we will copy this
binary into the FPGA board. 

## Host-Side Code

Software Prerequisite: **x86 compilation software**, **`libibverbs`**.
We use Ubuntu 20.04 in our development.

Source coed: `host/`.

To compile, go to `host/` and run `make` directly.
It produces two binaries `host.o` and `monitor.o`.
We include all test files within these binaries.
To run Clio, we must have `host.o`. However, `monitor.o` is optional.