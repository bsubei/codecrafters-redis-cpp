#pragma once

// System includes.
#include <string>
#include <vector>

// Our library's header includes.
#include "protocol.hpp"
#include "Utils.hpp"
// TODO detangle Parser.hpp
// #include "Parser.hpp"

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

// TODO get rid of this namespace
namespace RESP
{

}