# LegoMem FPGA

## Code Layout

- `generated_ip/`: collects all generated IPs throughout the project.
- `net_reliable/`: our reliable go-back-n network layer, exporting a single big `relnet` IP.
- `top/`: including both Alex's original eth/ip/udp stack and legomem core BD.
          This is the top-level design.
