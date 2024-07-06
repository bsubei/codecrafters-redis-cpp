#pragma once

#include <optional>
#include <string>

struct Config
{
    std::optional<std::string> dir{};
    std::optional<std::string> dbfilename{};
};
