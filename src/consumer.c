// SPDX-License-Identifier: BSD-3-Clause

#include "consumer.h"

#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

#include "packet.h"
#include "ring_buffer.h"
#include "utils.h"

pthread_barrier_t b;
pthread_mutex_t counter_lock;
pthread_cond_t counter_cond;

typedef struct log_file_fields {
	char *action;
	size_t hash;
	unsigned long timestamp;
	int stat;
} log_file_fields;

typedef struct arr_fields {
	log_file_fields *lgf;
	int num_mx;
	int size;
} arr_fields;

arr_fields *init_struct(arr_fields *arf, int nr)
{
	arf = (arr_fields *)malloc(sizeof(arr_fields));
	arf->lgf = (log_file_fields *)malloc(nr * sizeof(log_file_fields));
	for (int i = 0; i < nr; i++)
		arf->lgf[i].action = (char *)malloc(5 * sizeof(char));
	arf->num_mx = nr;
	arf->size = 0;
	return arf;
}

int cmp(const void *a, const void *b)
{
	log_file_fields *pa = (log_file_fields *)a;
	log_file_fields *pb = (log_file_fields *)b;

	return pa->timestamp - pb->timestamp;
}

arr_fields *arf;

void *consumer_thread(so_consumer_ctx_t *ctx)
{
	/* TODO: implement consumer thread */
	so_packet_t pack;
	char out_buf[PKT_SZ];
	int len, size;

	pthread_mutex_lock(ctx->const_lock);
	size = ctx->id;
	pthread_mutex_unlock(ctx->const_lock);

	while (1) {
		if (ctx->producer_rb->stop == 1 && ctx->producer_rb->len == 0) {
			sem_post(&ctx->producer_rb->full);
			break;
		}
		sem_wait(&ctx->producer_rb->full);
		if (ring_buffer_dequeue(ctx->producer_rb, &pack, sizeof(so_packet_t)) ==
			1) {
			size_t hash = packet_hash(&pack);
			so_action_t action = process_packet(&pack);

			pthread_mutex_lock(ctx->lock);
			arf->lgf[size].hash = hash;
			memcpy(arf->lgf[size].action, RES_TO_STR(action), 5);
			arf->lgf[size].timestamp = pack.hdr.timestamp;
			arf->lgf[size].stat = 1;
			pthread_mutex_unlock(ctx->lock);

			pthread_barrier_wait(&b);

			pthread_mutex_lock(ctx->lock);
			qsort(arf->lgf, arf->num_mx, sizeof(log_file_fields), cmp);
			pthread_mutex_unlock(ctx->lock);

			pthread_mutex_lock(ctx->lock);
			if (arf->lgf[size].stat == 1) {
				for (int i = 0; i < arf->num_mx; i++) {
					len = snprintf(out_buf, 256, "%s %016lx %lu\n",
								   arf->lgf[i].action, arf->lgf[i].hash,
								   arf->lgf[i].timestamp);
					write(ctx->f, out_buf, len);
					arf->lgf[i].stat = 0;
				}
			}

			pthread_mutex_unlock(ctx->lock);

			pthread_barrier_wait(&b);

		} else if (ctx->producer_rb->stop) {
			sem_post(&ctx->producer_rb->full);
			break;
		}
		sem_post(&ctx->producer_rb->empty);
	}
	return NULL;
}

int create_consumers(pthread_t *tids, int num_consumers,
					 struct so_ring_buffer_t *rb, const char *out_filename)
{
	pthread_mutex_t *log_lock =
		(pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_t *ct_lock =
		(pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(log_lock, NULL);
	pthread_mutex_init(ct_lock, NULL);
	pthread_barrier_init(&b, NULL, num_consumers);
	arf = init_struct(arf, num_consumers);
	int f = open(out_filename, O_RDWR | O_CREAT | O_TRUNC, 0666);

	for (int i = 0; i < num_consumers; i++) {
		/* TODO: Launch consumer threads */
		so_consumer_ctx_t *ctx =
			(so_consumer_ctx_t *)malloc(sizeof(so_consumer_ctx_t));

		ctx->producer_rb = rb;
		ctx->out_filename = out_filename;
		ctx->lock = log_lock;
		ctx->const_lock = ct_lock;
		ctx->f = f;
		ctx->id = i;
		pthread_create(tids + i, NULL, (void *(*)(void *))consumer_thread, ctx);
	}

	return num_consumers;
}
