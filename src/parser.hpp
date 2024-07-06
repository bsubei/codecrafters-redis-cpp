#pragma once

// TODO clean up these headers, way too many
// System includes.
#include <array>
#include <algorithm>
#include <string>
#include <vector>
#include <optional>
#include <cstring>
#include <unistd.h>
#include <cassert>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <exception>
#include <sstream>
#include <iostream>
#include <utility>

// Our library's header includes.
#include "protocol.hpp"
#include "string_parser.hpp"
#include "utils.hpp"

class Config;
class Cache;

inline DataType byte_to_data_type(char first_byte)
{
    switch (first_byte)
    {
    case '+':
        return DataType::SimpleString;
    case '-':
        return DataType::SimpleError;
    case ':':
        return DataType::Integer;
    case '$':
        return DataType::BulkString;
    case '*':
        return DataType::Array;
    default:
        std::cerr << "Unimplemented parsing for data type: " << first_byte << std::endl;
        std::terminate();
    }
}

template <StringLike StringType>
DataType get_type(const StringType &s)
{
    return s.size() > 0 ? byte_to_data_type(s.front()) : DataType::Unknown;
}

// Given a string of the Array contents (of size n), break it up into n tokens. We can't simply split by the terminator because some elements are complex (bulk string).
// Each returned token is the unparsed string. e.g. for an input of "*2\r\n$4\r\nECHO\r\n$2\r\nhi", we return:
// ["$4\r\nECHO\r\n", "$2\r\nhi"]
template <StringLike StringType>
std::vector<std::string> tokenize_array(const StringType &s, const int num_tokens)
{
    std::vector<std::string> tokens{};
    tokens.reserve(num_tokens);
    auto it = s.cbegin();
    // Skip past the array length part, jump right into the actual array contents.
    move_past_terminator(it);
    for (int i = 0; i < num_tokens; ++i)
    {
        auto terminator_it = it;
        switch (byte_to_data_type(*it))
        {
        case DataType::SimpleString:
            move_past_terminator(terminator_it);
            tokens.emplace_back(&*it, terminator_it - it);
            it = terminator_it;
            break;
        case DataType::BulkString:
            move_past_terminator(terminator_it);
            move_past_terminator(terminator_it);
            tokens.emplace_back(&*it, terminator_it - it);
            it = terminator_it;
            break;
        default:
            std::cerr << "Unable to tokenize array for given s: " << s << std::endl;
            std::terminate();
        }
    }

    return tokens;
}

// TODO eventually use the length in the header to efficiently read and also correctly handle text with newlines in it.
// TODO make this more efficient and avoid creating strings and passing them around
template <StringLike StringType>
std::vector<std::string> parse_string(const StringType &s, const int num_tokens)
{
    // TODO probably better to do this with istringstream
    std::vector<std::string> tokens{};
    auto it = s.cbegin();
    DataType data_type = byte_to_data_type(*it);

    // Given a message that looks like this: "$4\r\nECHO\r\n$2\r\nhi", parse the ECHO and hi parts. We ignore the length headers.
    for (int i = 0; i < num_tokens; ++i)
    {
        if (data_type == DataType::BulkString)
        {
            // If the it points at a bulk string, just ignore the length header.
            move_past_terminator(it);

            // Now read the actual string.
            auto terminator_it = it;
            move_up_to_terminator(terminator_it);
            tokens.emplace_back(&*it, &*terminator_it);
            // Move the iterator to the next token.
            move_past_terminator(it);
        }
        else if (data_type == DataType::SimpleString)
        {
            // Move past the '+' char.
            ++it;
            // Now read the actual string.
            auto terminator_it = it;
            move_up_to_terminator(terminator_it);
            tokens.emplace_back(&*it, &*terminator_it);
            // TODO I think there's a bug here where it is not updated to move past terminator_it(??)
        }
    }

    return tokens;
}

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

template <typename T>
Message make_message(T &&data, DataType data_type)
{
    return Message(std::forward<T>(data), data_type);
}

Message generate_response_message(const Command &command, const Config &config, Cache& cache);

std::string command_to_string(CommandVerb command);