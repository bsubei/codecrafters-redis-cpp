// This source file's own header include.
#include "protocol.hpp"

// System includes.
#include <cassert>
#include <exception>
#include <iostream>
#include <ostream>
#include <variant>

// TODO this is causing issues in MSAN
std::ostream &operator<<(std::ostream &outs, const Message &message) {
  outs << "Message (data_type: " << static_cast<int>(message.data_type)
       << ", data: ";
  std::visit(MessageDataVisitor{
                 [&outs](const Message::NestedVariantT &message_data) {
                   outs << "vector of size " << message_data.size() << ": [\n";
                   for (const auto &message : message_data) {
                     assert(std::holds_alternative<Message::StringVariantT>(
                                message.data) &&
                            "Nested Array messages are not allowed");
                     outs << std::get<Message::StringVariantT>(message.data)
                          << ",\n";
                   }
                   outs << "\n]";
                 },
                 [&outs](const Message::StringVariantT &message_data) {
                   outs << message_data;
                 },
                 [](const auto &&message_data) {
                   std::cerr
                       << "Unable to call operator<< on message with data: "
                       << message_data << std::endl;
                   std::terminate();
                 }},
             message.data);
  outs << ")\n";
  return outs;
}