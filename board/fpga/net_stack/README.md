# Go-Back-N Reliable Network
Last Updated: Feb 13, 2020

## Introduction
This is a Go-Back-N based reliable network built on top of UDP network stack. It consists of four major parts. Their functions are described as below.

### Reliable receiver
- receive data packet, check seqnum, deliever to onboard pipeline and send back ack/nack response.
- receive ack/nack packet and forward to unacked packets queue

### Reliable sender
- get data from onboard pipeline, append seqnum, send out to network and store it in unacked packet queue
- receive ack and remove acked packets in queue accordingly
- retransmit unacked packets if receive nack or timeout

### Unacked packets queue
- store unacked packets

### Send arbiter
- Arbitrate between response packet, normal send-out packet and retransmission packets
- Priority: rsp > retrans > normal

## Configuration
There are only a few configurations in `include/fpga/rel_net.h`
- `DEBUG_MODE`: define this to run hls c-simulation, undefine this to run hls synthesis
- `MAX_PACKET_SIZE`: the size in 8bytes(per transfer size) of the largest packet that can be stored in the unacked packet queue. default value is 4096/8
- `WINDOW_SIZE`: the max num of packets that can be sent out without acknowledgement. Since it's a circular queue, the actual capacity is `WINDOWSIZE-1`. default value is 8
- `RETRANS_TIMEOUT_CYCLE`: retransmission timeout in clock cycles
