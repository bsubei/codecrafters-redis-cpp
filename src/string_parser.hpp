#pragma once

// System includes.
#include <string>
#include <vector>

// Our library's header includes.
#include "protocol.hpp"
#include "utils.hpp"

void move_up_to_terminator(auto &it)
{
    while (*it != '\r')
        ++it;
}
void move_past_terminator(auto &it)
{
    move_up_to_terminator(it);
    // Move past the '\r'
    ++it;
    // Move past the '\n'
    ++it;
}
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

// Given a string/string_view containing an RESP Array, return the length from the header part of the message.
// e.g. when given "*2\r\n$4\r\nECHO\r\n$2\r\nhi\r\n", this function return the int 2.
template <StringLike StringType>
int parse_array_length(const StringType &s)
{
    // Skip the first char (the "*").
    auto it = s.cbegin() + 1;
    // From https://redis.io/docs/latest/develop/reference/protocol-spec/#high-performance-parser-for-the-redis-protocol
    int len = 0;
    while (*it != '\r')
    {
        len = (len * 10) + (*it - '0');
        ++it;
    }
    return len;
}

// Given a string/string_view containing an RESP Array type, return the num_tokens tokens that make up the contents (ignoring the header).
// e.g. for an input of "*2\r\n$4\r\nECHO\r\n$2\r\nhi\r\n", we return:
// ["$4\r\nECHO\r\n", "$2\r\nhi\r\n"]
// TODO reuse parse_string to implement this
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
// Given a string/view containing an RESP non-Array, return its contents based on its exact type.
template <StringLike StringType>
std::string parse_string(const StringType &s, DataType data_type)
{
    // TODO probably better to do this with istringstream
    auto it = s.cbegin();

    // Given a message that looks like this: "$4\r\nECHO\r\n$2\r\nhi", parse the ECHO and hi parts. We ignore the length headers.
    if (data_type == DataType::BulkString)
    {
        // If the it points at a bulk string, just ignore the length header.
        move_past_terminator(it);

        // Now read the actual string.
        auto terminator_it = it;
        move_up_to_terminator(terminator_it);
        return std::string(&*it, terminator_it - it);
    }
    else if (data_type == DataType::SimpleString)
    {
        // Move past the '+' char.
        ++it;
        // Now read the actual string.
        auto terminator_it = it;
        move_up_to_terminator(terminator_it);
        return std::string(&*it, terminator_it - it);
    }

    std::cerr << "Unable to parse_string for given s: " << s << std::endl;
    std::terminate();
}
