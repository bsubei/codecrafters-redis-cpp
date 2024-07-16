// This source file's own header include.
#include "cache.hpp"

// System includes.
#include <algorithm>
#include <mutex>

// TODO clean up any expired cache elements we try to access so we don't waste
// time checking their expiry next time around.
// TODO eventually have the server actively go around testing for expired values
// and removing them (probably better to store the expiry values in a sorted
// way)
std::optional<std::string> Cache::get(const std::string &key) const {
  // Acquire a "shared" lock, so we only lock out writes to the cache.
  // Simultaneous reads don't need to wait.
  std::shared_lock lock(mutex);
  // If the data exists,
  if (data.contains(key)) {
    // and if the data is unexpired,
    if (!data.at(key).second.has_value() ||
        std::chrono::steady_clock::now() <= *data.at(key).second) {
      // Get the value for this key.
      return data.at(key).first;
    }
  }
  return std::nullopt;
}
void Cache::set(
    const std::string &key, const std::string &value,
    const std::optional<std::chrono::milliseconds> &expiry_duration) {
  ExpiryValueT expiry_time = std::nullopt;
  if (expiry_duration.has_value()) {
    // When expiry time is set manually, it's sent as a duration (as in, it
    // should expire after this much time from now).
    expiry_time = std::chrono::steady_clock::now() + expiry_duration.value();
  }
  // Acquire a unique lock, blocking out every other read/write, because we're
  // writing to the cache.
  std::unique_lock lock(mutex);
  data[key] = {value, expiry_time};
}

std::vector<std::string> Cache::keys() const {
  // Acquire a "shared" lock, so we only lock out writes to the cache.
  // Simultaneous reads don't need to wait.
  std::shared_lock lock(mutex);
  std::vector<std::string> keys{};
  keys.reserve(data.size());
  std::transform(data.cbegin(), data.cend(), std::back_inserter(keys),
                 [](const auto &cache_entry) { return cache_entry.first; });
  return keys;
}