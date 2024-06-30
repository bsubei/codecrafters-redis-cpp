#include "Server.hpp"

#include <iostream>

int main(int argc, char **argv)
{
    Server server{};
    if (!server.is_ready())
    {
        return 1;
    }

    server.run();

    return 0;
}
