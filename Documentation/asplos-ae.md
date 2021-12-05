# ASPLOS'22 Artifact Evaluation Instructions

We want to first thank you for evaluating our artifact.

Clio is a complicated system with many moving parts.
The whole compilation and configuration flow involes FPGA,
which is lengthy and error-prone, even to FPGA experts.
To make the evaluation process smoother, we decide to pre-setup the whole system including the host side test program and the FPGA boards. _As an eveluator, you only need to run software scripts on normal servers_.

We recommend you to read [Documentation/compile.md](./compile.md) for repo organization overview.

## Key Results to Reproduce

Our paper has many results ranging from end-to-end microbenchmark, on-board testing, and end-to-end applications.

Except for microbenmarkes, other testings are a bit tricky to setup
and may require evaluators to play around FPGA directly.
Hence we decide to only include microbenchmarks in this "reproduce testing".
Nonetheless, we believe reproducing the scalability, latency, and throughput numbers would be sufficient to prove Clio's design points.

1. Process, PTE, and MR Scalability: paper Figure 4 and Figure 5.
2. End-to-End Throughput: paper Figure 8.
3. Latency: paper Figure 10 Read Latency and Figure 11 Write Latency.

If you'd like to try out the compilation process, please checkout [Documentation/compile.md](./compile.md).

### Prepare

1. Use the instructions we posted on HotCRP to login into our server (wuklab-11).
2. Once login, we could see three folders. The `artifcats/` folder has the pre-generated FPGA bitstreams and ARM binaries. The `scripts/` folder has bash scripts used to configure FPGA. The `Clio-asplosae/` folder is a freshly cloned Clio repo from Github. You will use this folder only.
```bash
[asplos-ae@wuklab-11]~% pwd
/home/asplos-ae
[asplos-ae@wuklab-11]~% ls -l
total 12
drwxrwxr-x. 4 asplos-ae asplos-ae 4096 Dec  5 01:23 artifacts
drwxr-xr-x. 9 asplos-ae asplos-ae 4096 Dec  4 12:22 Clio-asplosae
drwxrwxr-x. 2 asplos-ae asplos-ae 4096 Dec  2 14:05 scripts
```
3. Switch folder to `Clio-asplosae/host/`. You will run test scripts from here.
```bash
[asplos-ae@wuklab-11 ~]$ cd Clio-asplosae/host/
[asplos-ae@wuklab-11 host]$ pwd
/home/asplos-ae/Clio-asplosae/host
```

### Figure 4 (Process Scalability)

This test reproduces paper Figure 4's `Clio-Read` and `Clio-Write` lines.
Since Clio uses connection-less RPC-oriented interfaces,
Clio perfectly scales with the increasing number of connections/processes.

To test, run the following command to invoke the testing script:
```bash
$ ./scripts/test_rw_processes.sh
```

Below is the expected output. The read/write latencies stay flat.
```bash
...
All sessions created, start read/write test..
nr_sessions:    1 avg_Write:  3959.578125 ns Throughput: 32.326676 Mbps
nr_sessions:    8 avg_Write:  3736.976562 ns Throughput: 34.252289 Mbps
nr_sessions:  100 avg_Write:  3710.851562 ns Throughput: 34.493430 Mbps
nr_sessions:  400 avg_Write:  3720.109375 ns Throughput: 34.407591 Mbps
nr_sessions:  700 avg_Write:  3702.250000 ns Throughput: 34.573570 Mbps
nr_sessions: 1000 avg_Write:  3735.078125 ns Throughput: 34.269698 Mbps
nr_sessions:    1 avg_Read:  3918.640625 ns Throughput: 32.664389 Mbps
nr_sessions:    8 avg_Read:  3920.921875 ns Throughput: 32.645384 Mbps
nr_sessions:  100 avg_Read:  3903.953125 ns Throughput: 32.787279 Mbps
nr_sessions:  400 avg_Read:  3910.218750 ns Throughput: 32.734742 Mbps
nr_sessions:  700 avg_Read:  3904.187500 ns Throughput: 32.785311 Mbps
nr_sessions: 1000 avg_Read:  3909.632812 ns Throughput: 32.739647 Mbps
All tests are done.
```

### Figure 5 (PTE/MR Scalability)

XXX

### Figure 8 (End-to-End Throughput)

XXX

### Figure 9 and Figure 10 (End-to-End Read/Write Latency)

This test reproduces paper Figure 9 and Figure 10's Clio lines.

To test, run the following command to invoke the testing script:
```bash
$ ./scripts/test_rw_presetup.sh
```

Below is the expected output.
```bash
...
size:     4 avg_WRITE:  3881.570312 ns Throughput: 8.244086 Mbps
size:    64 avg_WRITE:  4663.929688 ns Throughput: 109.778670 Mbps
size:   256 avg_WRITE:  4822.085938 ns Throughput: 424.712464 Mbps
size:  1024 avg_WRITE:  5730.507812 ns Throughput: 1429.541721 Mbps
size:  4096 avg_WRITE: 12935.218750 ns Throughput: 2533.238953 Mbps
size:     4 avg_READ:  4013.015625 ns Throughput: 7.974053 Mbps
size:    64 avg_READ:  4112.539062 ns Throughput: 124.497298 Mbps
size:   256 avg_READ:  4633.179688 ns Throughput: 442.029047 Mbps
size:  1024 avg_READ:  6788.617188 ns Throughput: 1206.725873 Mbps
size:  4096 avg_READ: 12328.343750 ns Throughput: 2657.940163 Mbps
All tests are done.
```

## Notes and Troubleshooting

Board-3126 (named so in our old script) info:
- FPGA IP:      192.168.1.26
- FPGA ARM IP:  192.168.0.26
- Host serial console: `sudo minicom -D /dev/ttyUSB0`
- hw_server 3121 (hw_server -s tcp::3121 -e set jtag-port-filter 13276)
- vivado localhost:3121

Before you load the bitstream into the FPGA, first open the serial console to the board. While PetaLinux is downloading the bitstream into the board, you can see various messages printed in the serial console. At one point, type `run netboot` in the serial console to boot Linux on ARM. Then use `username: root` & `password: root` to login. Finally, run the following script to configure FPGA chip's IP and MAC address.

```bash
#
# Board-3126
#
cat > test.sh <<EOF
ip addr del 192.168.0.10/24 dev eth0
ip addr add 192.168.0.26/24 dev eth0
insmod /lib/modules/4.19.0/extra/xilinx-axidma.ko
# Change FPGA IP 192.168.1.26
devmem 0xA000C000 32 0xC0A8011A
#devmem 0xA000C004 32 0xf0e0c01A
EOF
bash test.sh
```

The following GIF showcases the whole boot flow:

[TODO GIF]()

### Default ARM SoC Setup

By default, in `ctrl.c`, we call `prepare_onboard_va()` to pre-allocate this process and its address space: PID=1, VA range `[0x40000000, 0xb4c0000000)`. A client program can directly use this information to access on-board resources.