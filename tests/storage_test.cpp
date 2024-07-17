#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <sstream>
#include <string>

#include "../src/storage.hpp"

namespace {
std::string get_random_string_n_bytes(const std::size_t length) {
  // These are declared thread_local so we don't needlessly create them for
  // every function call, and at the same time preserving thread safety.
  // Use a fixed seed so our tests are deterministic.
  // NOLINTNEXTLINE(cert-msc51-cpp, cert-msc32-c)
  thread_local std::mt19937 generator(42);
  // We generate each byte randomly (all values are possible).
  thread_local std::uniform_int_distribution<> distribution(0, 255);

  std::string result(length, 0);
  std::generate_n(result.begin(), length,
                  [&]() { return static_cast<char>(distribution(generator)); });
  return result;
}
} // namespace

TEST(StorageTest, ParseLengthEncodedString) {
  // Empty input causes program termination.
  std::istringstream empty_ss("");
  ASSERT_DEATH(
      { parse_length_encoded_string(empty_ss); }, "Unable to read length byte");
  // TODO test other failure cases

  // Test case: length encoding bits are 00 and length is zero.
  // NOTE: we have to be a little careful because std::string treats "\0" bytes
  // as a c-str null char.
  std::istringstream input{std::string("\x00", 1)};
  std::string expected_output{};
  std::string actual_output = parse_length_encoded_string(input);
  EXPECT_EQ(actual_output.size(), expected_output.size());
  EXPECT_EQ(actual_output, expected_output);

  // Test case: length encoding bits are 00 and length is one.
  input = std::istringstream{"\x01S"};
  expected_output = "S";
  actual_output = parse_length_encoded_string(input);
  EXPECT_EQ(actual_output.size(), expected_output.size());
  EXPECT_EQ(actual_output, expected_output);

  // Test case: length encoding bits are 00 and length is 12.
  input = std::istringstream{"\x0Cqwertydvorak"};
  expected_output = "qwertydvorak";
  actual_output = parse_length_encoded_string(input);
  EXPECT_EQ(actual_output.size(), expected_output.size());
  EXPECT_EQ(actual_output, expected_output);

  // Test case: length encoding bits are 00 and length is 63 (the edge
  // case).
  expected_output = get_random_string_n_bytes(63);
  input = std::istringstream{"\x3F" + expected_output};
  actual_output = parse_length_encoded_string(input);
  EXPECT_EQ(actual_output.size(), expected_output.size());
  EXPECT_EQ(actual_output, expected_output);

  // Test case: length encoding bits are 01 and length is 64 (edge case).
  expected_output = get_random_string_n_bytes(64);
  input = std::istringstream{"\x40\x40" + expected_output};
  actual_output = parse_length_encoded_string(input);
  EXPECT_EQ(actual_output.size(), expected_output.size());
  EXPECT_EQ(actual_output, expected_output);

  // Test case: length encoding bits are 01 and length is 700. If we drop the
  // top two bits (the '4'), we get 0x02BC, which is 700.
  expected_output = get_random_string_n_bytes(700);
  input = std::istringstream{"\x42\xBC" + expected_output};
  actual_output = parse_length_encoded_string(input);
  EXPECT_EQ(actual_output.size(), expected_output.size());
  EXPECT_EQ(actual_output, expected_output);

  // Test case: length encoding bits are 01 and length is 16383. If we drop the
  // top two bits, the remaining 14 bits are 0x3FFF, which is 16383.
  expected_output = get_random_string_n_bytes(16383);
  input = std::istringstream{"\x7F\xFF" + expected_output};
  actual_output = parse_length_encoded_string(input);
  EXPECT_EQ(actual_output.size(), expected_output.size());
  EXPECT_EQ(actual_output, expected_output);

  // Test case: length encoding bits are 10 and length is 16384.
  // The first byte "\x80" has length encoding bits 10 (the rest of the byte is
  // discarded). The next four bytes come in little-endian and make up the
  // length 16384 (which is the same as the swapped bytes: 0x00004000).
  expected_output = get_random_string_n_bytes(16384);
  input = std::istringstream{"\x80" + std::string("\x00", 1) + "\x40" +
                             std::string(2, '\x00') + expected_output};
  actual_output = parse_length_encoded_string(input);
  EXPECT_EQ(actual_output.size(), expected_output.size());
  EXPECT_EQ(actual_output, expected_output);

  // Test case: length encoding bits are 10 and length is 17000.
  // The first byte "\x80" has length encoding bits 10 (the rest of the byte is
  // discarded). The next four bytes come in little-endian and make up the
  // length 17000 (which is the same as the swapped bytes: 0x00004268).
  expected_output = get_random_string_n_bytes(17000);
  input = std::istringstream{"\x80\x68\x42" + std::string(2, '\x00') +
                             expected_output};
  actual_output = parse_length_encoded_string(input);
  EXPECT_EQ(actual_output.size(), expected_output.size());
  EXPECT_EQ(actual_output, expected_output);

  // Test case: length encoding bits are 11 and the string an 8-bit integer with
  // value 0.
  expected_output = "0";
  input = std::istringstream{"\xC0" + std::string("\x00", 1)};
  actual_output = parse_length_encoded_string(input);
  EXPECT_EQ(actual_output.size(), expected_output.size());
  EXPECT_EQ(actual_output, expected_output);

  // Test case: length encoding bits are 11 and the string an 8-bit integer with
  // value 1.
  expected_output = "1";
  input = std::istringstream{"\xC0\x01"};
  actual_output = parse_length_encoded_string(input);
  EXPECT_EQ(actual_output.size(), expected_output.size());
  EXPECT_EQ(actual_output, expected_output);

  // Test case: length encoding bits are 11 and the string an 8-bit integer with
  // value 255.
  expected_output = "255";
  input = std::istringstream{"\xC0\xFF"};
  actual_output = parse_length_encoded_string(input);
  EXPECT_EQ(actual_output.size(), expected_output.size());
  EXPECT_EQ(actual_output, expected_output);

  // Test case: length encoding bits are 11 and the string a 16-bit integer with
  // value 256.
  expected_output = "256";
  input = std::istringstream{"\xC1" + std::string(1, '\x00') + "\x01"};
  actual_output = parse_length_encoded_string(input);
  EXPECT_EQ(actual_output.size(), expected_output.size());
  EXPECT_EQ(actual_output, expected_output);

  // Test case: length encoding bits are 11 and the string a 16-bit integer with
  // value (2^16)-1 (65535).
  expected_output = "65535";
  input = std::istringstream{"\xC1\xFF\xFF"};
  actual_output = parse_length_encoded_string(input);
  EXPECT_EQ(actual_output.size(), expected_output.size());
  EXPECT_EQ(actual_output, expected_output);

  // Test case: length encoding bits are 11 and the string a 32-bit integer with
  // value 2^16 (65536).
  expected_output = "65536";
  input = std::istringstream{"\xC2" + std::string(2, '\x00') + "\x01" +
                             std::string("\x00", 1)};
  actual_output = parse_length_encoded_string(input);
  EXPECT_EQ(actual_output.size(), expected_output.size());
  EXPECT_EQ(actual_output, expected_output);

  // Test case: length encoding bits are 11 and the string a 32-bit integer with
  // value (2^32)-1.
  expected_output = std::to_string((static_cast<std::uint64_t>(1) << 32U) - 1);
  input = std::istringstream{"\xC2\xFF\xFF\xFF\xFF"};
  actual_output = parse_length_encoded_string(input);
  EXPECT_EQ(actual_output.size(), expected_output.size());
  EXPECT_EQ(actual_output, expected_output);
}

