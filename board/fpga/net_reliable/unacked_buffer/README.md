# Unacknowledged Packet Buffer Controller
Last Updated: Mar 26, 2020

## Introduction
This is the controller of unacked packet buffer. It use a datamover to read and write the buffer in dram. For every packet it receives, it writes the whole packet to dram and only stores the routing info on chip. This simplifies the management.

To be able to retrive the packet we stored, we need to know its address and length. Address can be calculated from the session id and seqnum. However, length has to be stored somewhere. Storing the length for every packet onchip consumes too much resources, so they are stored together with each packet.

## Memory Layout
The buffer is made up of small slots stored in a matrix style. Each slot stores one packet, the size of each slot is defined by `MAX_PACKET_SIZE`. Each session has `WINDOW_SIZE` slots, the slots in the same session are stored consecutively. And the per session slot groups are also stored consecutively. The whole buffer is stored at the offset `BUFF_ADDRESS_START` defined in `unacked_buffer.hpp`.
```
		 0					N * MAX_PACKET_SIZE
		 +------+------+-----+------+
session 0|  00  |  01  | ... |  0N  |
		 +------+------+-----+------+
session 1|  10  |  11  | ... |  1N  |
		 +------+------+-----+------+
		 | ...  |  ... | ... |  ... |
		 +------+------+-----+------+
session M|  M0  |  M1  | ... |  MN  |
		 +------+------+-----+------+
```

In the first 8 bytes of each slot stores the lenth of the packet stored in that slot. Every time we want to read out a packet, we need to first read out the length, then we are able to read out the packet contents.
```
0		 8			MAX_PACKET_SIZE
+--------+--------------+
| length |  packet data |
+--------+--------------+
```

The buffer stores the ip route info of every session in BRAM.

## TODO
Maybe we need to store the address of every packet onboard and avoid writting everything back to dram.
