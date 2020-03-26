/*
 * TODO
 *
 * We will remove thise manual coding later.
 * We will build a simple membership management during runtime.
 * A UDP port will receive and broadcast this info.
 */
struct endpoint_info ei_wuklab02 = {
	.mac		= { 0xe4, 0x1d, 0x2d, 0xb2, 0xba, 0x51 },
	.ip		= 0xc0a80102, /* 192.168.1.2 */
	.udp_port	= 8888,
};
struct endpoint_info ei_wuklab05 = {
	.mac		= { 0xe4, 0x1d, 0x2d, 0xe4, 0x81, 0x51 },
	.ip		= 0xc0a80105, /* 192.168.1.5 */
	.udp_port	= 8888,
};
struct endpoint_info ei_wuklab06 = {
	.mac		= { 0xe4, 0x1d, 0x2d, 0xb3, 0x54, 0x11 },
	.ip		= 0xc0a80106, /* 192.168.1.6 */
	.udp_port	= 8888,
};
struct endpoint_info board_0 = {
	.mac		= { 0xe4, 0x1d, 0x2d, 0x88, 0x77, 0x51 },
	.ip		= 0xc0a801c8, /* 192.168.1.200 */
	.udp_port	= 1234,
};
struct endpoint_info board_1 = {
	.mac		= { 0xe4, 0x1d, 0x2d, 0xb2, 0x00, 0x00 },
	.ip		= 0xc0a80180,
	.udp_port	= 1234,
};
