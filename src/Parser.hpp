#include <string>

namespace RESP
{
    class UnimplementedException : public std::exception
    {
    };

    class Parser
    {
        // This is the terminator for the RESP protocol that separates its parts.
        static constexpr auto TERMINATOR = "\r\n";
    };

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
}