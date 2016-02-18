#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include "common.h"


int epoll_fd;

void peer_init() {
	epoll_fd = epoll_create1(0);
	assert(epoll_fd >= 0);
}

void peer_epoll_add(struct peer *peer, uint32_t events) {
	struct epoll_event ev = {
		.events = events,
		.data = {
			.ptr = peer,
		},
	};
	assert(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, peer->fd, &ev) == 0);
}

void peer_epoll_del(struct peer *peer) {
	assert(epoll_ctl(epoll_fd, EPOLL_CTL_DEL, peer->fd, NULL) == 0);
}

void peer_loop() {
	while (1) {
#define MAX_EVENTS 10
		struct epoll_event events[MAX_EVENTS];
		int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
		assert(nfds >= 0);

    for (int n = 0; n < nfds; n++) {
			struct peer *peer = events[n].data.ptr;
			peer->event_handler(peer);
		}
	}
}


void buf_init(struct buf *buf) {
	buf->start = 0;
	buf->length = 0;
}

ssize_t buf_fill(struct buf *buf, int fd) {
	if (buf->start + buf->length == BUF_LEN_MAX) {
		assert(buf->start > 0);
		memmove(buf->buf, buf_at(buf, 0), buf->length);
		buf->start = 0;
	}

	size_t space = BUF_LEN_MAX - buf->length - buf->start;
	ssize_t in = read(fd, buf_at(buf, buf->length), space);
	if (in <= 0) {
		return in;
	}
	buf->length += in;
	return in;
}

void buf_consume(struct buf *buf, size_t length) {
	assert(buf->length >= length);

	buf->length -= length;
	if (buf->length) {
		buf->start += length;
	} else {
		buf->start = 0;
	}
}


char *packet_type_names[] = {
	"Mode-S short",
	"Mode-S long",
};


uint64_t mlat_timestamp_scale_mhz_in(uint64_t timestamp, uint32_t mhz) {
	return timestamp * (MLAT_MHZ / mhz);
}

uint64_t mlat_timestamp_scale_width_in(uint64_t timestamp, uint64_t max, struct mlat_state *state) {
	if (timestamp < state->timestamp_last) {
		// Counter reset
		state->timestamp_generation += max;
	}

	state->timestamp_last = timestamp;
	return state->timestamp_generation + timestamp;
}

uint64_t mlat_timestamp_scale_in(uint64_t timestamp, uint64_t max, uint16_t mhz, struct mlat_state *state) {
	return mlat_timestamp_scale_mhz_in(mlat_timestamp_scale_width_in(timestamp, max, state), mhz);
}


uint32_t rssi_scale_in(uint32_t value, uint32_t max) {
	return value * (RSSI_MAX / max);
}


static uint8_t hex_table[256] = {0};
static char hex_char_table[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F', };

void hex_init() {
	for (int i = '0'; i <= '9'; i++) {
		hex_table[i] = i - '0';
	}
	for (int i = 'a'; i <= 'f'; i++) {
		hex_table[i] = 10 + i - 'a';
	}
	for (int i = 'A'; i <= 'F'; i++) {
		hex_table[i] = 10 + i - 'A';
	}
}

void hex_to_bin(uint8_t *out, const char *in, size_t bytes) {
	const uint8_t *in2 = (uint8_t *) in;
	for (size_t i = 0, j = 0; i < bytes; i++, j += 2) {
		out[i] = (hex_table[in2[j]] << 4) | hex_table[in2[j + 1]];
	}
}

uint64_t hex_to_int(const char *in, size_t bytes) {
	const uint8_t *in2 = (uint8_t *) in;
	uint64_t ret = 0;
	bytes *= 2;
	for (size_t i = 0; i < bytes; i++) {
		ret <<= 4;
		ret |= hex_table[in2[i]];
	}
	return ret;
}

void hex_from_bin(char *out, const uint8_t *in, size_t bytes) {
	for (size_t i = 0, j = 0; i < bytes; i++, j += 2) {
		out[j] = hex_char_table[in[i] >> 4];
		out[j + 1] = hex_char_table[in[i] & 0xf];
	}
}


void uuid_gen(char *out) {
	uuid_t uuid;
	uuid_generate(uuid);
	uuid_unparse(uuid, out);
}
