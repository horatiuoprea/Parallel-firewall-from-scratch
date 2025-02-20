/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef __SO_CONSUMER_H__
#define __SO_CONSUMER_H__

#include "packet.h"
#include "ring_buffer.h"

typedef struct so_consumer_ctx_t {
	struct so_ring_buffer_t *producer_rb;

	/* TODO: add synchronization primitives for timestamp ordering */
	const char *out_filename;
	pthread_mutex_t *lock, *const_lock;
	int id, f;
} so_consumer_ctx_t;

int create_consumers(pthread_t *tids, int num_consumers, so_ring_buffer_t *rb,
					 const char *out_filename);

#endif /* __SO_CONSUMER_H__ */
