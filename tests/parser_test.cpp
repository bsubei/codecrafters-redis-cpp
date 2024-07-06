#include <gtest/gtest.h>

#include "../src/parser.hpp"

TEST(MessageTest, ToString)
{
    EXPECT_EQ(make_message("PING", DataType::SimpleString).to_string(),
              "+PING\r\n");
    EXPECT_EQ(make_message("ECHO", DataType::BulkString).to_string(),
              "$4\r\nECHO\r\n");
    EXPECT_EQ(make_message(std::vector<Message>{}, DataType::Array).to_string(),
              "*0\r\n");
    EXPECT_EQ(
        make_message(std::vector<Message>{
                         make_message("hello", DataType::BulkString),
                         make_message("goodbye", DataType::SimpleString),
                     },
                     DataType::Array)
            .to_string(),
        "*2\r\n$5\r\nhello\r\n+goodbye\r\n");
    EXPECT_EQ(make_message("", DataType::NullBulkString).to_string(),
              "$-1\r\n");
}

TEST(MessageTest, FromString)
{
    EXPECT_EQ(Message::from_string("+OK\r\n"), make_message("OK", DataType::SimpleString));
    EXPECT_EQ(Message::from_string("$4\r\nNOPE\r\n"), make_message("NOPE", DataType::BulkString));
    EXPECT_EQ(Message::from_string("*0\r\n"), make_message(std::vector<Message>{}, DataType::Array));
    EXPECT_EQ(
        Message::from_string("*3\r\n+ONE\r\n$3\r\nTWO\r\n+three\r\n"),
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

    EXPECT_EQ(m1, Message::from_string(m1.to_string()));
    EXPECT_EQ(m2, Message::from_string(m2.to_string()));
    EXPECT_EQ(m3, Message::from_string(m3.to_string()));
    EXPECT_EQ(empty, Message::from_string(empty.to_string()));
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}