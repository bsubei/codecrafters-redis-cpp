// This source file's own header include.
#include "storage.hpp"

// System includes.
#include <cassert>
#include <cstddef>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <utility>

// Our library's header includes.
#include "config.hpp"

namespace {

std::optional<std::ifstream> read_file(const std::filesystem::path &filepath) {
  std::ifstream file_contents(filepath, std::ios::binary);
  if (!file_contents) {
    std::cerr << "Failed to read file contents at: " << filepath << std::endl;
    return std::nullopt;
  }
  return file_contents;
}

std::string read_string_n_bytes(std::istream &is, const std::size_t num_bytes) {
  // Read the rest of the string and return it.
  std::string buf{};
  buf.resize(num_bytes);
  is.read(&buf[0], num_bytes);
  if (!is.good()) {
    std::cerr << "Unable to read string with " << num_bytes << " bytes"
              << std::endl;
    std::terminate();
  }
  return buf;
}

// Copy over all the bytes and return an appropriately-sized int out of them.
template <std::size_t num_bytes>
decltype(auto) bytes_to_int(const std::array<std::byte, num_bytes> &bytes) {
  // Use the lambda to declare the result with different types so we can use it
  // outside the constexpr if blocks.
  constexpr auto lambda = []() {
    if constexpr (num_bytes == 1) {
      return static_cast<std::uint8_t>(0);
    } else if constexpr (num_bytes == 2) {
      return static_cast<std::uint16_t>(0);
    } else if constexpr (num_bytes == 4) {
      return static_cast<std::uint32_t>(0);
    } else if constexpr (num_bytes == 8) {
      return static_cast<std::uint64_t>(0);
    } else {
      static_assert(false && "Unsupported num_bytes was given");
    }
  };

  auto result = lambda();
  std::memcpy(&result, bytes.data(), num_bytes);
  // If we are big endian, we need to swap the bytes from little endian to big
  // endian.
  if constexpr (std::endian::native == std::endian::big) {
    result = std::byteswap(result);
  }
  return result;
}

template <std::size_t num_bytes>
decltype(auto) read_int_n_bytes(std::istream &is) {
  std::array<std::byte, num_bytes> buf{};
  is.read(reinterpret_cast<char *>(buf.data()), num_bytes);
  if (!is.good()) {
    std::cerr << "Unable to read int with " << num_bytes << " bytes"
              << std::endl;
    std::terminate();
  }
  return bytes_to_int<num_bytes>(buf);
}

// If the next byte is the given opcode, consume it and return true. Otherwise,
// just return false.
bool is_opcode_section(const std::byte opcode, std::istream &is) {
  if (std::byte(is.peek()) == opcode) {
    // Consume the opcode byte.
    is.get();
    if (!is.good()) {
      std::cerr << "Unable to read opcode section: " << std::setbase(16)
                << std::to_integer<std::uint8_t>(opcode) << std::endl;
      std::terminate();
    }
    return true;
  }
  return false;
}

Header read_rdb_header(std::istream &is) {
  std::string buf{};
  buf.resize(5);
  is.read(buf.data(), 5);
  if (!is.good() || buf != RDB_MAGIC) {
    std::cerr << "Unable to read magic '" << RDB_MAGIC << "' in header"
              << std::endl;
    std::terminate();
  }

  buf.clear();
  buf.resize(4);
  is.read(buf.data(), 4);
  if (!is.good()) {
    std::cerr << "Unable to read RDB version in header" << std::endl;
    std::terminate();
  }

  std::uint8_t version = std::stoul(buf);
  if (version < MIN_SUPPORTED_RDB_VERSION) {
    std::cerr << "RDB version is too old: " << std::to_string(version)
              << std::endl;
  }

  return Header{.version = version};
}

Metadata read_rdb_metadata(std::istream &is) {
  Metadata metadata{};
  // Keep reading key-value pairs until we no longer see the Metadata opcode.
  while (is_opcode_section(RDB_AUX, is)) {
    // Read a string-encoded key.
    const std::string key = parse_length_encoded_string(is);
    // Read a string-encoded value.
    const std::string value = parse_length_encoded_string(is);
    // Manually set the metadata fields.
    if (key == "ctime") {
      metadata.creation_time = std::stoul(value);
    } else if (key == "used-mem") {
      metadata.used_memory = std::stoul(value);
    } else if (key == "redis-bits") {
      auto num_bits = std::stoul(value);
      if (num_bits == 32) {
        metadata.redis_num_bits = NumBits::ARCHITECTURE_32_BITS;
      } else if (num_bits == 64) {
        metadata.redis_num_bits = NumBits::ARCHITECTURE_64_BITS;
      } else {
        std::cerr << "Encountered unsupported number of bits in metadata: "
                  << std::to_string(num_bits) << std::endl;
        std::terminate();
      }
    } else if (key == "redis-ver") {
      metadata.redis_version = value;
    } else {
      std::cout << "Encountered unknown Metadata key and value pair: (" << key
                << ", " << value << ")" << std::endl;
    }
  }

  return metadata;
}
std::vector<DatabaseSection> read_rdb_database_sections(std::istream &is) {
  std::vector<DatabaseSection> db_sections{};
  // Read each database section
  while (is_opcode_section(RDB_DB_SELECTOR, is)) {
    DatabaseSection db_section{};
    // Double check that the db number it's saying we're in is the correct one.
    const std::uint8_t db_number = is.get();
    if (!is.good()) {
      std::cerr << "Unable to read DB number" << std::endl;
      std::terminate();
    }
    if (db_number != db_sections.size()) {
      std::cerr << "Invalid db number encountered: "
                << std::to_string(db_number) << std::endl;
      std::terminate();
    }
    // Expect the hash table size section and read the sizes.
    if (!is_opcode_section(RDB_RESIZE, is)) {
      std::cerr << "Expected RDB_RESIZE opcode" << std::endl;
      std::terminate();
    }
    const std::uint32_t num_key_value_pairs = parse_length_encoded_integer(is);
    const std::uint32_t num_expiry_pairs = parse_length_encoded_integer(is);
    std::uint32_t num_expiry_so_far = 0;
    // Now read that many key-value pairs.
    for (std::uint32_t i = 0; i < num_key_value_pairs; ++i) {
      Cache::ExpiryValueT expiry{std::nullopt};
      // Check for a possible expiry prefix.
      if (is_opcode_section(RDB_EXPIRE_TIME_S, is)) {
        expiry.emplace(std::chrono::seconds(read_int_n_bytes<4>(is)));
        ++num_expiry_so_far;
      } else if (is_opcode_section(RDB_EXPIRE_TIME_MS, is)) {
        expiry.emplace(std::chrono::milliseconds(read_int_n_bytes<8>(is)));
        ++num_expiry_so_far;
      }
      // Read the value type.
      auto value_type = read_int_n_bytes<1>(is);
      // Read the string-encoded key.
      std::string key = parse_length_encoded_string(is);
      // TODO we currently only support the "string encoding" value type.
      if (value_type == 0) {
        // Read value encoded as string.
        std::string value = parse_length_encoded_string(is);
        // Finally, we can populate this key-value pair possibly with an expiry.
        db_section.data.emplace(key, Cache::EntryT{value, expiry});

      } else {
        std::cerr << "Got unsupported value type: "
                  << std::to_string(value_type) << std::endl;
        std::terminate();
      }
    }
    // When done reading all the key-value pairs, save this DB section and move
    // on to the next one.
    db_sections.push_back(db_section);

    // Double-check that the promised number of expiry pairs were seen.
    assert(num_expiry_so_far == num_expiry_pairs &&
           "Mismatching num expiry pairs");
  }
  return db_sections;
}
EndOfFile read_rdb_eof_section(std::istream &is) {
  if (is_opcode_section(RDB_EOF, is)) {
    // TODO compute the actual CRC of the entire stream from start to this point
    // so we can compare it. Read the recorded CRC.
    std::array<std::uint8_t, 8> buf{};
    is.read(reinterpret_cast<char *>(buf.data()), 8);
    if (!is.good()) {
      std::cerr << "Unable to read CRC 8 bytes" << std::endl;
      std::terminate();
    }
    return EndOfFile{.crc64 = buf};
  }
  std::cerr << "Unable to read End of File section" << std::endl;
  std::terminate();
}

} // namespace

