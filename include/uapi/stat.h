/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */
#ifndef _UAPI_STAT_H_
#define _UAPI_STAT_H_

#include <uapi/stringify.h>

/*
 * Each counter is 8B long.
 */
enum STAT_TYPES {
	/*
	 * Network group
	 */
	STAT_NET_NR_RX,
	STAT_NET_NR_TX,

	/* Reliable go-back-N layer */
	STAT_NET_NR_ACK_RX,
	STAT_NET_NR_ACK_TX,
	STAT_NET_NR_ACK_ERROR,

	/*
	 * Core legomem group
	 */
	STAT_LEGOMEM_NR_READ,
	STAT_LEGOMEM_NR_WRITE,
	STAT_LEGOMEM_NR_ALLOC,
	STAT_LEGOMEM_NR_FREE,
	STAT_LEGOMEM_NR_OPEN_SESSION_TX,
	STAT_LEGOMEM_NR_OPEN_SESSION_RX,
	STAT_LEGOMEM_NR_CLOSE_SESSION_TX,
	STAT_LEGOMEM_NR_CLOSE_SESSION_RX,
	STAT_LEGOMEM_NR_MIGRATION_TX,
	STAT_LEGOMEM_NR_MIGRATION_RX,

	NR_STAT_TYPES,
};

/* This is.. stupid? */
static inline char *stat_type_string(enum STAT_TYPES item)
{
#define S(_OP) \
	case _OP:				return __stringify(_OP)

	switch (item) {
	S(STAT_NET_NR_RX);
	S(STAT_NET_NR_TX);
	S(STAT_NET_NR_ACK_RX);
	S(STAT_NET_NR_ACK_TX);
	S(STAT_NET_NR_ACK_ERROR);
	S(STAT_LEGOMEM_NR_READ);
	S(STAT_LEGOMEM_NR_WRITE);
	S(STAT_LEGOMEM_NR_ALLOC);
	S(STAT_LEGOMEM_NR_FREE);
	S(STAT_LEGOMEM_NR_OPEN_SESSION_TX);
	S(STAT_LEGOMEM_NR_OPEN_SESSION_RX);
	S(STAT_LEGOMEM_NR_CLOSE_SESSION_TX);
	S(STAT_LEGOMEM_NR_CLOSE_SESSION_RX);
	S(STAT_LEGOMEM_NR_MIGRATION_TX);
	S(STAT_LEGOMEM_NR_MIGRATION_RX);
	case NR_STAT_TYPES:			return "invalid";
	}
	return "invalid";
}

#endif /* _UAPI_STAT_H_ */
