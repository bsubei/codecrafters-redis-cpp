#include <optional>
#include <future>
#include <deque>

class Server
{
private:
    std::optional<int> socket_fd{};
    // These futures are handles to the asynchronous tasks, each handling a client connection.
    std::deque<std::future<void>> futures{};

public:
    Server();
    ~Server();

    bool is_ready() const;

    void run();

    void cleanup_finished_client_tasks();
};
