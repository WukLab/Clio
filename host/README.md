# Host Stack

## Network

Public APIs

- `net_send`
- `net_receive`: block until there is a incoming message
- `net_receive_nb`: a non-blocking version of `net_receive`. It will return immediately if there is no message.

Two layers of implementations

1. Transport Layer (`struct transport_net_ops`)
    - Reliable Go-Back-N
    - Bypass
2. Raw Network (`struct raw_net_ops`)
    - Raw IB verbs
    - Raw socket
