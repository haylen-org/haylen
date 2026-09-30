#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"

namespace haylen::storage {

class UserStorage;

// Named save slots kept as JSON files in user storage. Each file holds the saved data and a small summary, such as a title and a progress label, for load menus. Slot names use letters, digits, dashes and underscores. Like other files of user storage, writes and removes become durable with `UserStorage::flush`, and every method may run on any thread.
class SaveSlots final {
  public:
    struct Info {
        std::string slot;
        std::int64_t savedAt = 0;
        core::Json summary;
    };

    // Returns the current time in Unix seconds. Without one, saves are stamped with the system clock.
    using Clock = std::function<std::int64_t()>;

    explicit SaveSlots(UserStorage& storage, std::string folder = "saves", Clock now = {});

    void write(std::string_view slot, const core::Json& data, const core::Json& summary = core::Json::object());
    [[nodiscard]] std::optional<core::Json> read(std::string_view slot) const;
    [[nodiscard]] std::optional<Info> getInfo(std::string_view slot) const;
    [[nodiscard]] bool exists(std::string_view slot) const;
    bool remove(std::string_view slot);

    // Lists every slot, newest first.
    [[nodiscard]] std::vector<Info> list() const;

  private:
    static constexpr std::string_view kExtension = ".json";

    [[nodiscard]] static bool isValidSlot(std::string_view slot) noexcept;
    [[nodiscard]] static std::int64_t unixSeconds();
    [[nodiscard]] std::string pathOf(std::string_view slot) const;
    [[nodiscard]] std::optional<core::Json> document(std::string_view slot) const;

    UserStorage& userStorage;
    std::string directory;
    Clock clock;
};

} // namespace haylen::storage
