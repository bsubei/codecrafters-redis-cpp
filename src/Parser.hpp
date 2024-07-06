#pragma once

// TODO clean up these headers, way too many
#include <array>
#include <algorithm>
#include <string>
#include <variant>
#include <vector>
#include <optional>
#include <cstring>
#include <cstdint>
#include <unistd.h>
#include <cassert>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <exception>
#include <sstream>
#include <variant>
#include <iostream>
#include <utility>

#include "Utils.hpp"

class Config;
class Cache;

namespace RESP
{
    class UnimplementedException : public std::exception
    {
    };

    class ParsingException : public std::exception
    {
    public:
        explicit ParsingException(const std::string &message) : message_(message) {}

        virtual const char *what() const noexcept override
        {
            return message_.c_str();
        }

    private:
        std::string message_;
    };

    // This is the terminator for the RESP protocol that separates its parts.
    static constexpr auto TERMINATOR = "\r\n";

    // See the table of data types in https://redis.io/docs/latest/develop/reference/protocol-spec/#resp-protocol-description
    enum class DataType
    {
        Unknown,
        // Comes in this format: +<data>\r\n
        SimpleString,
        SimpleError,
        Integer,
        // Also known as binary string.
        // Comes in this format: $<length>\r\n<data>\r\n
        BulkString,
        // TODO we currently don't handle reading NullBulkString correctly, only writing it out.
        NullBulkString,
        // Comes in this format: *<num_elems>\r\n<elem_1>\r\n....<elem_n>\r\n
        Array,
        // The rest of these are RESP3, which we don't implement
        Null,
        Boolean,
        Double,
        BigNumber,
        BulkError,
        VerbatimString,
        Map,
        Set,
        Push,
    };

    DataType byte_to_data_type(char first_byte);

    template <StringLike StringType>
    DataType get_type(const StringType &s)
    {
        return s.size() > 0 ? byte_to_data_type(s.front()) : DataType::Unknown;
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

    // Given a string of the Array contents (of size n), break it up into n tokens. We can't simply split by the terminator because some elements are complex (bulk string).
    // Each returned token is the unparsed string. e.g. for an input of "*2\r\n$4\r\nECHO\r\n$2\r\nhi", we return:
    // ["$4\r\nECHO\r\n", "$2\r\nhi"]
    template <StringLike StringType>
    std::vector<std::string> tokenize_array(const StringType &s, const int num_tokens)
    {
        std::vector<std::string> tokens{};
        tokens.reserve(num_tokens);
        auto it = s.cbegin();
        // Skip past the array length part, jump right into the actual array contents.
        move_past_terminator(it);
        for (int i = 0; i < num_tokens; ++i)
        {
            auto terminator_it = it;
            switch (byte_to_data_type(*it))
            {
            case DataType::SimpleString:
                move_past_terminator(terminator_it);
                tokens.emplace_back(&*it, terminator_it - it);
                it = terminator_it;
                break;
            case DataType::BulkString:
                move_past_terminator(terminator_it);
                move_past_terminator(terminator_it);
                tokens.emplace_back(&*it, terminator_it - it);
                it = terminator_it;
                break;
            default:
                std::cerr << "Unable to tokenize array for given s: " << s << std::endl;
                std::terminate();
            }
        }

        return tokens;
    }

    // TODO eventually use the length in the header to efficiently read and also correctly handle text with newlines in it.
    // TODO make this more efficient and avoid creating strings and passing them around
    template <StringLike StringType>
    std::vector<std::string> parse_string(const StringType &s, const int num_tokens)
    {
        // TODO probably better to do this with istringstream
        std::vector<std::string> tokens{};
        auto it = s.cbegin();
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
                // TODO I think there's a bug here where it is not updated to move past terminator_it(??)
            }
        }

