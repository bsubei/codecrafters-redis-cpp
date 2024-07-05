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

    // NOTE: we return a reference to the strings that are in the given message arguments. This is
    // fine as long as the caller doesn't hold on to these references longer than they do the
    // Message they passed in as an argument.
    const std::string &get_first_elem(const Message &message)
    {
        switch (message.data_type)
        {
        case DataType::Array:
        {
            const auto &messages = std::get<std::vector<Message>>(message.data);
            const auto &first_message = messages.front();
            assert(first_message.data_type != DataType::Array && "Nested Array messages are not allowed!");
            return std::get<std::string>(first_message.data);
        }
        case DataType::BulkString:
        case DataType::SimpleString:
            return std::get<std::string>(message.data);
        default:
            std::cerr << "Cannot parse command from client message of unknown data type: "
                      << static_cast<int>(message.data_type) << std::endl;
            std::terminate();
        }
    }

    bool is_array(const Message &message)
    {
        return message.data_type == DataType::Array;
    }

    // TODO we can probably get away with moving the message when calling this func since it's not used afterwards.
    std::optional<Command> parse_and_validate_command(const Message &message)
    {
        const auto first_elem = tolower(get_first_elem(message));
        // Two args in the message means the command has one argument.
        const bool is_array_and_has_two_elements = is_array(message) && std::get<std::vector<Message>>(message.data).size() == 2;
        if (first_elem == "ping")
        {
            // PING can be either a simple string on its own or an Array if an argument is provided.
            if (is_array_and_has_two_elements)
            {
                const auto &arg = std::get<std::vector<Message>>(message.data)[1];
                assert(arg.data_type != DataType::Array && "Nested Array messages are not allowed!");
                return Command{CommandVerb::Ping, {std::get<std::string>(arg.data)}};
            }
            return Command{CommandVerb::Ping, {}};
        }
        else if (first_elem == "echo" && is_array_and_has_two_elements)
        {
            // ECHO requires exactly one argument and therefore must be an Array message.
            if (is_array_and_has_two_elements)
            {
                const auto &arg = std::get<std::vector<Message>>(message.data)[1];
                assert(arg.data_type != DataType::Array && "Nested Array messages are not allowed!");
                return Command{CommandVerb::Echo, {std::get<std::string>(arg.data)}};
            }
        }
        else if (first_elem == "get" && is_array_and_has_two_elements)
        {
            // GET requires exactly one argument and therefore must be an Array message.
            if (is_array_and_has_two_elements)
            {
                const auto &arg = std::get<std::vector<Message>>(message.data)[1];
                assert(arg.data_type != DataType::Array && "Nested Array messages are not allowed!");
                return Command{CommandVerb::Get, {std::get<std::string>(arg.data)}};
            }
        }
        else if (first_elem == "set")
        {
            // SET must have at least two arguments (the key and value to set).
            if (is_array(message))
            {
                const auto &messages = std::get<std::vector<Message>>(message.data);
                for (const auto &m : messages)
                {
                    assert(m.data_type != DataType::Array && "Nested Array messages are not allowed!");
                }
                // The first element is the command, so three elements means we have two arguments to the command.
                const bool has_at_least_three_elements = messages.size() >= 3;
                if (has_at_least_three_elements)
                {
                    std::vector<std::string> args{};
                    // Copy the messages (skipping the first) as strings and make them the command arguments.
                    std::transform(messages.cbegin() + 1, messages.cend(), std::back_inserter(args), [](const auto &m)
                                   { return std::get<std::string>(m.data); });
                    return Command{CommandVerb::Set, std::move(args)};
                }
            }
        }
        else if (first_elem == "config")
        {
            // CONFIG GET must provide at least one argument.
            if (is_array(message))
            {
                const auto &messages = std::get<std::vector<Message>>(message.data);
                for (const auto &m : messages)
                {
                    assert(m.data_type != DataType::Array && "Nested Array messages are not allowed!");
                }
                // The first two elements make up the command, so three elements means we have one argument to the command.
                const bool has_at_least_three_elements = std::get<std::vector<Message>>(message.data).size() >= 3;
                if (has_at_least_three_elements && tolower(std::get<std::string>(messages[1].data)) == "get")
                {
                    std::vector<std::string> args{};
                    // Copy the messages (skipping the first two) as strings and make them the command arguments.
                    std::transform(messages.cbegin() + 2, messages.cend(), std::back_inserter(args), [](const auto &m)
                                   { return std::get<std::string>(m.data); });
                    return Command{CommandVerb::ConfigGet, std::move(args)};
                }
            }
        }

        return std::nullopt;
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
        case DataType::NullBulkString:
            ss << "$-1" << TERMINATOR;
            break;
        case DataType::Unknown:
            std::cerr << "Trying to stringify a Message with Unknown DataType" << std::endl;
            std::terminate();
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
    std::optional<Message> parse_message_from_client(const int socket_fd)
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

        const std::string request(buffer.data(), BUFFER_SIZE);
        std::cout << "Parsing request from client: " << request << std::endl;

        return Message::from_string(request);
    }

    Message generate_response_message(const Message &request_message, Cache &cache, const Config &config)
    {
        // Figure out what command is being sent to us in the request from the client.
        // This function also makes sure the Message  has the correct form (Array type if needed, and number of arguments).
        const auto command = parse_and_validate_command(request_message);

        // Respond with "+OK\r\n" if we don't know how to handle this command.
        if (!command)
        {
            // Print out an error but reply with "OK".
            std::cerr << "Could not parse command from given reqeust: " << request_message.to_string() << std::endl;
            return Message{"OK", DataType::SimpleString};
        }

        // Handle any state changes we need to do before replying to the client.
        // The SET command has the side-effect of updating the given key-value pairs in our cache/db.
        if (command->verb == CommandVerb::Set)
        {
            const auto &key = command->arguments.front();
            const auto &value = command->arguments[1];
            std::optional<std::chrono::milliseconds> expiry{};
            if (command->arguments.size() == 4 && command->arguments[2] == "px")
            {
                auto num = std::stoi(command->arguments[3]);
                expiry = std::chrono::milliseconds(num);
            }

            cache.set(key, value, expiry);
        }

        // Now return the response Message we should reply with.
        Message response_message{};

        // TODO properly do this later (make a function that returns a bulk-string or array response).
        if (command->verb == CommandVerb::Ping)
        {
            // If PING had an argument, reply with PONG and that argument (as bulk strings).
            if (command->arguments.size() == 1)
            {
                const auto data_type = DataType::BulkString;
                return make_message(
                    std::vector<Message>{
                        make_message("PONG", data_type),
                        make_message(command->arguments.front(), data_type)},
                    DataType::Array);
            }
            // Otherwise, reply with the simple string "PONG".
            return make_message("PONG", DataType::SimpleString);
        }
        else if (command->verb == CommandVerb::Echo)
        {
            // TODO we assume ECHO always comes with one and only one argument. We may need to revisit this.
            return make_message(command->arguments.front(), DataType::BulkString);
        }
        else if (command->verb == CommandVerb::Get)
        {
            // TODO we don't currently handle "*" globs or multiple keys.
            // TODO we assume GET always comes with one and only one argument.
            const auto &key = command->arguments.front();
            const auto value = cache.get(key);
            if (value)
            {
                return make_message(*value, DataType::BulkString);
            }
            // TODO need to handle null bulk string properly
            return make_message("", DataType::NullBulkString);
        }
        else if (command->verb == CommandVerb::ConfigGet)
        {
            // TODO we don't currently handle "*" globs or multiple keys.
            const auto &key = command->arguments.front();
            std::optional<std::string> value{};
            if (tolower(key) == "dir")
            {
                value = config.dir;
            }
            else if (tolower(key) == "dbfilename")
            {
                value = config.dbfilename;
            }

            // Reply with array listing the key and value if found.
            if (value.has_value())
            {
                return make_message(
                    std::vector<Message>{
                        make_message(key, DataType::BulkString),
                        make_message(*value, DataType::BulkString)},
                    DataType::Array);
            }
            // Otherwise, respond with empty array.
            return make_message(std::vector<Message>{}, DataType::Array);
        }

        // Print out an error but reply with "OK".
        std::cerr << "Could not generate a valid response for the given command: " << Command::to_string(command->verb)
                  << ", with args (size " << command->arguments.size() << "): ";
        for (const auto &arg : command->arguments)
        {
            std::cerr << arg;
        }
        std::cerr << std::endl;
        return Message{"OK", DataType::SimpleString};
    }

    std::string Command::to_string(CommandVerb command)
    {
        switch (command)
        {
        case CommandVerb::Unknown:
            return "UNKNOWN COMMAND";
            // throw ParsingException{"Could not parse command: " + std::to_string(static_cast<int>(command))};
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
        }
        std::cerr << "Unknown CommandVerb enum encountered: " << static_cast<int>(command) << std::endl;
        std::terminate();
    }
    /*
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
            if (cmd == Command::Echo && elements.size() == 2)
            {
                request.arguments.push_back(std::move(elements[1]));
            }
            else if (cmd == Command::Ping && elements.size() == 2)
            {
                request.arguments.push_back(std::move(elements[1]));
            }
            else if (cmd == Command::Get && elements.size() == 2)
            {
                request.arguments.push_back(std::move(elements[1]));
            }
            else if (cmd == Command::Set && elements.size() >= 3)
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
            else if (cmd == Command::ConfigGet && elements.size() == 3)
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

    std::string Request::to_string(Command command)
    {
        switch (command)
        {
        case Command::Unknown:
            return "UNKNOWN COMMAND";
            // throw ParsingException{"Could not parse command: " + std::to_string(static_cast<int>(command))};
        case Command::Ping:
            return "ping";
        case Command::Echo:
            return "echo";
        case Command::Set:
            return "set";
        case Command::Get:
            return "get";
        case Command::ConfigGet:
            return "config get";
        }
        std::cerr << "Unknown Command enum encountered: " << static_cast<int>(command) << std::endl;
        std::terminate();
    }
    */
}