#include "Server.h"

#include <iostream>

int main(int argc, char **argv)
{
    // Flush after every std::cout / std::cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    // You can use print statements as follows for debugging, they'll be visible when running tests.
    std::cout << "Logs from your program will appear here!\n";

    Server server{};
    if (!server.is_ready())
    {
        return 1;
    }

    // TODO think about how we can accept adn process multiple client connections using multiple threads.
    server.run();

    return 0;
}
