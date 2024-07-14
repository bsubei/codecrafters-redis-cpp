// This source file's own header include.
#include "storage.hpp"

// System includes.
#include <cstddef>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>

// Our library's header includes.
#include "config.hpp"

namespace {

std::ifstream read_file(const std::filesystem::path &filepath) {
  std::ifstream file_contents(filepath, std::ios::binary);
  if (!file_contents) {
    std::cerr << "Failed to read file contents at: " << filepath << std::endl;
    std::terminate();
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
    std::terminate();
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

} // namespace

// TODO we only support string encodings, see
// https://rdb.fnordig.de/file_format.html#string-encoding
std::string parse_length_encoded_string(std::istream &is) {
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
    return read_string_n_bytes(is, length);
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
    return read_string_n_bytes(is, length);
  }
  // Discard the remaining 6 bits. The next 4 bytes represent the length. This
  // covers lengths from 16384 to (2^32)-1.
  case 0b10: {
    const auto length = read_int_n_bytes<4>(is);
    return read_string_n_bytes(is, length);
  }
  // Special format. We only support "Integers as Strings". Expect 0, 1, or 2
  // in the remaining 6 bits.
  case 0b11: {
    std::byte string_encoding_bits = length_byte & std::byte{0x3F};
    switch (std::to_integer<std::uint8_t>(string_encoding_bits)) {
    // An 8 bit integer follows.
    case 0:
      // Read the next 1 byte as an integer, and convert to a string.
      return std::to_string(read_int_n_bytes<1>(is));
    // A 16 bit integer follows.
    case 1:
      // Same but two bytes instead of one.
      return std::to_string(read_int_n_bytes<2>(is));
    // A 32 bit integer follows.
    case 2:
      // Same but four bytes instead of one.
      return std::to_string(read_int_n_bytes<4>(is));
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
RDB read_rdb(std::istream &is) {
  // TODO currently we don't do anything with the version besides check if
  // it's too old and print it out.
  auto header = read_rdb_header(is);
  std::cout << "HEADER VERSION: " << std::to_string(header.version)
            << std::endl;

  auto metadata = read_rdb_metadata(is);
  std::cout << "METADATA: " << std::endl;
  if (metadata.creation_time) {
    std::cout << "\tcreation_time: " << *metadata.creation_time << std::endl;
  }
  if (metadata.used_memory) {
    std::cout << "\tused_memory: " << *metadata.used_memory << std::endl;
  }
  if (metadata.redis_version) {
    std::cout << "\tredis_version: " << *metadata.redis_version << std::endl;
  }
  if (metadata.redis_num_bits) {
    std::cout << "\tredis_num_bits: "
              << std::to_string(
                     static_cast<std::uint8_t>(*metadata.redis_num_bits))
              << std::endl;
  }
  // TODO Parse the database sections

  // TODO Parse the EndOfFile section

  return RDB{.header = header,
             .metadata = metadata,
             .database_sections = {},
             .eof = {}};
}

Cache load_cache(const Config &config) {
  if (config.dbfilename && config.dir) {
    const auto filepath = std::filesystem::path(*config.dir) /
                          std::filesystem::path(*config.dbfilename);
    std::cout << "Reading RDB from file: " << filepath << std::endl;
    auto file_contents = read_file(filepath);
    auto rdb = read_rdb(file_contents);
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