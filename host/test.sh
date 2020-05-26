ip addr del 192.168.0.10/24 dev eth0
ip addr add 192.168.0.22/24 dev eth0
insmod /lib/modules/4.19.0/extra/xilinx-axidma.ko
# Change IP 192.168.1.232
devmem 0xA000C000 32 0xC0A80116
devmem 0xA000C004 32 0xfabeec16
