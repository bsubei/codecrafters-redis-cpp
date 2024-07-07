// This source file's own header include.
#include "redis_core.hpp"

// System includes.
#include <algorithm>
#include <iostream>
#include <string_view>
#include <variant>

// Our library's header includes.
#include "cache.hpp"
#include "config.hpp"
#include "utils.hpp"

namespace {
// NOTE: we return a reference to one of the strings inside the given message
// arguments. This is fine as long as the caller doesn't hold on to these
// references longer than they do the Message they passed in as an argument.
const std::string &get_first_elem(const Message &message) {
  return std::visit(
      MessageDataVisitor{
          [](const Message::NestedVariantT &message_data)
              -> const std::string & {
            const auto &first_message = message_data.front();
            assert(std::holds_alternative<Message::StringVariantT>(
                       first_message.data) &&
                   "Nested Array messages are not allowed!");
            return std::get<Message::StringVariantT>(first_message.data);
          },
          [](const Message::StringVariantT &message_data)
              -> const std::string & { return message_data; },
          [](const auto &&message_data) -> const std::string & {
            std::cerr << "Unable to call get_first_elem on message with data: "
                      << message_data << std::endl;
            std::terminate();
          }},
      message.data);
}

bool is_array(const Message &message) {
  return std::holds_alternative<Message::NestedVariantT>(message.data);
}

} // anonymous namespace
// TODO we can probably get away with moving the message when calling this func
// since it's not used afterwards.
std::optional<Command> parse_and_validate_command(const Message &message) {
  const auto first_elem = tolower(get_first_elem(message));
  // Two args in the message means the command has one argument.
  const bool is_array_and_has_two_elements =
      is_array(message) &&
      std::get<Message::NestedVariantT>(message.data).size() == 2;
  if (first_elem == "ping") {
    // PING can be either a simple string on its own or an Array if an argument
    // is provided.
    if (is_array_and_has_two_elements) {
      const auto &arg = std::get<Message::NestedVariantT>(message.data)[1];
      assert(arg.data_type != DataType::Array &&
             "Nested Array messages are not allowed!");
      return Command{CommandVerb::Ping,
                     {std::get<Message::StringVariantT>(arg.data)}};
    }
    return Command{CommandVerb::Ping, {}};
  } else if (first_elem == "echo" && is_array_and_has_two_elements) {
    // ECHO requires exactly one argument and therefore must be an Array
    // message.
    if (is_array_and_has_two_elements) {
      const auto &arg = std::get<Message::NestedVariantT>(message.data)[1];
      assert(arg.data_type != DataType::Array &&
             "Nested Array messages are not allowed!");
      return Command{CommandVerb::Echo,
                     {std::get<Message::StringVariantT>(arg.data)}};
    }
  } else if (first_elem == "get" && is_array_and_has_two_elements) {
    // GET requires exactly one argument and therefore must be an Array message.
    if (is_array_and_has_two_elements) {
      const auto &arg = std::get<Message::NestedVariantT>(message.data)[1];
      assert(arg.data_type != DataType::Array &&
             "Nested Array messages are not allowed!");
      return Command{CommandVerb::Get,
                     {std::get<Message::StringVariantT>(arg.data)}};
    }
  } else if (first_elem == "set") {
    // SET must have at least two arguments (the key and value to set).
    if (is_array(message)) {
      const auto &messages = std::get<Message::NestedVariantT>(message.data);
      for ([[maybe_unused]] const auto &m : messages) {
        assert(m.data_type != DataType::Array &&
               "Nested Array messages are not allowed!");
      }
      // The first element is the command, so three elements means we have two
      // arguments to the command.
      const bool has_at_least_three_elements = messages.size() >= 3;
      if (has_at_least_three_elements) {
        std::vector<std::string> args{};
        // Copy the messages (skipping the first) as strings and make them the
        // command arguments.
        std::transform(messages.cbegin() + 1, messages.cend(),
                       std::back_inserter(args), [](const auto &m) {
                         return std::get<Message::StringVariantT>(m.data);
                       });
        return Command{CommandVerb::Set, std::move(args)};
      }
    }
  } else if (first_elem == "config") {
    // CONFIG GET must provide at least one argument.
    if (is_array(message)) {
      const auto &messages = std::get<Message::NestedVariantT>(message.data);
      for ([[maybe_unused]] const auto &m : messages) {
        assert(m.data_type != DataType::Array &&
               "Nested Array messages are not allowed!");
      }
      // The first two elements make up the command, so three elements means we
      // have one argument to the command.
      const bool has_at_least_three_elements =
          std::get<Message::NestedVariantT>(message.data).size() >= 3;
      if (has_at_least_three_elements &&
          tolower(std::get<Message::StringVariantT>(messages[1].data)) ==
              "get") {
        std::vector<std::string> args{};
        // Copy the messages (skipping the first two) as strings and make them
        // the command arguments.
        std::transform(messages.cbegin() + 2, messages.cend(),
                       std::back_inserter(args), [](const auto &m) {
                         return std::get<Message::StringVariantT>(m.data);
                       });
        return Command{CommandVerb::ConfigGet, std::move(args)};
      }
    }
  }

  return std::nullopt;
}
std::string message_to_string(const Message &message) {
  std::stringstream ss{};
  std::visit(
      MessageDataVisitor{
          [&ss](const Message::NestedVariantT &message_data) {
            ss << "*" << message_data.size() << TERMINATOR;
            for (const auto &elem : message_data) {
              ss << message_to_string(elem);
            }
          },
          [&ss, data_type = message.data_type](
              const Message::StringVariantT &message_data) {
            switch (data_type) {
            case DataType::SimpleString:
              ss << "+" << message_data << TERMINATOR;
              break;
            case DataType::BulkString:
              ss << "$" << message_data.size() << TERMINATOR << message_data
                 << TERMINATOR;
              break;
            case DataType::NullBulkString:
              ss << "$-1" << TERMINATOR;
              break;
            default:
              std::cerr << "Trying to stringify a Message with Unknown DataType"
                        << std::endl;
              std::terminate();
            }
          },
      },
      message.data);
  return ss.str();
}

