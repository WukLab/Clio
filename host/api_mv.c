/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */

#include <uapi/vregion.h>
#include <uapi/compiler.h>
#include <uapi/sched.h>
#include <uapi/list.h>
#include <uapi/err.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "core.h"
#include "net/net.h"

/*
 * Request/Response Packet Layout:
 * [ETH/IP/UDP/GBN/Lego/op_mvobj/.../]
 *
 * TODO
 * Figrue out struct layout, opcode position.
 * Build APIs etc.
 */
