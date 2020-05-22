/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */
#ifndef _HOST_API_KVS_H_
#define _HOST_API_KVS_H_

#define MAX_KEY_SIZE 8

int legomem_kvs_create(struct legomem_context *ctx, uint16_t key_size,
		      char *key, uint16_t value_size, void *value);
int legomem_kvs_update(struct legomem_context *ctx, uint16_t key_size,
		       char *key, uint16_t value_size, void *value);
int legomem_kvs_read(struct legomem_context *ctx, uint16_t key_size,
		     char *key, uint16_t value_size, void *value);
int legomem_kvs_delete(struct legomem_context *ctx, uint16_t key_size, 
			char *key);

#endif /* _HOST_API_KVS_H_ */
