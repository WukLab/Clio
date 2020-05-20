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
	__START_COMMON_STATS,

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

	__START_HOST_STATS,

	/*
	 * Host-side specific
	 */

	STAT_NR_MIGRATED_VREGION,

	/* Reliable go-back-N layer */
	STAT_NET_GBN_NR_RX,
	STAT_NET_GBN_NR_RX_ERROR_NO_SESSION,
	STAT_NET_GBN_NR_RX_ERROR_UNKNOWN_TYPE,
	STAT_NET_GBN_NR_RX_ACK,
	STAT_NET_GBN_NR_RX_NACK,
	STAT_NET_GBN_NR_RX_DATA,
	STAT_NET_GBN_NR_TX_ACK,
	STAT_NET_GBN_NR_TX_NACK,
	STAT_NET_GBN_NR_TX_DATA,

	/* raw verbs layer */
	STAT_NET_RAW_VERBS_NR_TX,
	STAT_NET_RAW_VERBS_NR_RX,
	STAT_NET_RAW_VERBS_NR_RX_ZEROCOPY,
	STAT_NET_RAW_VERBS_NR_POST_RECVS,

	NR_STAT_TYPES,
};

/* This is.. stupid? */
static inline char *stat_type_string(enum STAT_TYPES item)
{
#define S(_OP) \
	case _OP:				return __stringify(_OP)

	switch (item) {
	S(__START_COMMON_STATS);
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

	S(__START_HOST_STATS);
	S(STAT_NR_MIGRATED_VREGION);
	S(STAT_NET_GBN_NR_RX);
	S(STAT_NET_GBN_NR_RX_ERROR_NO_SESSION);
	S(STAT_NET_GBN_NR_RX_ERROR_UNKNOWN_TYPE);
	S(STAT_NET_GBN_NR_RX_ACK);
	S(STAT_NET_GBN_NR_RX_NACK);
	S(STAT_NET_GBN_NR_RX_DATA);
	S(STAT_NET_GBN_NR_TX_ACK);
	S(STAT_NET_GBN_NR_TX_NACK);
	S(STAT_NET_GBN_NR_TX_DATA);

	S(STAT_NET_RAW_VERBS_NR_TX);
	S(STAT_NET_RAW_VERBS_NR_RX);
	S(STAT_NET_RAW_VERBS_NR_RX_ZEROCOPY);
	S(STAT_NET_RAW_VERBS_NR_POST_RECVS);

	default:
		return "invalid";
	}
	return "invalid";
}

#endif /* _UAPI_STAT_H_ */