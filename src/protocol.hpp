#pragma once

// System includes.
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

// Our library's header includes.
#include "utils.hpp"

// This file contains all the types required to work with the Redis serialization protocol specification (RESP).
// Only RESP 2.0 is supported, 3.0 is explicitly not supported.

// This is the terminator for the RESP protocol that separates its parts.
static constexpr auto TERMINATOR = "\r\n";

// See the table of data types in https://redis.io/docs/latest/develop/reference/protocol-spec/#resp-protocol-description
enum class DataType
{
    Unknown,
    // Comes in this format: +<data>\r\n
    SimpleString,
    SimpleError,
    Integer,
    // Also known as binary string.
    // Comes in this format: $<length>\r\n<data>\r\n
    BulkString,
    // TODO we currently don't handle reading NullBulkString correctly, only writing it out.
    NullBulkString,
    // Comes in this format: *<num_elems>\r\n<elem_1>\r\n....<elem_n>\r\n
    Array,
    // The rest of these are RESP3, which we don't implement
    Null,
    Boolean,
    Double,
    BigNumber,
    BulkError,
    VerbatimString,
    Map,
    Set,
    Push,
};

// TODO consider using wise_enum to make parsing these commands easier.
// These are the kinds of commands sent from the client that the server is able to parse and respond to.
enum class CommandVerb : std::uint8_t
{
    Unknown = 0,
    Ping = 1,
    Echo = 2,
    Set = 3,
    Get = 4,
    ConfigGet = 5,
};

// A Message sent from the client to the server is parsed into a Command.
// This Command is then used by the server to decide what action(s) to take and how to respond to the client.
struct Command
{
    CommandVerb verb{};
    std::vector<std::string> arguments{};
};
