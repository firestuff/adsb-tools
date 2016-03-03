#pragma once

void socket_pre_bind(int);
void socket_pre_listen(int);
void socket_ready(int);
void socket_ready_send(int);
void socket_connected_send(int);
void socket_connected_receive(int);
