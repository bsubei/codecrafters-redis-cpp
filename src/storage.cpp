// This source file's own header include.
#include "storage.hpp"

// System includes.
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>

// Our library's header includes.
#include "config.hpp"

namespace {

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

} // namespace

// TODO we only support string encodings, see
// https://rdb.fnordig.de/file_format.html#string-encoding
std::string parse_length_encoded_string(std::istream &is) {
  // Read the first byte, and use that to discover what encoding we need to use.
  int l = is.get();
  if (!is.good()) {
    std::cerr << "Unable to read length byte" << std::endl;
    std::terminate();
  }
  std::byte length_byte = std::byte(static_cast<std::uint8_t>(l));

  std::cout << "READING LENGTH BYTE: " << std::setfill('0') << std::setw(2)
            << std::setbase(16) << l << std::endl;

  const std::byte length_encoding_bits =
      (length_byte & LENGTH_ENCODING_MASK) >> 6;
  switch (std::to_integer<std::uint8_t>(length_encoding_bits)) {
  // The remaining 6 bits represent the length of the string. This covers
  // lengths from 0 to 63.
  case 0b00: {
    const std::uint8_t length =
        std::to_integer<std::uint8_t>(length_byte & (~LENGTH_ENCODING_MASK));
    std::cout << "TURNS OUT LENGTH IS: " << std::to_string(length) << std::endl;
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
    // The most significant byte was the first byte (because the data is in
    // little endian order), and the least significant byte was the second byte
    // we read.
    const auto most_significant_byte =
        std::to_integer<std::uint16_t>(length_byte & ~LENGTH_ENCODING_MASK);
    std::uint16_t length =
        (most_significant_byte << 8) | least_significant_byte;
    std::cout << "TURNS OUT LENGTH IS: " << std::to_string(length) << std::endl;
    return read_string_n_bytes(is, length);
  }
  // Discard the remaining 6 bits. The next 4 bytes represent the length. This
  // covers lengths from 16384 to (2^32)-1.
  case 0b10: {
    // NOTE: the next 4 bytes come in as little endian so we have to switch
    // their order if our system uses big endian.
    std::array<std::byte, 4> length_bytes{};
    is.read(reinterpret_cast<char *>(length_bytes.data()), 4);
    if (!is.good()) {
      std::cerr << "Unable to read 4 length bytes" << std::endl;
      std::terminate();
    }
    auto length = std::bit_cast<std::uint32_t>(length_bytes);
    // If we are big endian, we need to swap the bytes from little endian to big
    // endian.
    if constexpr (std::endian::native == std::endian::big) {
      length = std::byteswap(length);
    }
    return read_string_n_bytes(is, length);
  }
  // Special format. We only support "Integers as Strings". Expect 0, 1, or 2
  // in the remaining 6 bits.
  case 0b11: {
    std::byte string_encoding_bits = length_encoding_bits & std::byte{0xFC};
    // TODO finish
    switch (std::to_integer<std::uint8_t>(string_encoding_bits)) {
    // An 8 bit integer follows.
    case 0:
    // A 16 bit integer follows.
    case 1:
    // A 32 bit integer follows.
    case 2:
    default:
      std::cerr << "Encountered unsupported string length encoding: "
                << std::setbase(16)
                << (std::to_integer<std::uint8_t>(string_encoding_bits))
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

Cache read_cache_from_rdb(const Config &config) {
  if (config.dbfilename && config.dir) {
    const auto filepath = std::filesystem::path(*config.dir) /
                          std::filesystem::path(*config.dbfilename);
    std::cout << "Reading RDB from file: " << filepath << std::endl;

    std::ifstream file_contents(filepath, std::ios::binary);
    if (!file_contents) {
      std::cerr << "Failed to read file contents at: " << filepath << std::endl;
      std::terminate();
    }

    // TODO Parse the file contents (only the key-value cache data for now).
  }

  return {};
};