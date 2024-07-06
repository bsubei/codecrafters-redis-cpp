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
// Given an iterator starting at a number, parse that number and move the iterator just past the CRLF newline terminator.
int parse_num(auto &it)
{
    // From https://redis.io/docs/latest/develop/reference/protocol-spec/#high-performance-parser-for-the-redis-protocol
    int len = 0;
    while (*it != '\r')
    {
        len = (len * 10) + (*it - '0');
        ++it;
    }
    move_past_terminator(it);
    return len;
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
