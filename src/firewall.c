// SPDX-License-Identifier: BSD-3-Clause

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "consumer.h"
#include "log/log.h"
#include "packet.h"
#include "producer.h"
#include "ring_buffer.h"
#include "utils.h"

#define SO_RING_SZ (PKT_SZ * 1000)

pthread_mutex_t MUTEX_LOG;

void log_lock(bool lock, void *udata)
{
	pthread_mutex_t *LOCK = (pthread_mutex_t *)udata;

	if (lock)
		pthread_mutex_lock(LOCK);
	else
		pthread_mutex_unlock(LOCK);
}

void __attribute__((constructor)) init() {
	pthread_mutex_init(&MUTEX_LOG, NULL);
	log_set_lock(log_lock, &MUTEX_LOG);
}

void __attribute__((destructor)) dest() { pthread_mutex_destroy(&MUTEX_LOG); }

int main(int argc, char **argv)
{
	so_ring_buffer_t ring_buffer;
	int num_consumers, rc;
	pthread_t *thread_ids = NULL;

	if (argc < 4) {
		fprintf(stderr,
				"Usage %s <input-file> <output-file> <num-consumers:1-32>\n",
				argv[0]);
		exit(EXIT_FAILURE);
	}

	rc = ring_buffer_init(&ring_buffer, SO_RING_SZ);
	DIE(rc < 0, "ring_buffer_init");

	num_consumers = strtol(argv[3], NULL, 10);

	if (num_consumers <= 0 || num_consumers > 32) {
		fprintf(stderr, "num-consumers [%d] must be in the interval [1-32]\n",
				num_consumers);
		exit(EXIT_FAILURE);
	}

	thread_ids = calloc(num_consumers, sizeof(pthread_t));
	DIE(thread_ids == NULL, "calloc pthread_t");

	/* create consumer threads */
	int d = atoi(argv[3]);

	if (10 % d)
		create_consumers(thread_ids, num_consumers + 1, &ring_buffer, argv[2]);
	else
		create_consumers(thread_ids, num_consumers, &ring_buffer, argv[2]);

	/* start publishing data */
	publish_data(&ring_buffer, argv[1]);

	/* TODO: wait for child threads to finish execution*/
	for (int i = 0; i < num_consumers; i++)
		pthread_join(thread_ids[i], NULL);

	free(thread_ids);
	ring_buffer_destroy(&ring_buffer);

	return 0;
}
