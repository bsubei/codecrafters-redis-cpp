#include <shared_mutex>
#include <mutex>
#include <unordered_map>
#include <string>
#include <optional>

class Cache
{
private:
    std::unordered_map<std::string, std::string> data{};
    mutable std::shared_mutex mutex{};

public:
    std::optional<std::string> get(const std::string &key) const;
    void set(const std::string &key, const std::string &value);
};