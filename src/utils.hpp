#pragma once

// System includes.
#include <algorithm>
#include <cctype>
#include <concepts>
#include <string>

template <typename T>
concept StringLike = requires(T str) {
  { str.begin() } -> std::input_iterator;
  { str.end() } -> std::input_iterator;
  { str.cbegin() } -> std::input_iterator;
  { str.cend() } -> std::input_iterator;
  { str.size() } -> std::convertible_to<std::size_t>;
  { std::tolower(*str.cbegin()) } -> std::same_as<int>;
};

template <StringLike StringType> std::string tolower(const StringType &str) {
  std::string lower{str.cbegin(), str.cend()};
  std::transform(lower.cbegin(), lower.cend(), lower.begin(),
                 [](auto character) { return std::tolower(character); });
  return lower;
}