TEST(StorageTest, ReadRDB) {
  /*
  Read a hardcoded RDB file and expect that we parsed it correctly.
  This RDB file looks like this in hexdump:
    |REDIS0009..redis|
    |-ver.5.0.7..redi|
    |s-bits.@..ctime.|
    |u..f..used-mem..|
    |&....aof-preambl|
    |e.........mykey.|
    |myval...w-_.-||
  The header has version 9.
  The metadata has 5 key-value pairs.
  The database section has one key (mykey) and one value (myval).
  */
  std::vector<std::uint8_t> rdb_bytes = {
      0x52, 0x45, 0x44, 0x49, 0x53, 0x30, 0x30, 0x30, 0x39, 0xfa, 0x09,
      0x72, 0x65, 0x64, 0x69, 0x73, 0x2d, 0x76, 0x65, 0x72, 0x05, 0x35,
      0x2e, 0x30, 0x2e, 0x37, 0xfa, 0x0a, 0x72, 0x65, 0x64, 0x69, 0x73,
      0x2d, 0x62, 0x69, 0x74, 0x73, 0xc0, 0x40, 0xfa, 0x05, 0x63, 0x74,
      0x69, 0x6d, 0x65, 0xc2, 0x75, 0xd3, 0x92, 0x66, 0xfa, 0x08, 0x75,
      0x73, 0x65, 0x64, 0x2d, 0x6d, 0x65, 0x6d, 0xc2, 0xf8, 0x26, 0x0c,
      0x00, 0xfa, 0x0c, 0x61, 0x6f, 0x66, 0x2d, 0x70, 0x72, 0x65, 0x61,
      0x6d, 0x62, 0x6c, 0x65, 0xc0, 0x00, 0xfe, 0x00, 0xfb, 0x01, 0x00,
      0x00, 0x05, 0x6d, 0x79, 0x6b, 0x65, 0x79, 0x05, 0x6d, 0x79, 0x76,
      0x61, 0x6c, 0xff, 0xcc, 0xf7, 0x77, 0x2d, 0x5f, 0x89, 0x2d, 0x7c,
  };

  // NOTE: we have to create a std::string out of it because we want to be able
  // to explicitly give it the size, otherwise it treats the 0x00 bytes an null
  // char terminators and we end up with a shorter string than we should.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  const std::string rdb_string(reinterpret_cast<const char *>(rdb_bytes.data()),
                               rdb_bytes.size());
  std::istringstream input_stream{rdb_string};
  const auto rdb = read_rdb(input_stream);
  EXPECT_EQ(rdb.header.version, 9);
  EXPECT_EQ(rdb.metadata.redis_version, "5.0.7");
  EXPECT_EQ(rdb.metadata.redis_num_bits, NumBits::ARCHITECTURE_64_BITS);
  EXPECT_EQ(rdb.metadata.creation_time, 1720898421);
  EXPECT_EQ(rdb.metadata.used_memory, 796408);
  ASSERT_EQ(rdb.database_sections.size(), 1);
  ASSERT_EQ(rdb.database_sections.front().data.size(), 1);
  ASSERT_TRUE(rdb.database_sections.front().data.contains("mykey"));
  ASSERT_EQ(rdb.database_sections.front().data.at("mykey").first, "myval");
  EXPECT_EQ(rdb.eof.crc64,
            (std::array<std::uint8_t, 8>{0xcc, 0xf7, 0x77, 0x2d, 0x5f, 0x89,
                                         0x2d, 0x7c}));
}