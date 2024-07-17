#include <gtest/gtest.h>

#include "../src/redis_core.hpp"

TEST(MessageTest, MessageToString) {
  EXPECT_EQ(message_to_string(Message("PING", DataType::SimpleString)),
            "+PING\r\n");
  EXPECT_EQ(message_to_string(Message("ECHO", DataType::BulkString)),
            "$4\r\nECHO\r\n");
  EXPECT_EQ(
      message_to_string(Message(Message::NestedVariantT{}, DataType::Array)),
      "*0\r\n");
  EXPECT_EQ(message_to_string(Message(
                Message::NestedVariantT{
                    Message("hello", DataType::BulkString),
                    Message("goodbye", DataType::SimpleString),
                },
                DataType::Array)),
            "*2\r\n$5\r\nhello\r\n+goodbye\r\n");
  EXPECT_EQ(message_to_string(Message("", DataType::NullBulkString)),
            "$-1\r\n");
}

TEST(MessageTest, MessageFromString) {
  EXPECT_EQ(message_from_string("+OK\r\n"),
            Message("OK", DataType::SimpleString));
  EXPECT_EQ(message_from_string("$4\r\nNOPE\r\n"),
            Message("NOPE", DataType::BulkString));
  EXPECT_EQ(message_from_string("*0\r\n"),
            Message(Message::NestedVariantT{}, DataType::Array));
  EXPECT_EQ(message_from_string("*3\r\n+ONE\r\n$3\r\nTWO\r\n+three\r\n"),
            Message(
                Message::NestedVariantT{
                    Message("ONE", DataType::SimpleString),
                    Message("TWO", DataType::BulkString),
                    Message("three", DataType::SimpleString),
                },
                DataType::Array));
  // TODO we can't read NullBulkString. Need to implement it if we expect
  // clients to send us Null bulk strings.
}

TEST(MessageTest, RoundTrip) {
  const auto msg1 = Message("PING", DataType::SimpleString);
  const auto msg2 = Message("hiya there", DataType::BulkString);
  const auto msg3 =
      Message(Message::NestedVariantT{msg1, msg2}, DataType::Array);
  const auto empty = Message(Message::NestedVariantT{}, DataType::Array);

  EXPECT_EQ(msg1, message_from_string(message_to_string(msg1)));
  EXPECT_EQ(msg2, message_from_string(message_to_string(msg2)));
  EXPECT_EQ(msg3, message_from_string(message_to_string(msg3)));
  EXPECT_EQ(empty, message_from_string(message_to_string(empty)));
}