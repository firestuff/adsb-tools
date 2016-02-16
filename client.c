#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <strings.h>
#include <stdio.h>

#include "json.h"
#include "client.h"

struct client {
	char id[UUID_LEN];
	int fd;
	struct client *next;
};

typedef size_t (*serializer)(struct packet *, char *);
struct serializer {
	char *name;
	serializer serialize;
	struct client *client_head;
} serializers[] = {
	{
		.name = "json",
		.serialize = json_serialize,
		.client_head = NULL,
	},
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
	char buf[SERIALIZE_LEN];
	size_t len = serializer->serialize(NULL, buf);
	if (len == 0) {
		return true;
	}
	if (write(fd, buf, len) != len) {
		fprintf(stderr, "Failed to write hello to client\n");
		return false;
	}
	return true;
}

void client_add(int fd, struct serializer *serializer) {
	if (!client_hello(fd, serializer)) {
		return;
	}

	struct client *client = malloc(sizeof(*client));
	assert(client);

	uuid_gen(client->id);
	client->fd = fd;
	client->next = serializer->client_head;
	serializer->client_head = client;

	fprintf(stderr, "%s (%s): New client\n", client->id, serializer->name);
}

void client_write(struct packet *packet) {
	for (int i = 0; i < NUM_SERIALIZERS; i++) {
		struct serializer *serializer = &serializers[i];
		if (serializer->client_head == NULL) {
			continue;
		}
		char buf[SERIALIZE_LEN];
		size_t len = serializer->serialize(packet, buf);
		if (len == 0) {
			continue;
		}
		struct client *client = serializer->client_head, *prev_client = NULL;
		while (client) {
			if (write(client->fd, buf, len) == len) {
				prev_client = client;
				client = client->next;
			} else {
				fprintf(stderr, "%s (%s): Client disconnected\n", client->id, serializer->name);
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
