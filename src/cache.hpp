#pragma once

// System includes.
#include <chrono>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

class Cache {
public:
  using ValueT = std::string;
  using ExpiryValueT = std::optional<std::chrono::steady_clock::time_point>;
  using EntryT = std::pair<ValueT, ExpiryValueT>;
  using KeyT = std::string;

private:
  // This mutex will protect the data map.
  mutable std::shared_mutex mutex{};
  std::unordered_map<KeyT, EntryT> data{};

public:
  Cache() = default;
  Cache(std::unordered_map<KeyT, EntryT> data_in) : data(std::move(data_in)) {}

  // TODO consider changing this to return a ref string for efficiency.
  std::optional<std::string> get(const std::string &key) const;
  void set(const std::string &key, const std::string &value,
           const std::optional<std::chrono::milliseconds> &expiry_duration =
               std::nullopt);
  std::vector<std::string> keys() const;
};