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
// Given a string or string_view, parse it into a Message and return that.
template <StringLike StringType>
Message message_from_string(const StringType &s)
{
    // TODO if given string is missing terminators, we get undefined behavior

    const auto data_type = get_type(s);

    switch (data_type)
    {
    // Non-array types of Messages should be parsed as a single string.
    case DataType::BulkString:
    case DataType::SimpleString:
        return Message(parse_string(s, data_type), data_type);
    // Array type of Message is nested (assumes only one level of nesting) and contains a vector of Messages, which must
    // themselves be parsed. Note that we recurse over each of these nested Messages.
    case DataType::Array:
    {
        // We start with an incoming message that looks like this: "*2\r\n$4\r\nECHO\r\n$2\r\nhi\r\n"
        // Grab the number of tokens i.e. the "2" in "*2".
        const auto num_tokens = parse_array_length(s);
        // Now the remainder of our message looks like this: "$4\r\nECHO\r\n$2\r\nhi\r\n".
        // Split the message into that many tokens (e.g. "$4\r\nECHO\r\n" and "$2\r\nhi\r\n").
        const auto tokens = tokenize_array(s, num_tokens);
        // Recurse over each of these tokens, expecting them to come back as two parsed messages of non-Array data_type (e.g. BulkString).
        std::vector<Message> message_data{};
        message_data.reserve(tokens.size());
        std::transform(tokens.cbegin(), tokens.cend(), std::back_inserter(message_data), [](const auto &token)
                       { return message_from_string(token); });
        // Create a Message that contains that vector of Messages.
        return Message(std::move(message_data), data_type);
    }
    // TODO might need to eventually handle NullBulkString if we expect clients to send us nils.
    default:
        std::cerr << "message_from_string unimplemented for data_type " << static_cast<int>(data_type) << ", was given string: " << s << std::endl;
        std::terminate();
    }
    std::unreachable();
}
// TODO hacky way to get string literals (e.g. in tests) to work by simply copying them into a string. Needs improved.
inline Message message_from_string(const char *s)
{
    return message_from_string(std::string(s));
}

Message generate_response_message(const Command &command, const Config &config, Cache &cache);

std::string command_to_string(CommandVerb command);