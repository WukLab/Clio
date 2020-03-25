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

List of APIS:
- TODO

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

## Boards and Sessions

We have crafted couple special `board_info` and `session_net`.
One set of special local management session, and one for localhost.
Thus, we a host or monitor first starts running, the list of boards are:
```
  bucket                      name              ip:port       type
-------- ------------------------- -------------------- ----------
       0        special_local_mgmt            0.0.0.0:0      dummy
      19         special_localhost       127.0.0.1:8885  localhost
      19                   monitor       127.0.0.1:8888    monitor
```

And the list of sessions are:
```
  bucket    ses_local   ses_remote       ip:port_remote               remote_name
-------- ------------ ------------ -------------------- -------------------------
       0            0            0            0.0.0.0:0        special_local_mgmt
     619            0            0       127.0.0.1:8885         special_localhost
    1010            1            0       127.0.0.1:8888                   monitor
```