Message generate_response_message(const Command &command, const Config &config,
                                  Cache &cache) {
  if (command.verb == CommandVerb::Ping) {
    // If PING had an argument, reply with just that argument like ECHO would.
    if (command.arguments.size() == 1) {
      const auto data_type = DataType::BulkString;
      return Message(command.arguments.front(), data_type);
    }
    // Otherwise, reply with the simple string "PONG".
    return Message("PONG", DataType::SimpleString);
  } else if (command.verb == CommandVerb::Echo) {
    return Message(command.arguments.front(), DataType::BulkString);
  } else if (command.verb == CommandVerb::Get) {
    // TODO we don't currently handle "*" globs or multiple keys.
    // TODO we assume GET always comes with one and only one argument.
    const auto &key = command.arguments.front();
    const auto value = cache.get(key);
    if (value) {
      return Message(*value, DataType::BulkString);
    }
    return Message("", DataType::NullBulkString);
  } else if (command.verb == CommandVerb::ConfigGet) {
    // TODO we don't currently handle "*" globs or multiple keys.
    const auto &key = command.arguments.front();
    std::optional<std::string> value{};
    if (tolower(key) == "dir") {
      value = config.dir;
    } else if (tolower(key) == "dbfilename") {
      value = config.dbfilename;
    }

    // Reply with array listing the key and value if found.
    if (value.has_value()) {
      return Message(
          Message::NestedVariantT{Message(key, DataType::BulkString),
                                  Message(*value, DataType::BulkString)},
          DataType::Array);
    }
    // Otherwise, respond with empty array.
    return Message(Message::NestedVariantT{}, DataType::Array);
  }

  // Print out an error but reply with "OK".
  std::cerr << "Could not generate a valid response for the given command: "
            << command_to_string(command.verb) << ", with args (size "
            << command.arguments.size() << "): ";
  for (const auto &arg : command.arguments) {
    std::cerr << arg;
  }
  std::cerr << std::endl;
  return Message{"OK", DataType::SimpleString};
}

std::string command_to_string(CommandVerb command) {
  switch (command) {
  case CommandVerb::Ping:
    return "ping";
  case CommandVerb::Echo:
    return "echo";
  case CommandVerb::Set:
    return "set";
  case CommandVerb::Get:
    return "get";
  case CommandVerb::ConfigGet:
    return "config get";
  default:
    std::cerr << "Unknown CommandVerb enum encountered: "
              << static_cast<int>(command) << std::endl;
    std::terminate();
  }
}