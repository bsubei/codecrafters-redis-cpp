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

  void handle_client_connection(const int client_fd)
  {
    while (true)
    {
      // TODO going over the RESP protocol:
      // https://redis.io/docs/latest/develop/reference/protocol-spec/
      // TODO only deal with simple request-response model for now.
      const auto request = RESP::parse_request_from_client(client_fd);
      if (!request)
      {
        std::cout << "Closing connection with " << client_fd << std::endl;
        break;
      }

      for (const auto &command : request->commands)
      {
        std::cout << "Parsed Command: " << RESP::Request::to_string(command) << std::endl;
      }
      const auto responses = RESP::generate_responses(*request);
      for (const auto &response : responses)
      {
        std::cout << "Generated Response: " << response.data << std::endl;
        send_to_client(client_fd, RESP::response_to_string(response));
      }
    }
  }

  // Just waits on the future and swallows exceptions (prints them out to cerr).
  void consume_async_task(std::future<void> &f)
  {
    try
    {
      // NOTE: because the futures are created with the async launch policy, we can immediately call get() on them and don't have to check their validity or status.
      f.get();
    }
    catch (const std::future_error &future_error)
    {
      std::cerr << "Exception from async task: " << future_error.what() << std::endl;
    }
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
  constexpr auto ASYNC_MAX_LIMIT = 100;
  try
  {
    while (is_ready())
    {
      // If we've ended up creating too many simultaneous connections, wait until the oldest connection closes.
      // This makes sure the server doesn't get too swamped with incoming client connections and makes them wait.
      if (futures.size() >= ASYNC_MAX_LIMIT)
      {
        consume_async_task(futures.front());
        futures.pop_front();
      }

      // Create a new connection and spawn off an async task to handle this client.
      const int client_fd = await_client_connection(*socket_fd);
      futures.push_back(std::async(std::launch::async, handle_client_connection, client_fd));
    }
  }
  catch (const std::exception &server_error)
  {
    std::cerr << "Exception thrown while server was handling new incoming client connections: " << server_error.what() << std::endl;
  }
}

Server::~Server()
{
  // If the server is shutting down, wait for all the client connection tasks to finish up.
  for (auto &f : futures)
  {
    consume_async_task(f);
  }

  if (socket_fd)
  {
    close(*socket_fd);
  }
}
