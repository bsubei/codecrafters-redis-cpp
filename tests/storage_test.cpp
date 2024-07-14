#include <gtest/gtest.h>

#include <algorithm>
#include <istream>
#include <ostream>
#include <random>
#include <string>

#include "../src/storage.hpp"

namespace {
std::string get_random_string_n_bytes(const std::size_t length) {
  // These are declared thread_local so we don't needlessly create them for
  // every function call, and at the same time preserving thread safety.
  // Use a fixed seed so our tests are deterministic.
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
  std::string expected_output = "";
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
                             std::string("\x00", 2) + expected_output};
  actual_output = parse_length_encoded_string(input);
  EXPECT_EQ(actual_output.size(), expected_output.size());
  EXPECT_EQ(actual_output, expected_output);

  // Test case: length encoding bits are 10 and length is 17000.
  // The first byte "\x80" has length encoding bits 10 (the rest of the byte is
  // discarded). The next four bytes come in little-endian and make up the
  // length 17000 (which is the same as the swapped bytes: 0x00004268).
  expected_output = get_random_string_n_bytes(17000);
  input = std::istringstream{"\x80\x68\x42" + std::string("\x00", 2) +
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
  input = std::istringstream{"\xC1" + std::string("\x00", 1) + "\x01"};
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
  input = std::istringstream{"\xC2" + std::string("\x00", 2) + "\x01" +
                             std::string("\x00", 1)};
  actual_output = parse_length_encoded_string(input);
  EXPECT_EQ(actual_output.size(), expected_output.size());
  EXPECT_EQ(actual_output, expected_output);

  // Test case: length encoding bits are 11 and the string a 32-bit integer with
  // value (2^32)-1.
  expected_output = std::to_string((static_cast<std::uint64_t>(1) << 32) - 1);
  input = std::istringstream{"\xC2\xFF\xFF\xFF\xFF"};
  actual_output = parse_length_encoded_string(input);
  EXPECT_EQ(actual_output.size(), expected_output.size());
  EXPECT_EQ(actual_output, expected_output);
}

TEST(StorageTest, ReadRDB) {
  // TODO have a hardcoded RDB file as a constant here and load it into the
  // istream.

  std::istringstream is{""};

  // auto rdb = read_rdb(is);
  // TODO test we can read and parse the RDB.
  // EXPECT_EQ()
}