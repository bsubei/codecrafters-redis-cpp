#pragma once

// System includes.
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <istream>
#include <optional>
#include <unordered_map>
#include <vector>

// Our library's header includes.
#include "cache.hpp"

struct Config;

// RDB file format, see https://rdb.fnordig.de/file_format.html for details.
// Op codes
constexpr std::byte RDB_EOF{0xFF};
constexpr std::byte RDB_DB_SELECTOR{0xFE};
constexpr std::byte RDB_EXPIRE_TIME_S{0xFD};
constexpr std::byte RDB_EXPIRE_TIME_MS{0xFC};
constexpr std::byte RDB_RESIZE{0xFB};
constexpr std::byte RDB_AUX{0xFA}; // Auxiliary fields (AKA metadata fields).
constexpr auto RDB_MAGIC = "REDIS";
// If we detect the RDB file has a version lower than this, we bail.
constexpr auto MIN_SUPPORTED_RDB_VERSION = 7;
// The two most-significant bits encode the length.
constexpr std::byte LENGTH_ENCODING_MASK{0b11000000};

enum class NumBits : std::uint8_t {
  ARCHITECTURE_32_BITS,
  ARCHITECTURE_64_BITS,
};

struct Header {
  std::uint8_t version = 0;
};
struct Metadata {
  std::optional<std::uint64_t> creation_time{};
  std::optional<std::uint64_t> used_memory{}; // TODO units?
  std::optional<std::string> redis_version{};
  std::optional<NumBits> redis_num_bits{};
};
struct DatabaseSection {
  std::unordered_map<Cache::KeyT, Cache::EntryT> data{};
};
struct EndOfFile {
  std::array<std::uint8_t, 8> crc64{};
};

// An RDB file consists of these sections.
struct RDB {
  Header header{};
  Metadata metadata{};
  std::vector<DatabaseSection> database_sections{};
  EndOfFile eof{};
};

std::uint32_t parse_length_encoded_integer(std::istream &is);
std::string parse_length_encoded_string(std::istream &is);
RDB read_rdb(std::istream &is);
Cache load_cache(const Config &config);

// TODO We currently only support two kinds of string encodings:
// 1. Strings with a length prefix.
// 2. Special format "Integers as Strings", where you read 1, 2, or 4 bytes as
// an int, then make it into a string. See
// https://rdb.fnordig.de/file_format.html#string-encoding for details.
using LengthPrefixedString = std::uint32_t;
enum class IntAsString : std::uint8_t {
  ONE_BYTE,
  TWO_BYTES,
  FOUR_BYTES,
};
using StringEncoding = std::variant<LengthPrefixedString, IntAsString>;
StringEncoding parse_string_encoding(std::istream &is);

// helper type to create visitors for the StringEncoding variant.
template <class... Ts> struct StringEncodingVisitor : Ts... {
  using Ts::operator()...;
};