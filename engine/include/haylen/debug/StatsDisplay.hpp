#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace haylen::core {
class Engine;
}

namespace haylen::debug {

// The on-screen debug statistics. Compact mode draws the frame rate, frame time, draw calls, vertices and instances in a corner of the safe area with the 2D renderer, so it works in every app. Full mode shows the debug overlay window instead.
class StatsDisplay final {
  public:
    enum class Mode : std::uint8_t {
        Off,
        Compact,
        Full,
    };

    struct Numbers {
        double fps = 0.0;
        double milliseconds = 0.0;
        std::size_t drawCalls = 0;
        std::size_t vertices = 0;
        std::size_t instances = 0;
    };

    // Resolves the names "off", "compact" and "full".
    [[nodiscard]] static std::optional<Mode> modeFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view modeName(Mode value) noexcept;

    // Draws the numbers in the bottom left corner of the safe area above everything else, sized in points so they read the same at any design resolution.
    static void drawCompact(core::Engine& engine, const Numbers& numbers);

  private:
    static constexpr float kTextSize = 13.0F;
    static constexpr float kPadding = 6.0F;
    static constexpr float kMargin = 8.0F;
};

} // namespace haylen::debug
