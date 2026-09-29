#pragma once

#include <any>
#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace haylen::ai {

// Named values that the nodes of a behavior tree share, such as the current target or the last place the player was seen. Values keep their C++ type, and reading one with another type finds nothing.
class Blackboard final {
  public:
    template <typename T> void set(std::string key, T value) {
        values.insert_or_assign(std::move(key), std::any(std::move(value)));
    }

    // Returns the value, or null when the key is missing or holds another type.
    template <typename T> [[nodiscard]] const T* get(std::string_view key) const {
        const auto found = values.find(key);
        return found == values.end() ? nullptr : std::any_cast<T>(&found->second);
    }
    template <typename T> [[nodiscard]] T* get(std::string_view key) {
        const auto found = values.find(key);
        return found == values.end() ? nullptr : std::any_cast<T>(&found->second);
    }

    [[nodiscard]] bool has(std::string_view key) const {
        return values.find(key) != values.end();
    }
    void erase(std::string_view key) {
        const auto found = values.find(key);
        if (found != values.end()) {
            values.erase(found);
        }
    }
    void clear() noexcept {
        values.clear();
    }
    [[nodiscard]] std::size_t size() const noexcept {
        return values.size();
    }

  private:
    std::map<std::string, std::any, std::less<>> values;
};

} // namespace haylen::ai
