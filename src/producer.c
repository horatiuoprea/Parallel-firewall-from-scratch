// SPDX-License-Identifier: BSD-3-Clause

#include "producer.h"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "packet.h"
#include "ring_buffer.h"
#include "utils.h"

void publish_data(so_ring_buffer_t *rb, const char *filename)
{
	char buffer[PKT_SZ];
	ssize_t sz;
	int fd;

	fd = open(filename, O_RDONLY);
	DIE(fd < 0, "open");

	while ((sz = read(fd, buffer, PKT_SZ)) != 0) {
		DIE(sz != PKT_SZ, "packet truncated");

		/* enequeue packet into ring buffer */
		// sem_wait(&rb->empty);
		ring_buffer_enqueue(rb, buffer, sz);
		// sem_post(&rb->full);
	}

	ring_buffer_stop(rb);
}
