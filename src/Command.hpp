#include <string>
#include <optional>

enum class Command
{
    Ping,
};

std::optional<Command> parse_command(const std::string &command)
{
    if (command == "PING")
    {
        return Command::Ping;
    }
    else
    {
        return std::nullopt;
    }
}