# ASPLOS'22 Artifact Evaluation Instructions

We want to first thank you for evaluating our artifact.

Clio is a complicated system with many moving parts.
The whole compilation and configuration flow involes FPGA,
which is lengthy and error-prone, even to FPGA experts.
To make the evaluation process smoother, we decide to pre-setup the whole system including the host side test program and the FPGA boards. _As an eveluator, you only need to run software scripts on normal servers_.

## Key Results to Reproduce

Our paper has many results ranging from end-to-end microbenchmark, on-board testing, and end-to-end applications.

Except for microbenmarkes, other testings are a bit tricky to setup
and may require evaluators to play around FPGA directly.
Hence we decide to only include microbenchmarks in this "reproduce testing".
Nonetheless, we believe reproducing the scalability, latency, and throughput numbers would be sufficient to prove Clio's design points. The details:

1. Process, PTE, and MR Scalability: paper Figure 3 and Figure 4.
2. End-to-End Throughput: paper Figure 7.
3. Latency: paper Figure 9 Read Latency and Figure 10 Write Latency.

### Figure 3 and Figure 4 (Process/PTE/MR Scalability)

XXX

### Figure 7 (End-to-End Throughput)

XXX

### Figure 9 and Figure 10 (End-to-End Read/Write Latency)

XXX

## Notes and Troubleshooting

Board-3126 info:
- FPGA IP:      192.168.1.26
- FPGA ARM IP:  192.168.0.26
- Host serial console (on wuklab-11): `sudo minicom -D /dev/ttyUSB0`

Before you load the bitstream into the FPGA, first open the serial console to the board. While PetaLinux is downloading the bitstream into the board, you can see various messages printed in the serial console. At one point, type `run netboot` in the serial console to boot Linux on ARM. Then use `username: root` & `password: root` to login. Finally, run the following script to configure FPGA chip's IP and MAC address.

```bash
#
# Board-3126
#
cat > test.sh <<EOF
ip addr del 192.168.0.10/24 dev eth0
ip addr add 192.168.0.26/24 dev eth0 # ARM's IP
insmod /lib/modules/4.19.0/extra/xilinx-axidma.ko
# Change FPGA IP 192.168.1.26
devmem 0xA000C000 32 0xC0A8FF04
devmem 0xA000C004 32 0xf0e0c01A
EOF
bash test.sh
```

The following GIF showcases the whole boot flow:

[TODO GIF]()