# Go-Back-N Reliable Network
Last Updated: Mar 26, 2020

Note that we will export a final big IP to the shared `generated_ip/` folder.
The IP folder name is described by the last line of `run_vivado.tcl`:

```
ipx::package_project -root_dir ../generated_ip/net_top_relnet_zcu106 -vendor wuklab -library user -taxonomy UserIP -module relnet -import_files
```

## Introduction
This is a Go-Back-N based reliable network built on top of UDP network stack. It consists of several parts. Their functions are described as below.

### Reliable receiver
- Receive data packet, send gbn header to state table for seqnum checking, deliever to onboard pipeline according to check result.
- Receive ack/nack packet, send to state table for dequeue.

### Reliable sender
- Get data from onboard pipeline, check with state table to get the seqnum.
- Append seqnum, send out to network and store it in unacked packet buffer.

### Net session state table
#### State table
- Check seqnum of received data packets and send ACK/NACK response accordingly.
- Check seqnum of received ack/nack packets and perform dequeue and issue retransmission requests accordingly.
- Check if buffer for certain session is full and tell sender the seqnum to sent.
#### Timer
- Store the timeout for every net session
- Reset/stop timer for certain session according to request
- Generate timeout signal upon timeout event
#### Setup manager
- Accept connection open/close request and set the session state and timer

### Unacked packets buffer
- Store sent out packets into dram
- Retransmit unacked packet upon receiving retransmission request.

### Send arbiter
- Arbitrate between response packet, normal send-out packet and retransmission packets
- Priority: rsp > retrans > normal

## Configuration
There are a few configurations in `include/uapi/gbn.h`
- `MAX_PACKET_SIZE`: The size in byte of the largest packet that can be stored in the unacked packet queue. Default value is 9216(9 kb)
- `WINDOW_SIZE`: The max num of packets that can be sent out without acknowledgement. Default value is 128.
- `NR_MAX_SESSIONS_PER_NODE`: Maximum number of connection, default value is 1024.
- `RETRANS_TIMEOUT_CYCLE`: Retransmission timeout in clock cycles. Default is 100000000 cycles(4ms).
