#include "Parser.hpp"

#include <iostream>

namespace
{
    using namespace RESP;
    DataType byte_to_data_type(char first_byte)
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
        case '_':
        case ',':
        case '(':
        case '!':
        case '=':
        case '%':
        case '~':
        case '>':
            throw UnimplementedException{};
        default:
            return DataType::Unknown;
        }
    }

    DataType get_type_from_message(const std::string &message)
    {
        return message.size() ? byte_to_data_type(message.front()) : DataType::Unknown;
    }

    Request::Command parse_command(const std::string &message)
    {
        if (message == "PING")
        {
            return Request::Command::Ping;
        }
        else
        {
            return Request::Command::Unknown;
        }
    }

} // anonymous namespace

namespace RESP
{

    // TODO I'm now mixing the server abstraction with the parser.
    Request parse_request_from_client(const int socket_fd)
    {
        // TODO assume we only get requests of up to 1024 bytes, not more.
        constexpr auto BUFFER_SIZE = 1024;
        constexpr auto FLAGS = 0;

        std::array<char, BUFFER_SIZE> buffer{};
        const auto num_bytes_read = recv(socket_fd, buffer.data(), BUFFER_SIZE, FLAGS);

        const std::string message(buffer.data(), BUFFER_SIZE);

        std::cout << "RECEIVED MESSAGE: " << message << std::endl;
        return Request::parse_request(message);
    }

    Response generate_response(const Request &request)
    {
        Response response{};
        // TODO properly do this later
        if (request.command == Request::Command::Ping)
        {
            // TODO this handles the simple string reply to a simple string Ping, need to handle array request-response.
            response.data = "+PONG\r\n";
        }
        return response;
    }

    std::string response_to_string(const Response &response)
    {
        return response.data;
    }

    Request Request::parse_request(const std::string &message)
    {
        // Based on the data type, actually parse the message into a Request.
        const auto data_type = get_type_from_message(message);
        // No arguments expected, just a single string.
        if (data_type == DataType::SimpleString)
        {
            const auto start = 1; // Skip over the data type first byte.
            const auto end = message.find(TERMINATOR);
            if (end != std::string::npos)
            {
                // TODO use a string_view to iterate over the parts of the string and extract what we want.
                // TODO whoops I forgot to parse the length here...

                // TODO parse the length
                // TODO parse the command

                const auto cmd = parse_command(message);
                return Request{cmd};
            }
            else
            {
                throw ParsingException{"Failed to parse request due to missing terminator: " + message};
            }
        }
        else
        {
            // TODO else if array type
        }

        return Request{Command::Unknown};
    }

    std::string Request::to_string(Request::Command command)
    {
        switch (command)
        {
        case Request::Command::Ping:
            return "PING";
        default:
            throw ParsingException{"Could not parse command: " + std::to_string(static_cast<int>(command))};
        }
    }
}