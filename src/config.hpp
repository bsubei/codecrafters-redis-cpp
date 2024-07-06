#pragma once

// System includes.
#include <optional>
#include <string>

// TODO merge this and the cache to be part of the Server state
struct Config
{
    std::optional<std::string> dir{};
    std::optional<std::string> dbfilename{};
};
