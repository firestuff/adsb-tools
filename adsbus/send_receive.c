#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>

#include "flow.h"
#include "peer.h"
#include "receive.h"
#include "send.h"

#include "send_receive.h"

struct send_receive {
	struct peer peer;
	struct peer *on_close;
	uint8_t ref_count;
	struct list_head send_receive_list;
};

static struct list_head send_receive_head = LIST_HEAD_INIT(send_receive_head);

static void send_receive_new(int, void *, struct peer *);

static struct flow _send_receive_flow = {
	.name = "send_receive",
	.new = send_receive_new,
	.get_hello = send_get_hello,
	.ref_count = &peer_count_out_in,
};
struct flow *send_receive_flow = &_send_receive_flow;

static void send_receive_del(struct send_receive *send_receive) {
	list_del(&send_receive->send_receive_list);
	peer_call(send_receive->on_close);
	free(send_receive);
}

static void send_receive_on_close(struct peer *peer) {
	struct send_receive *send_receive = (struct send_receive *) peer;

	if (!--(send_receive->ref_count)) {
		send_receive_del(send_receive);
	}
}

static void send_receive_new(int fd, void *passthrough, struct peer *on_close) {
	struct send_receive *send_receive = malloc(sizeof(*send_receive));
	assert(send_receive);

	send_receive->peer.fd = -1;
	send_receive->peer.event_handler = send_receive_on_close;
	send_receive->on_close = on_close;
	send_receive->ref_count = 2;
	list_add(&send_receive->send_receive_list, &send_receive_head);

	flow_new(fd, send_flow, passthrough, (struct peer *) send_receive);
	int fd2 = fcntl(fd, F_DUPFD_CLOEXEC, 0);
	assert(fd2 >= 0);
	flow_new(fd2, receive_flow, NULL, (struct peer *) send_receive);
}

void send_receive_cleanup() {
	struct send_receive *iter, *next;
	list_for_each_entry_safe(iter, next, &send_receive_head, send_receive_list) {
		send_receive_del(iter);
	}
}
