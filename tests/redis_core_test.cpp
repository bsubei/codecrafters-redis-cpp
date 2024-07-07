#include <gtest/gtest.h>

#include "../src/redis_core.hpp"

TEST(MessageTest, MessageToString)
{
    EXPECT_EQ(message_to_string(Message("PING", DataType::SimpleString)),
              "+PING\r\n");
    EXPECT_EQ(message_to_string(Message("ECHO", DataType::BulkString)),
              "$4\r\nECHO\r\n");
    EXPECT_EQ(message_to_string(Message(Message::NestedVariantT{}, DataType::Array)),
              "*0\r\n");
    EXPECT_EQ(
        message_to_string(Message(Message::NestedVariantT{
                                      Message("hello", DataType::BulkString),
                                      Message("goodbye", DataType::SimpleString),
                                  },
                                  DataType::Array)),
        "*2\r\n$5\r\nhello\r\n+goodbye\r\n");
    EXPECT_EQ(message_to_string(Message("", DataType::NullBulkString)),
              "$-1\r\n");
}

TEST(MessageTest, MessageFromString)
{
    EXPECT_EQ(message_from_string("+OK\r\n"), Message("OK", DataType::SimpleString));
    EXPECT_EQ(message_from_string("$4\r\nNOPE\r\n"), Message("NOPE", DataType::BulkString));
    EXPECT_EQ(message_from_string("*0\r\n"), Message(Message::NestedVariantT{}, DataType::Array));
    EXPECT_EQ(
        message_from_string("*3\r\n+ONE\r\n$3\r\nTWO\r\n+three\r\n"),
        Message(Message::NestedVariantT{
                    Message("ONE", DataType::SimpleString),
                    Message("TWO", DataType::BulkString),
                    Message("three", DataType::SimpleString),
                },
                DataType::Array));
    // TODO we can't read NullBulkString. Need to implement it if we expect clients to send us Null bulk strings.
}

TEST(MessageTest, RoundTrip)
{
    const auto m1 = Message("PING", DataType::SimpleString);
    const auto m2 = Message("hiya there", DataType::BulkString);
    const auto m3 = Message(Message::NestedVariantT{m1, m2}, DataType::Array);
    const auto empty = Message(Message::NestedVariantT{}, DataType::Array);

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