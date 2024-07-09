#pragma once

// System includes.
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

// Our library's header includes.
#include "utils.hpp"

// This file contains all the types required to work with the Redis
// serialization protocol specification (RESP). Only RESP 2.0 is supported, 3.0
// is explicitly not supported.

// This is the terminator for the RESP protocol that separates its parts.
static constexpr auto TERMINATOR = "\r\n";

// See the table of data types in
// https://redis.io/docs/latest/develop/reference/protocol-spec/#resp-protocol-description
enum class DataType : std::uint8_t {
  Unknown = 0,
  // Comes in this format: +<data>\r\n
  SimpleString,
  SimpleError,
  Integer,
  // Also known as binary string.
  // Comes in this format: $<length>\r\n<data>\r\n
  BulkString,
  // TODO we currently don't handle reading NullBulkString correctly, only
  // writing it out.
  NullBulkString,
  // Comes in this format: *<num_elems>\r\n<elem_1>\r\n....<elem_n>\r\n
  Array,
  // The rest of these we don't implement for now.
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
// These are the kinds of commands sent from the client that the server is able
// to parse and respond to.
enum class CommandVerb : std::uint8_t {
  Unknown = 0,
  Ping,
  Echo,
  Set,
  Get,
  ConfigGet,
};

// A Message sent from the client to the server is parsed into a Command.
// This Command is then used by the server to decide what action(s) to take and
// how to respond to the client.
struct Command {
  CommandVerb verb{};
  std::vector<std::string> arguments{};
};

// helper type to create visitors for the Message data variant.
template <class... Ts> struct MessageDataVisitor : Ts... {
  using Ts::operator()...;
};

// A Message contains "data" to be interpreted based on the stored "data_type".
// A Message can be constructed from a string received from a client (e.g.
// "+PING\r\n"), or is generated as a response to the client. When a Message has
// data_type of Array, its "data" member is of the vector<Message> variant. That
// means that a Message can contain a vector of other Messages, but only at one
// level (i.e. the Messages in this vector cannot themselves be of data_type
// Array and have a vector<Message> inside of them). When a Message has any
// other data_type, its "data" is interpreted as a std::string according to the
// format corresponding to data_type.
struct Message {
  using StringVariantT = std::string;
  using NestedVariantT = std::vector<Message>;
  std::variant<StringVariantT, NestedVariantT> data{};
  DataType data_type{};

  // Empty ctor
  Message() = default;

  // Forwarding ctor.
  template <typename T>
  Message(T &&data_in, DataType data_type_in)
      : data(std::forward<T>(data_in)), data_type(data_type_in) {
    // TODO this check might make things too slow. Profile this at some point.
    // We check that the data_type and the data variant always match. i.e. if
    // data_type is Array, then the data is of the nested (vector) variant.
    // Otherwise, the data is of the non-nested string variant.
    if (data_type == DataType::Array) {
      if constexpr (!std::is_convertible_v<std::decay_t<T>, NestedVariantT>) {
        assert(false &&
               "Array messages must be initialized with a vector<Message>");
      }
    } else {
      if constexpr (!std::is_convertible_v<std::decay_t<T>, StringVariantT>) {
        assert(false && "Non-array messages must be initialized with a string");
      }
    }
  }

  bool operator==(const Message &other) const = default;

  // TODO this is causing issues in MSAN
  // For displaying in GTEST.
  // friend std::ostream &operator<<(std::ostream &os, const Message &message);
};
