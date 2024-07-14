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
  std::istringstream input{std::string{"\x00", 1}};
  std::string expected_output = "";
  EXPECT_EQ(parse_length_encoded_string(input), expected_output);

  // Test case: length encoding bits are 00 and length is one.
  input = std::istringstream{"\x01S"};
  expected_output = "S";
  EXPECT_EQ(parse_length_encoded_string(input), expected_output);

  // Test case: length encoding bits are 00 and length is 12.
  input = std::istringstream{"\x0Cqwertydvorak"};
  expected_output = "qwertydvorak";
  EXPECT_EQ(parse_length_encoded_string(input), expected_output);

  // Test case: length encoding bits are 00 and length is 63 (the edge
  // case).
  expected_output = get_random_string_n_bytes(63);
  input = std::istringstream{"\x3F" + expected_output};
  EXPECT_EQ(parse_length_encoded_string(input), expected_output);

  // Test case: length encoding bits are 01 and length is 64 (edge case).
  expected_output = get_random_string_n_bytes(64);
  input = std::istringstream{"\x40\x40" + expected_output};
  EXPECT_EQ(parse_length_encoded_string(input), expected_output);

  // Test case: length encoding bits are 01 and length is 700.
  expected_output = get_random_string_n_bytes(700);
  input = std::istringstream{"\x42\xBC" + expected_output};
  EXPECT_EQ(parse_length_encoded_string(input), expected_output);
}