        return tokens;
    }

    // Unfortunately, we can't encode the DataType as a template parameter because then we can't have Array
    // messages that contain a mixed array of any Message type. I settled on using a variant and using the
    // DataType member as the thing that tells both which variant of data to use and how to convert to/from
    // a string.
    struct Message
    {
        // TODO clean up all the annoying std::get<> when we access this variant.
        using data_variant_t = std::variant<std::string, std::vector<Message>>;
        data_variant_t data{};
        DataType data_type{};

        Message() = default;

        template <typename T>
        Message(T &&data_in, DataType data_type_in) : data(std::forward<T>(data_in)),
                                                      data_type(data_type_in) {}

        bool operator==(const Message &other) const = default;

        std::string to_string() const;
        // For displaying in GTEST.
        friend std::ostream &operator<<(std::ostream &os, const Message &message);

        // TODO update this func to accept string literals so we don't have to create temporary strings just to create a Message.
        template <StringLike StringType>
        static Message from_string(const StringType &s)
        {
            // TODO if given string is missing terminators, we do undefined behavior

            // TODO handle case insensitivity

            // TODO redo all comments
            // Based on the data type, actually parse the message into a Request.
            const auto data_type = get_type(s);
            std::vector<std::string> tokens{};

            switch (data_type)
            {
            case DataType::BulkString:
            case DataType::SimpleString:
                tokens = parse_string(s, 1);
                return Message{tokens.front(), data_type};
            case DataType::Array:
            {
                // We start with an incoming message that looks like this: "*2\r\n$4\r\nECHO\r\n$2\r\nhi"
                auto it = s.cbegin() + 1;
                // Grab the number of elements since this is an Array message. The iter should end up pointing at the actual message.
                // TODO don't need this weird iterator mutating func
                const auto num_tokens = parse_num(it);
                // Now the remainder of our message looks like this: "$4\r\nECHO\r\n$2\r\nhi"
                // Parse each of the tokens (e.g. "ECHO" and "hi" in the above example).
                const auto tokens = tokenize_array(s, num_tokens);
                std::vector<Message> message_data{};
                message_data.reserve(tokens.size());
                std::transform(tokens.cbegin(), tokens.cend(), std::back_inserter(message_data), [](const auto &token)
                               { return from_string(token); });
                return Message(std::move(message_data), data_type);
            }
            // TODO might need to eventually handle NullBulkString if we expect clients to send us nils.
            default:
                std::cerr << "from_string unimplemented for data_type " << static_cast<int>(data_type) << ", was given string: " << s << std::endl;
                std::terminate();
            }
            std::unreachable();
        }
        static Message from_string(const char *s)
        {
            return from_string(std::string(s));
        }
    };

    template <typename T>
    Message make_message(T &&data, DataType data_type)
    {
        return Message(std::forward<T>(data), data_type);
    }
    // Represents the "Request" in RESP's request-response communication model. The client sends a request, and the server responds with a response.
    /*
    struct Request
    {
        static std::string to_string(Command command);

        static Request parse_request(const std::string &message);

        Command command{};
        std::vector<std::string> arguments{};
    };

    // Represents the "Response" in RESP's request-response communication model. The client sends a request, and the server responds with a response.
    struct Response
    {
        Response(const std::string &data) : data(data) {}
        // TODO for now, just store the response string here, think about what this class should look like.
        std::string data{};
    };
    */

    // TODO consider using wise_enum to make parsing these commands easier.
    enum class CommandVerb : std::uint8_t
    {
        Unknown = 0,
        Ping = 1,
        Echo = 2,
        Set = 3,
        Get = 4,
        ConfigGet = 5,
    };

    struct Command
    {
        CommandVerb verb{};
        std::vector<std::string> arguments{};

        static std::string to_string(CommandVerb command);
    };

    std::optional<Message> parse_message_from_client(const int socket_fd);
    Message generate_response_message(const Message &request_message, Cache &cache, const Config &config);
    // std::string response_to_string(const Response &response);
}