#include "Cache.hpp"

std::optional<std::string> Cache::get(const std::string &key) const
{
    // Acquire a "shared" lock, so we only lock out writes to the cache. Simultaneous reads don't need to wait.
    std::shared_lock lock(mutex);
    if (data.contains(key))
    {
        return data.at(key);
    }
    return std::nullopt;
}
void Cache::set(const std::string &key, const std::string &value)
{
    // Acquire a unique lock, blocking out every other read/write, because we're writing to the cache.
    std::unique_lock lock(mutex);
    data[key] = value;
}