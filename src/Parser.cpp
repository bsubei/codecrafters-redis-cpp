#include "Parser.hpp"

#include <algorithm>
#include <string_view>
#include <sstream>
#include <iostream>

#include "Cache.hpp"
#include "Utils.hpp"

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
        const auto lower = tolower(message);
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
        else
        {
            return Request::Command::Unknown;
        }
    }

    void move_up_to_terminator(auto &it)
    {
        while (*it != '\r')
            ++it;
    }
    void move_past_terminator(auto &it)
    {
        move_up_to_terminator(it);
        // Move past the '\r'
        ++it;
        // Move past the '\n'
        ++it;
    }

    // Given an iterator starting at a number, parse that number and move the iterator just past the CRLF newline terminator.
    int parse_num(auto &it)
    {
        // From https://redis.io/docs/latest/develop/reference/protocol-spec/#high-performance-parser-for-the-redis-protocol
        int len = 0;
        while (*it != '\r')
        {
            len = (len * 10) + (*it - '0');
            ++it;
        }
        move_past_terminator(it);
        return len;
    }

    // TODO eventually use the length in the header to efficiently read and also correctly handle text with newlines in it.
    // TODO make this more efficient and avoid creating strings and passing them around
    std::vector<std::string> parse_tokens(auto &it, const int num_tokens)
    {
        std::vector<std::string> tokens{};
        DataType data_type = byte_to_data_type(*it);

        // Given a message that looks like this: "$4\r\nECHO\r\n$2\r\nhi", parse the ECHO and hi parts. We ignore the length headers.
        for (int i = 0; i < num_tokens; ++i)
        {
            if (data_type == DataType::BulkString)
            {
                // If the it points at a bulk string, just ignore the length header.
                move_past_terminator(it);

                // Now read the actual string.
                auto terminator_it = it;
                move_up_to_terminator(terminator_it);
                tokens.emplace_back(&*it, &*terminator_it);
                // Move the iterator to the next token.
                move_past_terminator(it);
            }
            else if (data_type == DataType::SimpleString)
            {
                // Move past the '+' char.
                ++it;
                // Now read the actual string.
                auto terminator_it = it;
                move_up_to_terminator(terminator_it);
                tokens.emplace_back(&*it, &*terminator_it);
            }
        }

        return tokens;
    }
} // anonymous namespace

namespace RESP
{

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

    Response generate_response(const Request &request, Cache &cache)
    {
        // TODO properly do this later
        if (request.command == Request::Command::Ping)
        {
            // TODO this handles the simple string reply to a simple string Ping, need to handle array request-response.
            return Response{"+PONG\r\n"};
        }
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
            std::cout << "GETTING VALUE FROM CACHE USING KEY: " << key << std::endl;
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

        // TODO just pretend everything's ok, even if we don't understand the command
        return Response{"+OK\r\n"};
    }

    std::string response_to_string(const Response &response)
    {
        return response.data;
    }

    Request build_request(std::vector<std::string> elements)
    {
        Request request{};

        if (elements.size() > 0)
        {
            const auto cmd = parse_command(tolower(elements.front()));
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
        }

        return request;
    }

    Request Request::parse_request(const std::string &message)
    {
        Request request{};
        // Based on the data type, actually parse the message into a Request.
        const auto data_type = get_type_from_message(message);
        std::string_view message_sv{message};
        // No arguments expected, just a single string. Example: "+PING\r\n"
        if (data_type == DataType::SimpleString)
        {
            // Just parse the one token from the start.
            auto it = message.cbegin();
            return build_request(parse_tokens(it, 1));
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
            return build_request(parse_tokens(it, num_tokens));
        }

        return request;
    }

    std::string Request::to_string(Request::Command command)
    {
        switch (command)
        {
        case Request::Command::Ping:
            return "ping";
        case Request::Command::Echo:
            return "echo";
        default:
            return "UNKNOWN COMMAND";
            // throw ParsingException{"Could not parse command: " + std::to_string(static_cast<int>(command))};
        }
    }
}