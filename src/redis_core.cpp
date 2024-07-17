// This source file's own header include.
#include "redis_core.hpp"

// System includes.
#include <algorithm>
#include <iostream>
#include <variant>

// Our library's header includes.
#include "cache.hpp"
#include "config.hpp"
#include "protocol.hpp"

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
                       first_message.get_data()) &&
                   "Nested Array messages are not allowed!");
            return std::get<Message::StringVariantT>(first_message.get_data());
          },
          [](const Message::StringVariantT &message_data)
              -> const std::string & { return message_data; },
          [](const auto &&message_data) -> const std::string & {
            std::cerr << "Unable to call get_first_elem on message with data: "
                      << message_data << std::endl;
            std::terminate();
          }},
      message.get_data());
}

bool is_array(const Message &message) {
  return std::holds_alternative<Message::NestedVariantT>(message.get_data());
}

Command parse_ping_command(const Message &message) {
  // PING can be either a simple string on its own or an Array if an argument
  // is provided.
  const auto &arg = std::get<Message::NestedVariantT>(message.get_data())[1];
  assert(arg.get_data_type() != DataType::Array &&
         "Nested Array messages are not allowed!");
  return Command{CommandVerb::Ping,
                 {std::get<Message::StringVariantT>(arg.get_data())}};
  return Command{CommandVerb::Ping, {}};
}

Command parse_echo_command(const Message &message) {
  // ECHO requires exactly one argument and therefore must be an Array
  // message.
  const auto &arg = std::get<Message::NestedVariantT>(message.get_data())[1];
  assert(arg.get_data_type() != DataType::Array &&
         "Nested Array messages are not allowed!");
  return Command{CommandVerb::Echo,
                 {std::get<Message::StringVariantT>(arg.get_data())}};
}
Command parse_get_command(const Message &message) {
  // GET requires exactly one argument and therefore must be an Array message.
  const auto &arg = std::get<Message::NestedVariantT>(message.get_data())[1];
  assert(arg.get_data_type() != DataType::Array &&
         "Nested Array messages are not allowed!");
  return Command{CommandVerb::Get,
                 {std::get<Message::StringVariantT>(arg.get_data())}};
}

Command parse_set_command(const Message &message) {
  // SET must have at least two arguments (the key and value to set).
  // The first element is the command, so three elements means we have two
  // arguments to the command.
  const auto &messages = std::get<Message::NestedVariantT>(message.get_data());
  for ([[maybe_unused]] const auto &msg : messages) {
    assert(msg.get_data_type() != DataType::Array &&
           "Nested Array messages are not allowed!");
  }
  std::vector<std::string> args{};
  // Copy the messages (skipping the first) as strings and make them the
  // command arguments.
  std::transform(messages.cbegin() + 1, messages.cend(),
                 std::back_inserter(args), [](const auto &msg) {
                   return std::get<Message::StringVariantT>(msg.get_data());
                 });
  return Command{CommandVerb::Set, std::move(args)};
}

std::optional<Command> parse_config_command_maybe(const Message &message) {
  // TODO this doesn't handle CONFIG GET with a single arg, it should.
  // CONFIG GET must provide at least one argument.
  // The first two elements make up the command, so three elements means we
  // have one argument to the command.
  const auto &messages = std::get<Message::NestedVariantT>(message.get_data());
  for ([[maybe_unused]] const auto &msg : messages) {
    assert(msg.get_data_type() != DataType::Array &&
           "Nested Array messages are not allowed!");
  }
  if (tolower(std::get<Message::StringVariantT>(messages[1].get_data())) ==
      "get") {
    std::vector<std::string> args{};
    // Copy the messages (skipping the first two) as strings and make them
    // the command arguments.
    std::transform(messages.cbegin() + 2, messages.cend(),
                   std::back_inserter(args), [](const auto &msg) {
                     return std::get<Message::StringVariantT>(msg.get_data());
                   });
    return Command{CommandVerb::ConfigGet, std::move(args)};
  }
  return std::nullopt;
}

Command parse_keys_command([[maybe_unused]] const Message &message) {
  // TODO actually parse the KEYS arguments (assume "*" for now).
  return Command{CommandVerb::Keys, {}};
}

