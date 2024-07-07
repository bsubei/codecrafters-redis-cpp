// This source file's own header include.
#include "protocol.hpp"

// System includes.
#include <cassert>
#include <iostream>
#include <sstream>
#include <variant>
#include <vector>

std::ostream &operator<<(std::ostream &os, const Message &message)
{
    os << "Message (data_type: " << static_cast<int>(message.data_type) << ", data: ";
    std::visit(MessageDataVisitor{[&os](const Message::NestedVariantT &message_data)
                                  {
                                      os << "vector of size " << message_data.size() << ": [\n";
                                      for (const auto &m : message_data)
                                      {
                                          assert(std::holds_alternative<Message::StringVariantT>(m.data) && "Nested Array messages are not allowed");
                                          os << std::get<Message::StringVariantT>(m.data) << ",\n";
                                      }
                                      os << "\n]";
                                  },
                                  [&os](const Message::StringVariantT &message_data)
                                  {
                                      os << message_data;
                                  },
                                  [](const auto &&message_data)
                                  {
                                      std::cerr << "Unable to call operator<< on message with data: " << message_data << std::endl;
                                      std::terminate();
                                  }},
               message.data);
    os << ")\n";
    return os;
}