#include "Server.hpp"

#include "Command.hpp"
#include "Parser.hpp"

#include <cassert>
#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

namespace
{
  std::optional<int> create_server_socket()
  {
    // Create the socket.
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
      std::cerr << "Failed to create server socket\n";
      return std::nullopt;
    }

    // Since the tester restarts your program quite often, setting SO_REUSEADDR
    // ensures that we don't run into 'Address already in use' errors
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
      std::cerr << "setsockopt failed\n";
      return 1;
    }

    // Bind the socket to the desired address/port.
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(6379);
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
    {
      std::cerr << "Failed to bind to port 6379\n";
      return std::nullopt;
    }

    // Listen on that socket.
    int connection_backlog = 5;
    if (listen(server_fd, connection_backlog) != 0)
    {
      std::cerr << "listen failed\n";
      return std::nullopt;
    }

    return server_fd;
  }

  int await_client_connection(const int server_fd)
  {
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);

    std::cout << "Waiting for a client to connect...\n";

    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);
    std::cout << "Client " << client_fd << " connected!\n";
    return client_fd;
  }

  void send_to_client(const int client_fd, const std::string &message)
  {
    send(client_fd, message.c_str(), message.size(), 0);
  }

  // Reads from a socket until EOF and returns the Commands that were parsed in between newline tokens.
  std::vector<Command> read_commands_from_socket(const int socket_fd)
  {
    std::vector<Command> commands{};

    // Read from the socket 1KB at a time.
    // TODO this function does not correctly handle when a command string happens to span two of these 1KB buffers.
    // TODO this function blocks forever if the socket happens to have exactly 1KB of data (we think there's more to read).
    constexpr size_t BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE];
    // Make this string once outside (and make it big enough), then reuse it in the loop.
    std::string buffer_s{};
    buffer_s.reserve(BUFFER_SIZE);
    int bytes_read = 0;
    // Keep reading until the socket gives us no more data.
    // NOTE: we use a do-while here even though it's weird, because we want to attempt to read at least once.
    do
    {
      // TODO this won't work because I have to parse headers

      // Reset the contents of the buffers (no reallocation needed).
      memset(buffer, 0, sizeof(buffer));
      buffer_s.clear();
      // Consume the data on the socket.
      bytes_read = recv(socket_fd, buffer, sizeof(buffer), 0);
      std::cout << "BYTES READ: " << bytes_read << "\n";
      if (bytes_read == -1)
      {
        // Something went wrong, just return whatever we got up to this point.
        return commands;
      }
      // Convert the buffer to a string.
      buffer_s.append(buffer, bytes_read); // TODO is this going to reallocate?
      std::cout << "buffer_s: " << buffer_s << "\n";

      // Keep looping until we consume all the Commands in between newlines that are in buffer_s.
      size_t start_pos = 0UL;
      bool found_newline = false;
      do
      {
        const size_t newline_pos = buffer_s.find('\n', start_pos);
        std::cout << "buffer_s: '" << buffer_s << "', with newline_pos: " << newline_pos << "\n";
        found_newline = newline_pos != std::string::npos;

        // If we found a newline, only parse up to the newline. Otherwise, parse the rest of the string.
        size_t end_pos = found_newline ? newline_pos - start_pos - 1 : std::string::npos;
        auto cmd = parse_command(buffer_s.substr(start_pos, end_pos));
        if (cmd)
        {
          commands.push_back(*cmd);
        }
        // TODO handle can't parse_command. At least print out an error.

        // Reset start_pos so we grab the next token in between newlines.
        start_pos = newline_pos + 1;
      } while (found_newline);

    } while (bytes_read == BUFFER_SIZE);

    return commands;
  }
} // anonymous namespace

Server::Server()
{
  socket_fd = create_server_socket();
}

bool Server::is_ready() const
{
  return socket_fd.has_value();
}

void Server::run()
{
  assert(is_ready());
  const int client_fd = await_client_connection(*socket_fd);

  // TODO going over the RESP protocol:
  // https://redis.io/docs/latest/develop/reference/protocol-spec/
  // TODO only deal with simple request-response model for now.
  /*
  auto cmds = read_commands_from_socket(client_fd);

  // TODO always respond with PONG for now. Need to change response based on received commands.
  for (const auto cmd : cmds)
  {
    const std::string message{"+PONG\r\n"};
    send_to_client(client_fd, message);
  }
  */

  const auto request = RESP::parse_request_from_client(client_fd);
  std::cout << "Parsed Request: " << RESP::Request::to_string(request.command) << std::endl;
  const auto response = RESP::generate_response(request);
  std::cout << "Generated Response: " << response.data << std::endl;
  send_to_client(client_fd, RESP::response_to_string(response));
}

Server::~Server()
{
  if (socket_fd)
  {
    close(*socket_fd);
  }
}
