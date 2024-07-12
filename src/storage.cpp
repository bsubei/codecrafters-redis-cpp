// This source file's own header include.
#include "storage.hpp"

// System includes.
#include <iostream>

// Our library's header includes.
#include "config.hpp"

Cache read_cache_from_rdb(const Config &config) {
  if (config.dbfilename && config.dir) {
    std::cout << "Reading RDB from file: " << *config.dir << "/"
              << *config.dbfilename << std::endl;
    // TODO Read the file contents.

    // TODO Parse the file contents (only the key-value cache data for now).
  }

  return {};
};