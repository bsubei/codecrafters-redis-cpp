#pragma once

#include <optional>
#include <string>

// Creates a socket, binds it to port 6379, listens on it, and returns its file descriptor.
std::optional<int> create_server_socket();

// Blocks on the given socket fd until it connects to a client, and returns the client's socket file descriptor.
int await_client_connection(const int server_fd);

// Sends the given string message over the socket specified by the client file descriptor.
void send_to_client(const int client_fd, const std::string &message);