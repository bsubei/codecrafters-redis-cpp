#pragma once

// System includes.
#include <algorithm>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// Our library's header includes.
#include "protocol.hpp"
#include "string_parser.hpp"
#include "utils.hpp"

class Config;
class Cache;

// Figure out what command is being sent to us in the request from the client.
// This function also makes sure the Message has the correct form (Array type if needed, and number of arguments).
std::optional<Command> parse_and_validate_command(const Message &message);
std::string message_to_string(const Message &message);

// TODO update this func to accept string literals so we don't have to create temporary strings just to create a Message.
template <StringLike StringType>
Message message_from_string(const StringType &s)
{
    // TODO if given string is missing terminators, we do undefined behavior

    // TODO handle case insensitivity

    // TODO redo all comments
    // Based on the data type, actually parse the message into a Request.
    const auto data_type = get_type(s);
    std::vector<std::string> tokens{};

    switch (data_type)
    {
    case DataType::BulkString:
    case DataType::SimpleString:
        tokens = parse_string(s, 1);
        return Message{tokens.front(), data_type};
    case DataType::Array:
    {
        // We start with an incoming message that looks like this: "*2\r\n$4\r\nECHO\r\n$2\r\nhi"
        auto it = s.cbegin() + 1;
        // Grab the number of elements since this is an Array message. The iter should end up pointing at the actual message.
        // TODO don't need this weird iterator mutating func
        const auto num_tokens = parse_num(it);
        // Now the remainder of our message looks like this: "$4\r\nECHO\r\n$2\r\nhi"
        // Parse each of the tokens (e.g. "ECHO" and "hi" in the above example).
        const auto tokens = tokenize_array(s, num_tokens);
        std::vector<Message> message_data{};
        message_data.reserve(tokens.size());
        std::transform(tokens.cbegin(), tokens.cend(), std::back_inserter(message_data), [](const auto &token)
                       { return message_from_string(token); });
        return Message(std::move(message_data), data_type);
    }
    // TODO might need to eventually handle NullBulkString if we expect clients to send us nils.
    default:
        std::cerr << "message_from_string unimplemented for data_type " << static_cast<int>(data_type) << ", was given string: " << s << std::endl;
        std::terminate();
    }
    std::unreachable();
}
inline Message message_from_string(const char *s)
{
    return message_from_string(std::string(s));
}

Message generate_response_message(const Command &command, const Config &config, Cache &cache);

std::string command_to_string(CommandVerb command);