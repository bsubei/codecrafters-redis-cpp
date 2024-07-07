// This source file's own header include.
#include "cache.hpp"

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
    if (!data_expiry.contains(key) ||
        std::chrono::steady_clock::now() <= data_expiry.at(key)) {
      // Get the value for this key.
      return data.at(key);
    }
  }
  return std::nullopt;
}
void Cache::set(
    const std::string &key, const std::string &value,
    const std::optional<std::chrono::milliseconds> &expiry_duration) {
  // Acquire a unique lock, blocking out every other read/write, because we're
  // writing to the cache.
  std::unique_lock lock(mutex);
  if (expiry_duration.has_value()) {
    data_expiry[key] =
        std::chrono::steady_clock::now() + expiry_duration.value();
  }
  data[key] = value;
}