#pragma once

#include <stdint.h>
#include <sys/epoll.h>


//////// peer

// All specific peer structs must be castable to this.
struct peer;
typedef void (*peer_event_handler)(struct peer *);
struct peer {
	int fd;
	peer_event_handler event_handler;
};
void peer_init();
void peer_epoll_add(struct peer *, uint32_t);
void peer_epoll_del(struct peer *);
void peer_loop();


//////// packet

#define DATA_LEN_MAX 14
struct packet {
	enum {
		MODE_S_SHORT,
		MODE_S_LONG,
	} type;
	#define NUM_TYPES 2
	uint8_t payload[DATA_LEN_MAX];
	uint64_t mlat_timestamp;
	uint32_t rssi;
};
extern char *packet_type_names[];


//////// mlat

#define MLAT_MHZ 120
// Use the signed max to avoid problems with some consumers; it's large enough to not matter.
#define MLAT_MAX INT64_MAX
#define RSSI_MAX UINT32_MAX

struct mlat_state {
	uint64_t timestamp_last;
	uint64_t timestamp_generation;
};

uint64_t mlat_timestamp_scale_in(uint64_t, uint64_t, uint16_t, struct mlat_state *);
uint64_t mlat_timestamp_scale_out(uint64_t, uint64_t, uint16_t);


//////// rssi

uint32_t rssi_scale_in(uint32_t, uint32_t);
uint32_t rssi_scale_out(uint32_t, uint32_t);
