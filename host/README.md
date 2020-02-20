# Host Stack

## Network

Public APIs

- `net_send`
- `net_receive`
- `net_receive_nb`

Two layers of implementations

1. Transport Layer (`struct transport_net_ops`)
    - Reliable Go-Back-N
    - Bypass
2. Raw Network (`struct raw_net_ops`)
    - Raw IB verbs
    - Raw socket
