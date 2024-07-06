#pragma once

// System includes.
#include <deque>
#include <future>
#include <unordered_map>
#include <optional>

// Our library's header includes.
#include "cache.hpp"
#include "config.hpp"

class Server
{
private:
    std::optional<int> socket_fd_{};
    // These futures are handles to the asynchronous tasks, each handling a client connection.
    std::deque<std::future<void>> futures_{};

    Cache cache_{};

    Config config_{};

public:
    Server(Config config);
    ~Server();

    bool is_ready() const;

    void run();

    void cleanup_finished_client_tasks();
};
