#include <optional>

class Server
{
private:
    std::optional<int> socket_fd{};

public:
    Server();
    ~Server();

    bool is_ready() const;

    void run();
};
