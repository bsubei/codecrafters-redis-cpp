#include "Server.hpp"

#include <iostream>

int main(int argc, char **argv)
{
    Server server{};
    if (!server.is_ready())
    {
        return 1;
    }

    // TODO think about how we can accept and process multiple client connections using multiple threads.
    server.run();

    return 0;
}
