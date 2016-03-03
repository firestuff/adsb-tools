#pragma once

struct flow;

void file_cleanup(void);
void file_fd_new(int, struct flow *, void *);
void file_read_new(char *, struct flow *, void *);
void file_write_new(char *, struct flow *, void *);
void file_append_new(char *, struct flow *, void *);
