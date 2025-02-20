// SPDX-License-Identifier: BSD-3-Clause

#include "ring_buffer.h"

int ring_buffer_init(so_ring_buffer_t *ring, size_t cap)
{
	ring->cap = cap;
	ring->data = (char *)malloc(cap * sizeof(char));
	if (!ring->data)
		return 0;

	ring->len = 0;
	ring->read_pos = 0;
	ring->write_pos = 0;
	ring->stop = 0;

	if (pthread_mutex_init(&ring->lock, NULL) != 0)
		return 0;
	if (sem_init(&ring->empty, 0, ring->cap / 256) != 0)
		return 0;
	if (sem_init(&ring->full, 0, 0) != 0)
		return 0;

	return 1;
}

ssize_t ring_buffer_enqueue(so_ring_buffer_t *ring, void *data, size_t size)
{
	sem_wait(&ring->empty);
	pthread_mutex_lock(&ring->lock);

	if (ring->len + size > ring->cap) {
		pthread_mutex_unlock(&ring->lock);
		return 0;
	}

	if (ring->write_pos + size <= ring->cap) {
		memcpy((void *)(ring->data + ring->write_pos), data, size);
		ring->write_pos = (ring->write_pos + size) % ring->cap;
	} else {
		size_t first_len = ring->cap - ring->write_pos;

		memcpy((void *)(ring->data + ring->write_pos), data, first_len);
		memcpy((void *)(ring->data), data + first_len, size - first_len);
		ring->write_pos = size - first_len;
	}

	ring->len += size;

	pthread_mutex_unlock(&ring->lock);
	sem_post(&ring->full);

	return 1;
}

ssize_t ring_buffer_dequeue(so_ring_buffer_t *ring, void *data, size_t size)
{
	pthread_mutex_lock(&ring->lock);

	if (ring->len < size) {
		pthread_mutex_unlock(&ring->lock);
		return 0;
	}

	if (ring->read_pos + size <= ring->cap) {
		memcpy(data, (void *)(ring->data + ring->read_pos), size);
		ring->read_pos = (ring->read_pos + size) % ring->cap;
	} else {
		size_t first_len = ring->cap - ring->read_pos;

		memcpy(data, (void *)(ring->data + ring->read_pos), first_len);
		memcpy(data + first_len, (void *)(ring->data), size - first_len);
		ring->read_pos = size - first_len;
	}

	ring->len -= size;

	pthread_mutex_unlock(&ring->lock);

	return 1;
}

void ring_buffer_stop(so_ring_buffer_t *ring)
{
	pthread_mutex_lock(&ring->lock);
	ring->stop = 1;
	pthread_mutex_unlock(&ring->lock);
}

void ring_buffer_destroy(so_ring_buffer_t *ring)
{
	free(ring->data);
	pthread_mutex_destroy(&ring->lock);
	sem_destroy(&ring->empty);
	sem_destroy(&ring->full);
}
