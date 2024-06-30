#include "Cache.hpp"

std::optional<std::string> Cache::get(const std::string &key) const
{
    // Acquire a "shared" lock, so we only lock out writes to the cache. Simultaneous reads don't need to wait.
    std::shared_lock lock(mutex);
    // If the data exists,
    if (data.contains(key))
    {
        // and if the data is unexpired,
        if (!data_expiry.contains(key) || std::chrono::steady_clock::now() <= data_expiry.at(key))
        {
            // Get the value for this key.
            return data.at(key);
        }
    }
    return std::nullopt;
}
void Cache::set(const std::string &key, const std::string &value, const std::optional<std::chrono::milliseconds> &expiry_duration)
{
    // Acquire a unique lock, blocking out every other read/write, because we're writing to the cache.
    std::unique_lock lock(mutex);
    if (expiry_duration.has_value())
    {
        data_expiry[key] = std::chrono::steady_clock::now() + expiry_duration.value();
    }
    data[key] = value;
}