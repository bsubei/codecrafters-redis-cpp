// This source file's own header include.
#include "storage.hpp"

// System includes.
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>

// Our library's header includes.
#include "config.hpp"

namespace {
// void read_rdb_header(std::istringstream &is) { is.get(); }
} // namespace

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