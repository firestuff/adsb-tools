#pragma once

#include <stdbool.h>

bool opts_add_connect_receive(const char *);
bool opts_add_connect_send(const char *);
bool opts_add_connect_send_receive(const char *);
bool opts_add_listen_receive(const char *);
bool opts_add_listen_send(const char *);
bool opts_add_listen_send_receive(const char *);
bool opts_add_file_read(const char *);
bool opts_add_file_write(const char *);
bool opts_add_file_write_read(const char *);
bool opts_add_file_append(const char *);
bool opts_add_file_append_read(const char *);
bool opts_add_exec_receive(const char *);
bool opts_add_exec_send(const char *);
bool opts_add_exec_send_receive(const char *);
bool opts_add_stdout(const char *);
bool opts_add_stdin(const char *);
