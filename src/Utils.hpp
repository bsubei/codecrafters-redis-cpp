#pragma once

#include <string>
#include <algorithm>
#include <concepts>
#include <cctype>

template <typename T>
concept StringLike = requires(T a) {
    { a.begin() } -> std::input_iterator;
    { a.end() } -> std::input_iterator;
    { a.cbegin() } -> std::input_iterator;
    { a.cend() } -> std::input_iterator;
    { a.size() } -> std::convertible_to<std::size_t>;
    { std::tolower(*a.cbegin()) } -> std::same_as<int>;
};

template <StringLike StringType>
std::string tolower(const StringType &s)
{
    std::string lower{s.cbegin(), s.cend()};
    std::transform(lower.cbegin(), lower.cend(), lower.begin(), [](auto c)
                   { return std::tolower(c); });
    return lower;
}