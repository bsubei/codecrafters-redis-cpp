#include <shared_mutex>
#include <mutex>
#include <unordered_map>
#include <string>
#include <optional>
#include <chrono>

class Cache
{
private:
    // This mutex will protect both the data and data_expiry members.
    mutable std::shared_mutex mutex{};
    std::unordered_map<std::string, std::string> data{};
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> data_expiry{};

public:
    std::optional<std::string> get(const std::string &key) const;
    void set(const std::string &key, const std::string &value, const std::optional<std::chrono::milliseconds> &expiry_duration = std::nullopt);
};