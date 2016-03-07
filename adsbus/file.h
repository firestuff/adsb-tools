#pragma once

struct flow;

void file_cleanup(void);
void file_read_new(const char *, struct flow *, void *);
void file_write_new(const char *, struct flow *, void *);
void file_append_new(const char *, struct flow *, void *);
