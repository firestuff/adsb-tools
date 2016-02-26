#pragma once

#include <stdbool.h>

#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wgnu-statement-expression"
#pragma GCC diagnostic ignored "-Wlanguage-extension-token"

#define offset_of(type, member) ((size_t) &((type *) NULL)->member)

#define container_of(ptr, type, member) ({ \
		typeof( ((type *) NULL)->member ) *__mptr = (ptr); \
		(type *)( (char *)__mptr - offset_of(type, member) );})

struct list_head {
	struct list_head *next;
	struct list_head *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define list_entry(ptr, type, member) \
		container_of(ptr, type, member)

#define list_for_each_entry(pos, head, member) \
		for (pos = list_entry((head)->next, typeof(*pos), member); \
				&pos->member != (head); \
				pos = list_entry(pos->member.next, typeof(*pos), member))

#define list_for_each_entry_safe(pos, n, head, member) \
		for (pos = list_entry((head)->next, typeof(*pos), member), \
					n = list_entry(pos->member.next, typeof(*pos), member); \
				&pos->member != (head); \
				pos = n, n = list_entry(n->member.next, typeof(*n), member))

void list_head_init(struct list_head *);
bool __attribute__ ((warn_unused_result)) list_is_empty(const struct list_head *);
void list_add(struct list_head *, struct list_head *);
void list_del(struct list_head *);
