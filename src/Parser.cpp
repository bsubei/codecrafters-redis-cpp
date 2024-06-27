#include "Parser.hpp"

#include <string_view>
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
        return message.size() > 0 ? byte_to_data_type(message.front()) : DataType::Unknown;
    }

    Request::Command parse_command(const std::string_view &message)
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
        std::cout << "Parsing message from client: " << message << std::endl;

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
        // TODO eventually use the length in the header to efficiently read.
        // Based on the data type, actually parse the message into a Request.
        const auto data_type = get_type_from_message(message);
        std::string_view message_sv{message};
        // No arguments expected, just a single string. Example: "+$4\r\nPING\r\n"
        if (data_type == DataType::SimpleString)
        {
            auto start = 1; // Skip over the data type first byte (the plus sign).
            auto end = message_sv.find(TERMINATOR);
            if (end == std::string::npos)
            {
                throw ParsingException{"Failed to parse body of simple string due to missing terminator: " + std::string{message_sv}};
            }
            message_sv = message_sv.substr(start, end - 1);
            const auto cmd = parse_command(message_sv);
            return Request{cmd};
        }
        else if (data_type == DataType::BulkString)
        {
            // TODO actually handle the bulk string case, the below is incomplete

            auto start = 1; // Skip over the data type first byte (the plus sign).
            auto end = message_sv.find(TERMINATOR);
            if (end == std::string::npos)
            {
                throw ParsingException{"Failed to parse length part of the header due to missing terminator: " + std::string{message_sv}};
            }

            // Skip over the CRLF to the body of the message.
            start = end + 2;
            message_sv = message_sv.substr(start);
            end = message_sv.find(TERMINATOR);
            if (end == std::string::npos)
            {
                throw ParsingException{"Failed to parse the body of the message due to missing terminator: " + std::string{message_sv}};
            }

            message_sv = message_sv.substr(0, end);
            const auto cmd = parse_command(message_sv);
            return Request{cmd};
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