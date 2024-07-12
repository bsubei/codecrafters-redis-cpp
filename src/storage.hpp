#pragma once

// Our library's header includes.
#include "cache.hpp"

struct Config;

Cache read_cache_from_rdb(const Config &config);