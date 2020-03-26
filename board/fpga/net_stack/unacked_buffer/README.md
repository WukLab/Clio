# Unacknowledged Packet Buffer Controller
Last Updated: Mar 26, 2020

## Introduction
This is the controller of unacked packet buffer. It use a datamover to read and write the buffer in dram.

## Memory Layout
The buffer is made up of small slots stored in a matrix style. Each slot stores one packet, the size of each slot is defined by `MAX_PACKET_SIZE`. Each session has `WINDOW_SIZE` slots, the slots in the same session are stored consecutively. And the per session slot groups are also stored consecutively. The whole buffer is stored at the offset `BUFF_ADDRESS_START` defined in `unacked_buffer.hpp`.

In the first 8 bytes of each slot stores the lenth of the packet stored in that slot. Every time we want to read out a packet, we need to first read out the length, then we are able to read out the packet contents.

The buffer stores the ip route info of every session in BRAM.
