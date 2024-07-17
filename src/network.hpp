#pragma once

// System includes.
#include <cstddef>
#include <optional>
#include <string>

// A strong typedef that can explicitly convert to the underlying type (int).
class SocketFd {
public:
  explicit SocketFd(int val_in) : value(val_in) {}
  explicit operator int() const { return value; }

private:
  int value;
};

// Creates a socket, binds it to port 6379, listens on it, and returns its file
// descriptor.
std::optional<SocketFd> create_server_socket();

// Blocks on the given socket fd until it connects to a client, and returns the
// client's socket file descriptor.
SocketFd await_client_connection(const SocketFd server_fd);

// Sends the given string message over the socket specified by the client file
// descriptor.
void send_to_client(const SocketFd client_fd, const std::string &message);

// Waits to receive data from the given client and returns it as a string (or
// nullopt if the client closes the connection).
std::optional<std::string>
receive_string_from_client(const SocketFd socket_fd,
                           const std::size_t max_size = 10000000);
