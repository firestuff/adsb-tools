#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include "common.h"
#include "json.h"
#include "stats.h"
#include "client.h"

struct client {
	struct peer peer;
	char id[UUID_LEN];
	struct serializer *serializer;
	struct client *prev;
	struct client *next;
};

typedef void (*serializer)(struct packet *, struct buf *);
struct serializer {
	char *name;
	serializer serialize;
	struct client *client_head;
} serializers[] = {
	{
		.name = "json",
		.serialize = json_serialize,
	},
	{
		.name = "stats",
		.serialize = stats_serialize,
	},
};
#define NUM_SERIALIZERS (sizeof(serializers) / sizeof(*serializers))

static void client_hangup(struct client *client) {
	fprintf(stderr, "C %s (%s): Client disconnected\n", client->id, client->serializer->name);
	if (client->prev) {
		client->prev->next = client->next;
	} else {
		client->serializer->client_head = client->next;
	}
	if (client->next) {
		client->next->prev = client->prev;
	}
	close(client->peer.fd);
	free(client);
}

static void client_hangup_wrapper(struct peer *peer) {
	client_hangup((struct client *) peer);
}

static bool client_hello(int fd, struct serializer *serializer) {
	struct buf buf = BUF_INIT;
	serializer->serialize(NULL, &buf);
	if (buf.length == 0) {
		return true;
	}
	if (write(fd, buf_at(&buf, 0), buf.length) != buf.length) {
		return false;
	}
	return true;
}

void client_init() {
	signal(SIGPIPE, SIG_IGN);
}

struct serializer *client_get_serializer(char *name) {
	for (int i = 0; i < NUM_SERIALIZERS; i++) {
		if (strcasecmp(serializers[i].name, name) == 0) {
			return &serializers[i];
		}
	}
	return NULL;
}

void client_add(int fd, struct serializer *serializer) {
	int flags = fcntl(fd, F_GETFL, 0);
	assert(flags >= 0);
	flags |= O_NONBLOCK;
	assert(fcntl(fd, F_SETFL, flags) == 0);

	if (!client_hello(fd, serializer)) {
		fprintf(stderr, "C xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx: Failed to write hello to client\n");
		return;
	}

	struct client *client = malloc(sizeof(*client));
	assert(client);

	client->peer.fd = fd;
	client->peer.event_handler = client_hangup_wrapper;

	uuid_gen(client->id);
	client->serializer = serializer;
	client->prev = NULL;
	client->next = serializer->client_head;
	serializer->client_head = client;

	// Only listen for hangup
	peer_epoll_add((struct peer *) client, EPOLLIN);

	fprintf(stderr, "C %s (%s): New client\n", client->id, serializer->name);
}

void client_add_wrapper(int fd, void *passthrough) {
	client_add(fd, (struct serializer *) passthrough);
}

void client_write(struct packet *packet) {
	for (int i = 0; i < NUM_SERIALIZERS; i++) {
		struct serializer *serializer = &serializers[i];
		if (serializer->client_head == NULL) {
			continue;
		}
		struct buf buf = BUF_INIT;
		serializer->serialize(packet, &buf);
		if (buf.length == 0) {
			continue;
		}
		struct client *client = serializer->client_head;
		while (client) {
			if (write(client->peer.fd, buf_at(&buf, 0), buf.length) == buf.length) {
				client = client->next;
			} else {
				struct client *next = client->next;
				client_hangup(client);
				client = next;
			}
		}
	}
}

void client_print_usage() {
	fprintf(stderr, "\nSupported output formats:\n");
	for (int i = 0; i < NUM_SERIALIZERS; i++) {
		fprintf(stderr, "\t%s\n", serializers[i].name);
	}
}
