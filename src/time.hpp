#pragma once

// System includes.
#include <chrono>

template <typename TimeType>
std::chrono::steady_clock::time_point
unix_timestamp_to_steady_clock(std::size_t num_time_type_increments) {

  return std::chrono::steady_clock::now() +
         (std::chrono::system_clock::time_point(
              TimeType(num_time_type_increments)) -
          std::chrono::system_clock::now());
}