// Parse only the bytes needed to determine the encoding and return the
// encoding.
StringEncoding parse_string_encoding(std::istream &is) {
  // Read the first byte, and use that to discover what encoding we need to use.
  std::uint8_t l = is.get();
  if (!is.good()) {
    std::cerr << "Unable to read length byte" << std::endl;
    std::terminate();
  }
  std::byte length_byte = std::byte(l);

  const std::byte length_encoding_bits =
      (length_byte & LENGTH_ENCODING_MASK) >> 6;
  switch (std::to_integer<std::uint8_t>(length_encoding_bits)) {
  // The remaining 6 bits represent the length of the string. This covers
  // lengths from 0 to 63.
  case 0b00: {
    const std::uint8_t length =
        std::to_integer<std::uint8_t>(length_byte & (~LENGTH_ENCODING_MASK));
    return LengthPrefixedString{length};
  }
  // Read one additional byte. The combined 14 bits represent the length. This
  // covers lengths from 64 to 16383.
  case 0b01: {
    const std::uint16_t least_significant_byte = is.get();
    if (!is.good()) {
      std::cerr << "Unable to read second length byte" << std::endl;
      std::terminate();
    }
    // The most significant byte was the first byte we read, and the least
    // significant byte was the second byte we read, because the data arrived in
    // little endian order.
    const auto most_significant_byte =
        std::to_integer<std::uint16_t>(length_byte & ~LENGTH_ENCODING_MASK);
    std::uint16_t length =
        (most_significant_byte << 8) | least_significant_byte;
    return LengthPrefixedString{length};
  }
  // Discard the remaining 6 bits. The next 4 bytes represent the length. This
  // covers lengths from 16384 to (2^32)-1.
  case 0b10: {
    const auto length = read_int_n_bytes<4>(is);
    return LengthPrefixedString{length};
  }
  // Special format. We only support "Integers as Strings". Expect 0, 1, or 2
  // in the remaining 6 bits.
  case 0b11: {
    std::byte string_encoding_bits = length_byte & std::byte{0x3F};
    switch (std::to_integer<std::uint8_t>(string_encoding_bits)) {
    // An 8 bit integer follows.
    case 0:
      return IntAsString::ONE_BYTE;
    // A 16 bit integer follows.
    case 1:
      return IntAsString::TWO_BYTES;
    // A 32 bit integer follows.
    case 2:
      return IntAsString::FOUR_BYTES;
    default:
      std::cerr << "Encountered unsupported string length encoding: "
                << std::setbase(16)
                << std::to_integer<std::uint8_t>(string_encoding_bits)
                << std::endl;
      std::terminate();
    }
  }
  default:
    std::cerr << "Encountered unknown length encoding: " << std::setbase(16)
              << std::to_integer<std::uint8_t>(length_encoding_bits)
              << std::endl;
    std::terminate();
  }
  return {};
}
std::uint32_t parse_length_encoded_integer(std::istream &is) {
  // Parsing a length-encoded integer is just like that of a string, except the
  // length of the string ends up being the integer we want, so we can just
  // return that.
  const auto encoding = parse_string_encoding(is);
  if (!std::holds_alternative<LengthPrefixedString>(encoding)) {
    std::cerr
        << "Cannot parse integer using encoding other than length prefixed"
        << std::endl;
    std::terminate();
  }
  return std::get<LengthPrefixedString>(encoding);
}
std::string parse_length_encoded_string(std::istream &is) {
  // Determine the kind of encoding (includes how many bytes to read).
  const auto string_encoding = parse_string_encoding(is);
  // Now read that many bytes depending on the encoding.
  return std::visit(StringEncodingVisitor{
                        [&is](const LengthPrefixedString &l) {
                          return read_string_n_bytes(is, l);
                        },
                        [&is](const IntAsString &o) {
                          // Read the next N bytes as an integer, and convert to
                          // a string.
                          if (o == IntAsString::ONE_BYTE) {
                            return std::to_string(read_int_n_bytes<1>(is));
                          } else if (o == IntAsString::TWO_BYTES) {
                            return std::to_string(read_int_n_bytes<2>(is));
                          } else if (o == IntAsString::FOUR_BYTES) {
                            return std::to_string(read_int_n_bytes<4>(is));
                          } else {
                            std::cerr
                                << "Unknown IntAsString enum: "
                                << std::to_string(static_cast<std::uint8_t>(o))
                                << std::endl;
                            std::terminate();
                          }
                        },
                        [](const auto &) {
                          std::cerr << "Unknown StringEncoding variant"
                                    << std::endl;
                          std::terminate();
                        },
                    },
                    string_encoding);
}
RDB read_rdb(std::istream &is) {
  // Read these sections in this particular sequence.
  auto header = read_rdb_header(is);
  auto metadata = read_rdb_metadata(is);
  auto db_sections = read_rdb_database_sections(is);
  auto eof_section = read_rdb_eof_section(is);
  return RDB{.header = header,
             .metadata = metadata,
             .database_sections = db_sections,
             .eof = eof_section};
}

Cache load_cache(const Config &config) {
  if (config.dbfilename && config.dir) {
    const auto filepath = std::filesystem::path(*config.dir) /
                          std::filesystem::path(*config.dbfilename);
    std::cout << "Reading RDB from file: " << filepath << std::endl;
    auto file_contents = read_file(filepath);
    if (!file_contents) {
      return Cache{};
    }
    auto rdb = read_rdb(*file_contents);
    if (rdb.database_sections.size() == 0) {
      std::cerr
          << "Expected to find at least one Database section in RDB file: "
          << filepath << std::endl;
      std::terminate();
    } else {
      if (rdb.database_sections.size() > 1) {
        std::cout << "Found more than one database sections: "
                  << rdb.database_sections.size() << std::endl;
      }
      return Cache(rdb.database_sections.front().data);
    }
  }
  return Cache{};
};