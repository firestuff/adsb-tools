#pragma once

void socket_pre_bind(int);
void socket_pre_listen(int);
void socket_connected(int);
void socket_connected_send(int);
void socket_send(int);
void socket_receive(int);
