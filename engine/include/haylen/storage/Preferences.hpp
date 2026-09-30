#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"

namespace haylen::storage {

class UserStorage;

// App preferences kept as one JSON object in user storage. Keys are dotted paths into nested objects, such as `audio.volume.music`, and changes reach the disk on save. Setting a key to `null` removes it.
class Preferences final {
  public:
    explicit Preferences(UserStorage& storage, std::string file = "preferences.json");

    // Replaces the values in memory with the stored file, or with nothing when there is no file yet.
    void load();
    void save();

    [[nodiscard]] bool has(std::string_view key) const;
    [[nodiscard]] core::Json get(std::string_view key, const core::Json& defaultValue = nullptr) const;
    void set(std::string_view key, core::Json value);
    bool remove(std::string_view key);
    void clear();

    [[nodiscard]] bool isDirty() const noexcept {
        return dirty;
    }
    [[nodiscard]] const core::Json& getValues() const noexcept {
        return values;
    }

  private:
    [[nodiscard]] static std::vector<std::string> splitKey(std::string_view key);
    [[nodiscard]] const core::Json* find(std::string_view key) const;

    UserStorage& userStorage;
    std::string path;
    core::Json values = core::Json::object();
    bool dirty = false;
};

} // namespace haylen::storage
