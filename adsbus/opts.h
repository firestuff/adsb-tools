#pragma once

#include <stdbool.h>

bool opts_add_connect_receive(char *);
bool opts_add_connect_send(char *);
bool opts_add_listen_receive(char *);
bool opts_add_listen_send(char *);
bool opts_add_file_read(char *);
bool opts_add_file_write(char *);
bool opts_add_file_append(char *);
bool opts_add_exec_receive(char *);
bool opts_add_exec_send(char *);
bool opts_add_stdout(char *);
bool opts_add_stdin(char *);
