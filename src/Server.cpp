#include "Server.hpp"

#include "Parser.hpp"

#include <algorithm>
#include <chrono>
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

  void process_request(const RESP::Request &request, Cache &cache)
  {
    if (request.command == RESP::Request::Command::Set && request.arguments.size() >= 2)
    {
      const auto &key = request.arguments.front();
      const auto &value = request.arguments[1];
      std::optional<std::chrono::milliseconds> expiry{};
      if (request.arguments.size() == 4 && request.arguments[2] == "px")
      {
        auto num = std::stoi(request.arguments[3]);
        expiry = std::chrono::milliseconds(num);
      }

      cache.set(key, value, expiry);
    }
  }

  void handle_client_connection(const int client_fd, Cache &cache)
  {
    // For a client, parse each incoming request, process the request, generate a response to the request, and send the response back to the client.
    // Do this in series, and keep repeating until the client closes the connection.
    while (true)
    {
      // TODO going over the RESP protocol:
      // https://redis.io/docs/latest/develop/reference/protocol-spec/
      // TODO only deal with simple request-response model for now.
      // TODO we don't support pipelining. So each client sends one request at a time, which results in one response.
      const auto request = RESP::parse_request_from_client(client_fd);
      if (!request)
      {
        std::cout << "Closing connection with " << client_fd << std::endl;
        break;
      }

      std::cout << "Parsed Command: " << RESP::Request::to_string(request->command) << std::endl;
      std::cout << "Parsed Arguments: ";
      for (const auto &arg : request->arguments)
      {
        std::cout << arg << " ";
      }
      std::cout << std::endl;

      std::cout << "Processing request..." << std::endl;
      process_request(*request, cache);

      const auto response = RESP::generate_response(*request, cache);
      std::cout << "Generated Response: " << response.data << std::endl;
      send_to_client(client_fd, RESP::response_to_string(response));
    }
  }

  // Just waits on the future and swallows exceptions (prints them out to cerr).
  void wait_for_async_task(std::future<void> &f)
  {
    try
    {
      f.get();
    }
    catch (const std::exception &future_error)
    {
      std::cerr << "Exception from async task: " << future_error.what() << std::endl;
    }
  }

  bool is_async_task_done(std::future<void> &f)
  {
    using namespace std::chrono_literals;
    if (f.wait_for(0s) == std::future_status::ready)
    {
      wait_for_async_task(f);
      return true;
    }

    return false;
  }

} // anonymous namespace

Server::Server()
{
  socket_fd_ = create_server_socket();
}

bool Server::is_ready() const
{
  return socket_fd_.has_value();
}

void Server::cleanup_finished_client_tasks()
{
  const auto new_end = std::remove_if(futures_.begin(), futures_.end(), [](auto &f)
                                      { return is_async_task_done(f); });
  if (new_end != futures_.end())
  {
    std::cout << "Cleanup resulted in erasing " << futures_.end() - new_end << " task(s)!" << std::endl;
    futures_.erase(new_end, futures_.end());
  }
}

void Server::run()
{
  using namespace std::chrono_literals;
  assert(is_ready());
  constexpr auto ASYNC_MAX_LIMIT = 100;
  constexpr auto CLEANUP_TASKS_DURATION = 1s;
  try
  {
    auto last_cleanup_time = std::chrono::system_clock::now();
    while (is_ready())
    {
      // The main server thread should clean up any stale tasks every now and then.
      if (std::chrono::system_clock::now() - last_cleanup_time > CLEANUP_TASKS_DURATION)
      {
        cleanup_finished_client_tasks();
        last_cleanup_time = std::chrono::system_clock::now();
      }

      // If we've ended up creating too many simultaneous connections, wait until the oldest connection closes.
      // This makes sure the server doesn't get too swamped with incoming client connections and makes them wait.
      if (futures_.size() >= ASYNC_MAX_LIMIT)
      {
        wait_for_async_task(futures_.front());
        futures_.pop_front();
      }

      // TODO the problem here is that because the main thread blocks on awaiting clients, we're not displaying any exceptions from the async tasks to console until we get another connection.
      // Create a new connection and spawn off an async task to handle this client.
      // NOTE: we give the task a reference to the cache since the tasks don't own the cache; this server process does instead. The server will be alive as long as any of the tasks.
      // NOTE: we pass off the cache using std::ref() because otherwise it would attempt to copy the cache (which is a move-only type due to the mutex).
      const int client_fd = await_client_connection(*socket_fd_);
      futures_.push_back(std::async(std::launch::async, handle_client_connection, client_fd, std::ref(cache_)));
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
  // This way, we ensure that the server process is alive as long as all its children tasks.
  while (futures_.size() > 0)
  {
    cleanup_finished_client_tasks();
  }

  if (socket_fd_)
  {
    close(*socket_fd_);
  }
}
