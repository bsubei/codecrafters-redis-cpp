#pragma once

// System includes.
#include <array>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

// Our library's header includes.
#include "cache.hpp"

struct Config;

Cache read_cache_from_rdb(const Config &config);

// RDB file format, see https://rdb.fnordig.de/file_format.html for details.
// Op codes
constexpr uint8_t RDB_EOF = 0xFF;
constexpr uint8_t RDB_DB_SELECTOR = 0xFE;
constexpr uint8_t RDB_EXPIRE_TIME_S = 0xFD;
constexpr uint8_t RDB_EXPIRE_TIME_MS = 0xFC;
constexpr uint8_t RDB_AUX = 0xFA; // Auxiliary fields (AKA metadata fields).
// TODO we only support version 7 exactly.
constexpr auto RDB_MAGIC = "REDIS0007";

enum class NumBits {
  ARCHITECTURE_32_BITS,
  ARCHITECTURE_64_BITS,
};

struct Header {
  std::uint8_t version = 0;
};
struct Metadata {
  std::optional<std::uint64_t> creation_time{};
  std::optional<std::uint64_t> used_memory{}; // TODO units?
  std::optional<std::uint8_t> redis_version{};
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