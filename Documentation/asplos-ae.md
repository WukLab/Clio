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