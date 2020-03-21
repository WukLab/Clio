# Host Stack

## Testing and Run

### loopback testing 

Requirements
1. monitor and host must use `lo` as the network device
2. all instances must use different UDP ports

```
./monitor.o -d lo -p 8888
./host.o -d lo -m 127.0.0.1:8888 -p 8887
./host.o -d lo -m 127.0.0.1:8888 -p 8886
```

### Real testing

Requirements
1. Use the Mellanox device. It's usually `ens4` or `p4p1`. Use `ifconfig` to check.
2. You need to know monitor's IP address

```
./monitor.o -d ens4 -p 8888 (assume runs on wuklab05)
./host.o -d lo -m 192.168.1.5:8888 -p 8888 (assume runs on wuklab01)
./host.o -d lo -m 192.168.1.5:8888 -p 8888 (assume runs on wuklab02)
```

## LegoMem Core APIs

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


Stack Layout:
```
    |---------------------------------------|
    |              User                     |
    |---------------------------------------|
    | (Rel-Go-back-N)   (Bypass)            |  <-   Transport Layer
    |---------------------------------------|
    | (Raw Socket)  (Raw IB Verbs)  (DPDK)  |  <-   Raw Net Layer
    |---------------------------------------|
    |         Ethernet NIC                  |
    |---------------------------------------|
```
