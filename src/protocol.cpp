// This source file's own header include.
#include "protocol.hpp"

// System includes.
#include <cassert>
#include <sstream>

std::ostream &operator<<(std::ostream &os, const Message &message)
{
    os << "Message (data_type: " << static_cast<int>(message.data_type) << ", data: ";
    switch (message.data_type)
    {
    case DataType::Array:
    {
        const auto &messages = std::get<std::vector<Message>>(message.data);
        os << "vector of size " << messages.size() << ": [\n";
        for (const auto &m : messages)
        {
            assert(m.data_type != DataType::Array && "Nested Array messages are not allowed");
            os << std::get<std::string>(m.data) << ",\n";
        }
        os << "\n]";
        break;
    }
    default:
        os << std::get<std::string>(message.data);
        break;
    }
    os << ")\n";
    return os;
}