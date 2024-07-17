// This source file's own header include.
#include "network.hpp"

// System includes.
#include <arpa/inet.h>
#include <cstddef>
#include <iostream>
#include <sys/socket.h>

std::optional<SocketFd> create_server_socket() {
  // Create the socket.
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    std::cerr << "Failed to create server socket\n";
    return std::nullopt;
  }

  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
      0) {
    std::cerr << "setsockopt failed\n";
    return std::nullopt;
  }

  // Bind the socket to the desired address/port.
  struct sockaddr_in server_addr {};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(6379);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) !=
      0) {
    std::cerr << "Failed to bind to port 6379\n";
    return std::nullopt;
  }

  // Listen on that socket.
  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return std::nullopt;
  }

  return SocketFd(server_fd);
}

SocketFd await_client_connection(const SocketFd server_fd) {
  struct sockaddr_in client_addr {};
  int client_addr_len = sizeof(client_addr);

  std::cout << "Waiting for a client to connect...\n";

  // NOLINTBEGIN(cppcoreguidelines-pro-type-cstyle-cast)
  int client_fd =
      accept4(static_cast<int>(server_fd), (struct sockaddr *)&client_addr,
              (socklen_t *)&client_addr_len, SOCK_CLOEXEC);
  // NOLINTEND(cppcoreguidelines-pro-type-cstyle-cast)
  std::cout << "Client " << client_fd << " connected!\n";
  return SocketFd(client_fd);
}

void send_to_client(const SocketFd client_fd, const std::string &message) {
  send(static_cast<int>(client_fd), message.c_str(), message.size(), 0);
}

std::optional<std::string>
receive_string_from_client(const SocketFd socket_fd,
                           const std::size_t max_size) {
  constexpr auto READ_SIZE = 1024UL;
  constexpr auto FLAGS = 0;

  std::string buffer{};
  buffer.reserve(std::min(max_size, READ_SIZE));

  // Loop and read one chunk of bytes (READ_SIZE) at a time until either the
  // socket runs out of data or we exceed the max size.
  while (buffer.size() < max_size) {
    // Prepare the buffer to read this many bytes.
    const auto num_bytes_to_read =
        std::min(max_size - buffer.size(), READ_SIZE);
    const auto current_size = buffer.size();
    buffer.resize(current_size + num_bytes_to_read);
    // The bytes will be inserted at the old back (but we can't just keep a
    // pointer to old back because resize() might reallocate and invalidate that
    // pointer).
    auto *insertion_point = buffer.data() + current_size;
    // Read the bytes into the buffer.
    const auto read_bytes = recv(static_cast<int>(socket_fd), insertion_point,
                                 num_bytes_to_read, FLAGS);
    // Check for an error.
    if (read_bytes < 0) {
      throw std::system_error(errno, std::system_category(), "recv failed");
    }
    // Connection has closed gracefully.
    if (read_bytes == 0) {
      return std::nullopt;
    }
    // Socket has no more bytes to read, we're done here.
    if (static_cast<std::size_t>(read_bytes) < num_bytes_to_read) {
      buffer.resize(current_size + read_bytes);
      return buffer;
    }
  }
  return buffer;
}
