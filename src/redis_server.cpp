// This source file's own header include.
#include "redis_server.hpp"

// Our library's header includes.
#include "network.hpp"
#include "redis_core.hpp"
#include "storage.hpp"

// System includes.
#include <cassert>
#include <chrono>
#include <iostream>
#include <unistd.h>

namespace {

void handle_command(const Command &command, Cache &cache) {
  // Handle any state changes we need to do before replying to the client.
  // The SET command has the side-effect of updating the given key-value pairs
  // in our cache/db.
  if (command.verb == CommandVerb::Set) {
    const auto &key = command.arguments.front();
    const auto &value = command.arguments[1];
    std::optional<std::chrono::milliseconds> expiry{};
    if (command.arguments.size() == 4 && command.arguments[2] == "px") {
      auto num = std::stoi(command.arguments[3]);
      expiry = std::chrono::milliseconds(num);
    }

    cache.set(key, value, expiry);
  }
}

void handle_client_connection(const int client_fd, const Config &config,
                              Cache &cache) {
  // For a client, parse each incoming request, process the request, generate a
  // response to the request, and send the response back to the client. Do this
  // in series, and keep repeating until the client closes the connection.
  while (true) {
    // RESP protocol:
    // https://redis.io/docs/latest/develop/reference/protocol-spec/
    // We only deal with simple request-response model for now.
    // TODO we don't support pipelining. So each client sends one request at a
    // time, which results in one response.
    const auto request = receive_string_from_client(client_fd);
    if (!request) {
      std::cout << "Closing connection with " << client_fd << std::endl;
      break;
    }
    std::cout << "Parsing request from client " << client_fd << ": " << *request
              << std::endl;

    const auto request_message = message_from_string(*request);
    std::cout << "Interpreting request as message: "
              << message_to_string(request_message) << std::endl;

    const auto command = parse_and_validate_command(request_message);
    Message response_message{};
    if (!command) {
      // Print out an error but reply with "OK".
      std::cerr << "Could not parse command from given request: "
                << message_to_string(request_message) << std::endl;
      response_message = Message{"OK", DataType::SimpleString};
    } else {
      handle_command(*command, cache);
      response_message = generate_response_message(*command, config, cache);
    }

    const auto response = message_to_string(response_message);
    std::cout << "Sending Response: " << response << std::endl;
    send_to_client(client_fd, response);
  }
}

// Just waits on the future and swallows exceptions (prints them out to cerr).
void wait_for_async_task(std::future<void> &f) {
  try {
    f.get();
  } catch (const std::exception &future_error) {
    std::cerr << "Exception from async task: " << future_error.what()
              << std::endl;
  }
}

bool is_async_task_done(std::future<void> &f) {
  using namespace std::chrono_literals;
  if (f.wait_for(0s) == std::future_status::ready) {
    wait_for_async_task(f);
    return true;
  }

  return false;
}

} // anonymous namespace

Server::Server(Config config)
    : socket_fd_(create_server_socket()), futures_(),
      cache_(read_cache_from_rdb(config)), config_(std::move(config)) {}

bool Server::is_ready() const { return socket_fd_.has_value(); }

void Server::cleanup_finished_client_tasks() {
  const auto new_end =
      std::remove_if(futures_.begin(), futures_.end(),
                     [](auto &f) { return is_async_task_done(f); });
  if (new_end != futures_.end()) {
    std::cout << "Cleanup resulted in erasing " << futures_.end() - new_end
              << " task(s)!" << std::endl;
    futures_.erase(new_end, futures_.end());
  }
}

void Server::run() {
  using namespace std::chrono_literals;
  assert(is_ready());
  constexpr auto ASYNC_MAX_LIMIT = 100;
  constexpr auto CLEANUP_TASKS_DURATION = 1s;
  try {
    auto last_cleanup_time = std::chrono::system_clock::now();
    while (is_ready()) {
      // The main server thread should clean up any stale tasks every now and
      // then. This should be pretty fast as it does not block on unfinished
      // tasks, just leaves them be.
      if (std::chrono::system_clock::now() - last_cleanup_time >
          CLEANUP_TASKS_DURATION) {
        cleanup_finished_client_tasks();
        last_cleanup_time = std::chrono::system_clock::now();
      }

      // If we've ended up creating too many simultaneous connections, wait
      // until the oldest connection closes. This makes sure the server doesn't
      // get too swamped with incoming client connections and makes them wait.
      // NOTE: this means the server intentionally stops responding to new
      // client connections until some clients finish. Ideally, we should only
      // wait a little bit for the front task and move on to the next ones in
      // the hope of finding a finished one.
      if (futures_.size() >= ASYNC_MAX_LIMIT) {
        wait_for_async_task(futures_.front());
        futures_.pop_front();
      }

      // TODO the problem here is that because the main thread blocks on
      // awaiting clients, we're not displaying any exceptions from the async
      // tasks to console until we get another connection.

      // Create a new connection and spawn off an async task to handle this
      // client.
      // NOTE: we give the task a reference to the cache since the tasks
      // don't own the cache; this server process does instead. The server will
      // be alive as long as any of the tasks.
      // NOTE: we pass off the cache using std::ref() because otherwise it would
      // attempt to copy the cache (which is a move-only type due to the mutex).
      // NOTE: passing the config_ is thread-safe because we never modify it,
      // just read from it.
      const int client_fd = await_client_connection(*socket_fd_);
      futures_.push_back(std::async(std::launch::async,
                                    handle_client_connection, client_fd,
                                    std::cref(config_), std::ref(cache_)));
    }
  } catch (const std::exception &server_error) {
    std::cerr << "Exception thrown while server was handling new incoming "
                 "client connections: "
              << server_error.what() << std::endl;
  }
}

Server::~Server() {
  // If the server is shutting down, wait for all the client connection tasks to
  // finish up. This way, we ensure that the server process is alive as long as
  // all its children tasks.
  while (futures_.size() > 0) {
    cleanup_finished_client_tasks();
  }

  if (socket_fd_) {
    close(*socket_fd_);
  }
}
