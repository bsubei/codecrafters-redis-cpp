#include "Parser.hpp"

#include <algorithm>
#include <string_view>
#include <iostream>

#include "Cache.hpp"
#include "Config.hpp"
#include "Utils.hpp"

namespace
{
    using namespace RESP;

    Request::Command parse_command(const std::vector<std::string> &elements)
    {
        const auto lower = tolower(elements.front());
        if (lower == "ping")
        {
            return Request::Command::Ping;
        }
        else if (lower == "echo")
        {
            return Request::Command::Echo;
        }
        else if (lower == "get")
        {
            return Request::Command::Get;
        }
        else if (lower == "set")
        {
            return Request::Command::Set;
        }
        else if (lower == "config")
        {
            if (elements.size() > 1 && tolower(elements[1]) == "get")
            {
                return Request::Command::ConfigGet;
            }
        }

        return Request::Command::Unknown;
    }

} // anonymous namespace

namespace RESP
{

    std::string Message::to_string() const
    {
        std::stringstream ss{};
        switch (data_type)
        {
        case DataType::Array:
            ss << "*" << std::get<std::vector<Message>>(data).size() << TERMINATOR;
            for (const auto &elem : std::get<std::vector<Message>>(data))
            {
                ss << elem.to_string();
            }
            break;
        case DataType::SimpleString:
            ss << "+" << std::get<std::string>(data) << TERMINATOR;
            break;
        case DataType::BulkString:
            ss << "$" << std::get<std::string>(data).size() << TERMINATOR << std::get<std::string>(data) << TERMINATOR;
            break;
        case DataType::Unknown:
            // TODO
            break;
        }
        return ss.str();
    }
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

    // TODO I'm now mixing the server abstraction with the parser.
    std::optional<Request> parse_request_from_client(const int socket_fd)
    {
        // TODO assume we only get requests of up to 1024 bytes, not more.
        constexpr auto BUFFER_SIZE = 1024;
        constexpr auto FLAGS = 0;

        std::array<char, BUFFER_SIZE> buffer{};
        const auto num_bytes_read = recv(socket_fd, buffer.data(), BUFFER_SIZE, FLAGS);
        if (num_bytes_read <= 0)
        {
            return std::nullopt;
        }

        const std::string message(buffer.data(), BUFFER_SIZE);
        std::cout << "Parsing message from client: " << message << std::endl;

        return Request::parse_request(message);
    }

    Response generate_response(const Request &request, Cache &cache, const Config &config)
    {
        // TODO properly do this later (make a function that returns a bulk-string or array response).
        if (request.command == Request::Command::Ping)
        {
            // TODO this handles the simple string reply to a simple string Ping, need to handle array request-response.
            // return make_message(DataType::SimpleString, "PONG");
            // return make_message<DataType::SimpleString>("PONG");
            // return Message<DataType::SimpleString>{.data = "PONG"};
            auto m = make_message("PONG", DataType::SimpleString);
            return Response{"+PONG\r\n"};
        }
        /*
        else if (request.command == Request::Command::Echo && request.arguments.size() == 1)
        {
            // TODO we assume ECHO always comes with one and only one argument.
            const auto &reply = request.arguments.front();
            std::stringstream ss;
            // Reply with bulk string with length header, then the actual string.
            ss << "$" << std::to_string(reply.size()) << TERMINATOR << reply << TERMINATOR;
            return Response{ss.str()};
        }
        else if (request.command == Request::Command::Get && request.arguments.size() == 1)
        {
            // TODO we assume GET always comes with one and only one argument.
            // TODO actually get stuff from the cache
            const auto &key = request.arguments.front();
            const auto value = cache.get(key);
            std::stringstream ss;
            // Reply with bulk string with length header, then the actual string.
            if (value.has_value())
            {
                ss << "$" << std::to_string(value->size()) << TERMINATOR << *value << TERMINATOR;
            }
            else
            {
                // Respond with "Null bulk string".
                ss << "$-1\r\n";
            }
            return Response{ss.str()};
        }
        else if (request.command == Request::Command::ConfigGet && request.arguments.size() == 1)
        {
            const auto &key = request.arguments.front();
            std::optional<std::string> value{};
            if (tolower(key) == "dir")
            {
                value = config.dir;
            }
            else if (tolower(key) == "dbfilename")
            {
                value = config.dbfilename;
            }

            std::stringstream ss;
            // Reply with array listing the key and value.
            if (value.has_value())
            {
                ss << "*2" << TERMINATOR;
                ss << "$" << std::to_string(key.size()) << TERMINATOR << key << TERMINATOR;
                ss << "$" << std::to_string(value->size()) << TERMINATOR << *value << TERMINATOR;
            }
            else
            {
                // Respond with empty array.
                ss << "*0\r\n";
            }
            return Response{ss.str()};
        }
        */

        // TODO just pretend everything's ok, even if we don't understand the command
        return Response{"+OK\r\n"};
    }

