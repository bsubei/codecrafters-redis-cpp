#include <optional>
#include <unordered_map>
#include <future>
#include <deque>

#include "Cache.hpp"

class Server
{
private:
    std::optional<int> socket_fd_{};
    // These futures are handles to the asynchronous tasks, each handling a client connection.
    std::deque<std::future<void>> futures_{};

    Cache cache_{};

public:
    Server();
    ~Server();

    bool is_ready() const;

    void run();

    void cleanup_finished_client_tasks();
};
