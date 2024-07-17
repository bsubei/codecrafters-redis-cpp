#pragma once

// System includes.
#include <exception>
#include <string>
#include <utility>
#include <vector>

// Our library's header includes.
#include "protocol.hpp"
#include "utils.hpp"

// Advances the iterator until it points to the start of the next terminator.
// Calling this if the iterator is already on the terminator does nothing.
void advance_to_next_terminator(auto &iter) {
  // Keep advancing until we see the '\r'.
  while (*iter != '\r')
    ++iter;
}
// Advances the iterator until it points to directly after the next
// terminator. Calling this if the iterator is already on the terminator simply
// advances the iterator two steps.
void advance_past_next_terminator(auto &iter) {
  // Keep advancing until we see the '\r'.
  advance_to_next_terminator(iter);
  // Advance past the '\r'.
  ++iter;
  // Advance past the '\n'.
  ++iter;
}
// Given a iterator to a string containing an RESP Array or BulkString, return
// the length from the header part of the message, and set the "iter" to be at
// the start of the message contents (skips the header). e.g. when given
// "*2\r\n$4\r\nECHO\r\n$2\r\nhi\r\n", this function returns the int 2 and sets
// the "iter" at the first "$".
int parse_length_header(auto &iter) {
  // Skip the first char (the "*" or "$").
  ++iter;
  // Reads the chars until the newline and interprets them as an int.
  // From
  // https://redis.io/docs/latest/develop/reference/protocol-spec/#high-performance-parser-for-the-redis-protocol
  int len = 0;
  while (*iter != '\r') {
    len = (len * 10) + (*iter - '0');
    ++iter;
  }
  // Make sure the iterator skips over the rest of the header ('\r' and '\n').
  ++iter;
  ++iter;
  return len;
}
inline DataType byte_to_data_type(char first_byte) {
  switch (first_byte) {
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
    std::cerr << "Unimplemented parsing for data type: " << first_byte
              << std::endl;
    std::terminate();
  }
}

template <StringLike StringType> DataType get_type(const StringType &str) {
  return str.size() > 0 ? byte_to_data_type(str.front()) : DataType::Unknown;
}

// Given a string/string_view containing an RESP Array type, return the
// num_tokens tokens that make up the contents (ignoring the header). e.g. for
// an input of "*2\r\n$4\r\nECHO\r\n$2\r\nhi\r\n", we return:
// ["$4\r\nECHO\r\n", "$2\r\nhi\r\n"]
// NOTE: the returned strings retain all the headers/terminators. i.e. the
// contents are not "parsed" (see parse_string for that).
template <StringLike StringType>
std::vector<std::string> tokenize_array(const StringType &str) {
  auto iter = str.cbegin();
  const auto num_tokens = parse_length_header(iter);
  // The iterator is now set at the start of the array contents.

  // Split the rest of the array contents into this many tokens.
  std::vector<std::string> tokens{};
  tokens.reserve(num_tokens);
  for (int i = 0; i < num_tokens; ++i) {
    auto terminator_it = iter;
    switch (byte_to_data_type(*iter)) {
    case DataType::SimpleString:
      // Grab everything up until and including the next terminator.
      advance_past_next_terminator(terminator_it);
      tokens.emplace_back(&*iter, terminator_it - iter);
      // Set the iterator to the start of the next element for the next
      // iteration.
      iter = terminator_it;
      break;
    case DataType::BulkString:
      // Grab everything up until and including the 2nd terminator (because bulk
      // strings have two terminators, e.g. "$2\r\nhi\r\n" and we want to grab
      // the whole thing).
      advance_past_next_terminator(terminator_it);
      advance_past_next_terminator(terminator_it);
      tokens.emplace_back(&*iter, terminator_it - iter);
      // Set the iterator to the start of the next element for the next
      // iteration.
      iter = terminator_it;
      break;
    // TODO we currently can't read NullBulkString because the first char looks
    // just like a regular BulkString.
    /*
    case DataType::NullBulkString:
        // A Null BulkString looks like: "$-1\r\n"
        // There is no actual string content we care about.
        tokens.emplace_back("");
        // Set the iterator to the start of the next element for the next
    iteration. advance_past_next_terminator(iter); break;
    */
    case DataType::Unknown:
    case DataType::SimpleError:
    case DataType::Integer:
    case DataType::NullBulkString:
    case DataType::Array:
    case DataType::Null:
    case DataType::Boolean:
    case DataType::Double:
    case DataType::BigNumber:
    case DataType::BulkError:
    case DataType::VerbatimString:
    case DataType::Map:
    case DataType::Set:
    case DataType::Push:
    default:
      std::cerr << "Unable to tokenize array for given str: " << str
                << std::endl;
      std::terminate();
    }
  }

  return tokens;
}

// Given a string/view containing an RESP non-Array, return its contents based
// on its exact type. NOTE: the returned string has all the terminators and
// headers stripped out.
template <StringLike StringType>
std::string parse_string(const StringType &str, DataType data_type) {
  auto iter = str.cbegin();
  // Given a message that looks like this: "$4\r\nECHO\r\n", parse and return
  // the "ECHO" part.
  switch (data_type) {
  case DataType::BulkString: {
    // Read the number of chars from the length header and set the iterator to
    // the start of the string.
    const auto num_chars = parse_length_header(iter);
    // Now read the string.
    return std::string(&*iter, num_chars);
  }

  case DataType::SimpleString: {
    // Ignore the '+' char.
    ++iter;
    // TODO test if empty simple strings are allowed.
    // Now read the actual string (from here up to but not including the next
    // terminator).
    auto terminator_it = iter;
    advance_to_next_terminator(terminator_it);
    return std::string(&*iter, terminator_it - iter);
  }
  case DataType::Unknown:
  case DataType::SimpleError:
  case DataType::Integer:
  case DataType::NullBulkString:
  case DataType::Null:
  case DataType::Boolean:
  case DataType::Double:
  case DataType::BigNumber:
  case DataType::BulkError:
  case DataType::VerbatimString:
  case DataType::Map:
  case DataType::Set:
  case DataType::Push:
  default:
    std::cerr << "Unable to parse_string for given str: " << str << std::endl;
    std::terminate();
  }
  std::unreachable();
}
