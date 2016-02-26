#include <stdlib.h>

#include "list.h"

void list_head_init(struct list_head *head) {
	head->next = head->prev = head;
}

bool list_is_empty(const struct list_head *head) {
	return head->next == head;
}

void list_add(struct list_head *new, struct list_head *head) {
	new->next = head;
	new->prev = head->prev;
	new->prev->next = new;
	head->prev = new;
}

void list_del(struct list_head *entry) {
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
	entry->prev = entry->next = NULL;
}
