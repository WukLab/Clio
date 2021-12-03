/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */

#ifndef _HOST_CONFIG_H_
#define _HOST_CONFIG_H_

#if 0
# define LEGOMEM_DEBUG
#endif

/*
 * Once enabled, the system will dump all transmitted/received packets.
 * Use git grep to find where they are used.
 */
#if 0
# define CONFIG_NETWORK_DUMP_TX
#endif
#if 1
# define CONFIG_NETWORK_DUMP_RX
#endif

/*
 * Choose which transport layer to use.
 * GBN: go-back-n ack-based retranmission, no CC
 * RPC: RPC-style, no ack, with an AIMD CC.
 */
#if 0
# define CONFIG_TRANSPORT_GBN
#else
# define CONFIG_TRANSPORT_RPC
#endif

/*
 * Once enabled, we will track the read/write dependency.
 * If current access address is depedent on a prior in-flight access,
 * the current acess will be blocked until the prior request has finished.
 *
 * This is not controlly memory consistency model.
 * It could be used along side weak consistency model.
 */
#if 0
#define CONFIG_MEMORY_MODEL_ENABLE_DEPENDENCY_TRACKING
#endif

/*
 * Once enabled, the client side will track the number of outstanding
 * reads and writes. And fence will use those numbers.
 *
 * Tracking has a cost, each operation is an atomic operation.
 * That's why we make this optional.
 */
#if 0
#define CONFIG_ENABLE_FENCE
#endif

#endif /* _HOST_CONFIG_H_ */
