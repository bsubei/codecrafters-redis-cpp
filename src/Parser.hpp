#include <array>
#include <string>
#include <variant>
#include <vector>
#include <optional>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <exception>

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

    // Represents the "Request" in RESP's request-response communication model. The client sends a request, and the server responds with a response.
    struct Request
    {
        // TODO consider using wise_enum to make parsing these commands easier.
        enum class Command
        {
            Unknown = 0,
            Ping = 1,
            Echo = 2,
        };
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

    std::optional<Request> parse_request_from_client(const int socket_fd);
    Response generate_response(const Request &request);
    std::string response_to_string(const Response &response);
}