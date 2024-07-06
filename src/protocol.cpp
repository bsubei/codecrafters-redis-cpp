// This source file's own header include.
#include "protocol.hpp"

// TODO make these free functions
std::string Command::to_string(CommandVerb command)
{
    switch (command)
    {
    case CommandVerb::Ping:
        return "ping";
    case CommandVerb::Echo:
        return "echo";
    case CommandVerb::Set:
        return "set";
    case CommandVerb::Get:
        return "get";
    case CommandVerb::ConfigGet:
        return "config get";
    default:
        std::cerr << "Unknown CommandVerb enum encountered: " << static_cast<int>(command) << std::endl;
        std::terminate();
    }
}