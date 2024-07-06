// This source file's own header include.
#include "network.hpp"

// System includes.
#include <arpa/inet.h>
#include <sys/socket.h>
#include <iostream>

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