std::optional<Command> parse_array_command(const Message &message) {
  const auto first_elem = tolower(get_first_elem(message));
  // Two args in the message means the command has one argument.
  const bool is_array_and_has_two_elements =
      is_array(message) &&
      std::get<Message::NestedVariantT>(message.get_data()).size() == 2;
  const bool is_array_and_has_at_least_three_elements =
      is_array(message) &&
      std::get<Message::NestedVariantT>(message.get_data()).size() >= 3;
  if (first_elem == "ping" && is_array_and_has_two_elements) {
    return parse_ping_command(message);
  }
  if (first_elem == "echo" && is_array_and_has_two_elements) {
    return parse_echo_command(message);
  }
  if (first_elem == "get" && is_array_and_has_two_elements) {
    return parse_get_command(message);
  }
  if (first_elem == "set" && is_array_and_has_at_least_three_elements) {
    return parse_set_command(message);
  }
  if (first_elem == "config" && is_array_and_has_at_least_three_elements) {
    return parse_config_command_maybe(message);
  }
  if (first_elem == "keys") {
    return parse_keys_command(message);
  }

  return std::nullopt;
}

} // anonymous namespace
// TODO we can probably get away with moving the message when calling this func
// since it's not used afterwards.
std::optional<Command> parse_and_validate_command(const Message &message) {
  if (is_array(message)) {
    return parse_array_command(message);
  }
  // Currently we don't handle any commands that are not Array Messages.
  return std::nullopt;
}
std::string message_to_string(const Message &message) {
  std::stringstream sstr{};
  std::visit(
      MessageDataVisitor{
          [&sstr](const Message::NestedVariantT &message_data) {
            sstr << "*" << message_data.size() << TERMINATOR;
            for (const auto &elem : message_data) {
              sstr << message_to_string(elem);
            }
          },
          [&sstr, data_type = message.get_data_type()](
              const Message::StringVariantT &message_data) {
            switch (data_type) {
            case DataType::SimpleString:
              sstr << "+" << message_data << TERMINATOR;
              break;
            case DataType::BulkString:
              sstr << "$" << message_data.size() << TERMINATOR << message_data
                   << TERMINATOR;
              break;
            case DataType::NullBulkString:
              sstr << "$-1" << TERMINATOR;
              break;
            case DataType::Unknown:
            case DataType::SimpleError:
            case DataType::Integer:
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
              std::cerr
                  << "Trying to stringify a Message with Unsupported DataType"
                  << std::endl;
              std::terminate();
            }
          },
      },
      message.get_data());
  return sstr.str();
}

Message generate_response_message(const Command &command, const Config &config,
                                  Cache &cache) {
  if (command.verb == CommandVerb::Ping) {
    // If PING had an argument, reply with just that argument like ECHO would.
    if (command.arguments.size() == 1) {
      const auto data_type = DataType::BulkString;
      return Message{command.arguments.front(), data_type};
    }
    // Otherwise, reply with the simple string "PONG".
    return Message{"PONG", DataType::SimpleString};
  }
  if (command.verb == CommandVerb::Echo) {
    return Message{command.arguments.front(), DataType::BulkString};
  }
  if (command.verb == CommandVerb::Get) {
    // TODO we don't currently handle "*" globs or multiple keys.
    // TODO we assume GET always comes with one and only one argument.
    const auto &key = command.arguments.front();
    const auto value = cache.get(key);
    if (value) {
      return Message{*value, DataType::BulkString};
    }
    return Message{"", DataType::NullBulkString};
  }
  if (command.verb == CommandVerb::ConfigGet) {
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
    return Message{Message::NestedVariantT{}, DataType::Array};
  }
  if (command.verb == CommandVerb::Set) {
    // Send back OK.
    return Message{"OK", DataType::SimpleString};
  }
  if (command.verb == CommandVerb::Keys) {
    // TODO actually parse the KEYS arguments (assume "*" for now).
    // Get all the keys from the cache, and make BulkString messages out of each
    // one.
    std::vector<std::string> keys = cache.keys();
    std::vector<Message> key_messages{};
    key_messages.reserve(keys.size());
    std::transform(
        keys.cbegin(), keys.cend(), std::back_inserter(key_messages),
        [](const auto &key) { return Message{key, DataType::BulkString}; });
    // Send them back as an array of these BulkString messages.
    return Message{key_messages, DataType::Array};
  }

  // Print out an error but reply with "OK".
  std::cerr << "Could not generate a valid response for the given command: "
            << command_to_string(command.verb) << ", with args (size "
            << command.arguments.size() << "): ";
  std::cerr << std::endl;
  for (const auto &arg : command.arguments) {
    std::cerr << arg << std::endl;
  }
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
  case CommandVerb::Keys:
    return "keys";
  case CommandVerb::Unknown:
  default:
    std::cerr << "Unknown CommandVerb enum encountered: "
              << static_cast<int>(command) << std::endl;
    std::terminate();
  }
}
void handle_command(const Command &command, Cache &cache) {
  // The SET command has the side-effect of updating the given key-value pairs
  // in our cache/db.
  if (command.verb == CommandVerb::Set) {
    const auto &key = command.arguments.front();
    const auto &value = command.arguments[1];
    std::optional<std::chrono::milliseconds> expiry{};
    if (command.arguments.size() == 4 && command.arguments[2] == "px") {
      auto num = std::stoul(command.arguments[3]);
      expiry = std::chrono::milliseconds(num);
    }

    cache.set(key, value, expiry);
  }
}