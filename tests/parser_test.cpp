#include <gtest/gtest.h>

#include "../src/parser.hpp"

TEST(MessageTest, MessageToString)
{
    EXPECT_EQ(message_to_string(make_message("PING", DataType::SimpleString)),
              "+PING\r\n");
    EXPECT_EQ(message_to_string(make_message("ECHO", DataType::BulkString)),
              "$4\r\nECHO\r\n");
    EXPECT_EQ(message_to_string(make_message(std::vector<Message>{}, DataType::Array)),
              "*0\r\n");
    EXPECT_EQ(
        message_to_string(make_message(std::vector<Message>{
                                           make_message("hello", DataType::BulkString),
                                           make_message("goodbye", DataType::SimpleString),
                                       },
                                       DataType::Array)),
        "*2\r\n$5\r\nhello\r\n+goodbye\r\n");
    EXPECT_EQ(message_to_string(make_message("", DataType::NullBulkString)),
              "$-1\r\n");
}

TEST(MessageTest, MessageFromString)
{
    EXPECT_EQ(message_from_string("+OK\r\n"), make_message("OK", DataType::SimpleString));
    EXPECT_EQ(message_from_string("$4\r\nNOPE\r\n"), make_message("NOPE", DataType::BulkString));
    EXPECT_EQ(message_from_string("*0\r\n"), make_message(std::vector<Message>{}, DataType::Array));
    EXPECT_EQ(
        message_from_string("*3\r\n+ONE\r\n$3\r\nTWO\r\n+three\r\n"),
        make_message(std::vector<Message>{
                         make_message("ONE", DataType::SimpleString),
                         make_message("TWO", DataType::BulkString),
                         make_message("three", DataType::SimpleString),
                     },
                     DataType::Array));
    // TODO we can't read NullBulkString. Need to implement it if we expect clients to send us Null bulk strings.
}

TEST(MessageTest, RoundTrip)
{
    const auto m1 = make_message("PING", DataType::SimpleString);
    const auto m2 = make_message("hiya there", DataType::BulkString);
    const auto m3 = make_message(std::vector<Message>{m1, m2}, DataType::Array);
    const auto empty = make_message(std::vector<Message>{}, DataType::Array);

    EXPECT_EQ(m1, message_from_string(message_to_string(m1)));
    EXPECT_EQ(m2, message_from_string(message_to_string(m2)));
    EXPECT_EQ(m3, message_from_string(message_to_string(m3)));
    EXPECT_EQ(empty, message_from_string(message_to_string(empty)));
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}