    std::string response_to_string(const Response &response)
    {
        return response.data;
    }

    Request build_request(const std::vector<std::string> &elements)
    {
        Request request{};

        if (elements.size() > 0)
        {
            const auto cmd = parse_command(elements);
            request.command = cmd;
            // Expect the ECHO command to have exactly two arguments.
            if (cmd == Request::Command::Echo && elements.size() == 2)
            {
                request.arguments.push_back(std::move(elements[1]));
            }
            else if (cmd == Request::Command::Ping && elements.size() == 2)
            {
                request.arguments.push_back(std::move(elements[1]));
            }
            else if (cmd == Request::Command::Get && elements.size() == 2)
            {
                request.arguments.push_back(std::move(elements[1]));
            }
            else if (cmd == Request::Command::Set && elements.size() >= 3)
            {
                // Take the Key and Value arguments.
                request.arguments.push_back(std::move(elements[1]));
                request.arguments.push_back(std::move(elements[2]));
                // TODO we assume the Set command only takes these arguments and in this order.
                // Now also take any expiry arguments and take care of lowercasing the PX/EX arg.
                if (elements.size() == 5)
                {
                    request.arguments.push_back(tolower(elements[3]));
                    request.arguments.push_back(std::move(elements[4]));
                }
            }
            else if (cmd == Request::Command::ConfigGet && elements.size() == 3)
            {
                // Example: "CONFIG GET dir"
                request.arguments.push_back(std::move(elements[2]));
            }
        }

        return request;
    }

    Request Request::parse_request(const std::string &message)
    {
        Request request{};
        // Based on the data type, actually parse the message into a Request.
        const auto data_type = get_type(message);
        // No arguments expected, just a single string. Example: "+PING\r\n"
        if (data_type == DataType::SimpleString)
        {
            // Just parse the one token from the start.
            return build_request(parse_string(message, 1));
        }
        else if (data_type == DataType::Array)
        {
            // We start with an incoming message that looks like this: "*2\r\n$4\r\nECHO\r\n$2\r\nhi"
            // TODO rewrite this mess and use the array length, and each bulk string length header. Stop messing about with newline tokens and string views
            auto it = message.cbegin() + 1;
            // Grab the number of elements since this is an Array message. The iter should end up pointing at the actual message.
            const auto num_tokens = parse_num(it);
            // Now the remainder of our message looks like this: "$4\r\nECHO\r\n$2\r\nhi"
            // Parse each of the tokens (e.g. "ECHO" and "hi" in the above example).
            // Build a Request object out of these string tokens.
            return build_request(parse_string(std::string_view(it, message.cend()), num_tokens));
        }

        return request;
    }

    std::string Request::to_string(Request::Command command)
    {
        switch (command)
        {
        case Request::Command::Unknown:
            return "UNKNOWN COMMAND";
            // throw ParsingException{"Could not parse command: " + std::to_string(static_cast<int>(command))};
        case Request::Command::Ping:
            return "ping";
        case Request::Command::Echo:
            return "echo";
        case Request::Command::Set:
            return "set";
        case Request::Command::Get:
            return "get";
        case Request::Command::ConfigGet:
            return "config get";
        }
        std::cerr << "Unknown Request::Command enum encountered: " << static_cast<int>(command) << std::endl;
        std::terminate();
    }
}