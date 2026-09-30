#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace haylen::audio {

// How the app shares sound with the system and other apps, which `app.json` sets in its `audio` object. The category is the audio session category of iOS and tvOS: ambient follows the silent switch and mixes with the audio of other apps, solo ambient follows the silent switch and silences other apps, and playback keeps playing with the silent switch on and silences other apps unless it mixes with them.
struct Session {
    enum class Category : std::uint8_t {
        Ambient,
        SoloAmbient,
        Playback,
    };

    Category category = Category::Ambient;

    // Lets the playback category play along with the audio of other apps. Ambient always mixes and solo ambient never does, so only playback accepts it.
    bool mixWithOthers = false;

    [[nodiscard]] static std::optional<Category> categoryFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view categoryName(Category value) noexcept;

  private:
    static const std::array<std::pair<std::string_view, Category>, 3> kCategoryNames;
};

} // namespace haylen::audio
