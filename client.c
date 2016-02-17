#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "json.h"
#include "stats.h"
#include "client.h"

struct client {
	char id[UUID_LEN];
	int fd;
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
	}
};
#define NUM_SERIALIZERS (sizeof(serializers) / sizeof(*serializers))


struct serializer *client_get_serializer(char *name) {
	for (int i = 0; i < NUM_SERIALIZERS; i++) {
		if (strcasecmp(serializers[i].name, name) == 0) {
			return &serializers[i];
		}
	}
	return NULL;
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

	uuid_gen(client->id);
	client->fd = fd;
	client->next = serializer->client_head;
	serializer->client_head = client;

	fprintf(stderr, "C %s (%s): New client\n", client->id, serializer->name);
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
		struct client *client = serializer->client_head, *prev_client = NULL;
		while (client) {
			if (write(client->fd, buf_at(&buf, 0), buf.length) == buf.length) {
				prev_client = client;
				client = client->next;
			} else {
				fprintf(stderr, "C %s (%s): Client disconnected\n", client->id, serializer->name);
				if (prev_client) {
					prev_client->next = client->next;
				} else {
					serializer->client_head = client->next;
				}
				struct client *del = client;
				client = client->next;
				free(del);
			}
		}
	}
}
