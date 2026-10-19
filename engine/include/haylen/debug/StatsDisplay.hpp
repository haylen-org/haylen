#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/2d/graphics/Renderer.hpp"

namespace haylen::core {
class Engine;
}

namespace haylen::debug {

// The on-screen debug statistics. Compact mode draws a line for each item it shows, the frame rate, draw calls and geometry unless the app picks others, in a corner of the safe area with the 2D renderer, so it works in every app. Full mode shows the debug overlay window instead, where the player picks the items. The display keeps its lines from one frame to the next and reads its numbers only when its text changes, so a frame that shows the same text allocates nothing.
class StatsDisplay final {
  public:
    enum class Mode : std::uint8_t {
        Off,
        Compact,
        Full,
    };

    // The lines of the compact mode: the frame rate and frame time, the draw calls and passes, the vertices and instances, the memory of Lua and textures, the scenes, tweens and timers, the physics bodies and contacts, the particles and the voices.
    enum class Item : std::uint8_t {
        FrameRate,
        DrawCalls,
        Geometry,
        Memory,
        Counters,
        Physics,
        Particles,
        Audio,
    };

    static constexpr std::size_t kItemCount = 8;

    [[nodiscard]] static std::optional<Mode> modeFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view modeName(Mode value) noexcept;
    [[nodiscard]] static std::optional<Item> itemFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view itemName(Item value) noexcept;
    [[nodiscard]] static std::vector<Item> getDefaultItems();

    [[nodiscard]] bool isShowing(Item item) const noexcept {
        return showing[static_cast<std::size_t>(item)];
    }
    void setShowing(Item item, bool value) noexcept;

    // Shows exactly the items of the list.
    void setItems(const std::vector<Item>& items) noexcept;
    [[nodiscard]] std::vector<Item> getItems() const;

    // Draws the lines in the bottom left corner of the safe area above everything else, sized in points so they read the same at any design resolution. The text takes new numbers a few times per second, so they stay readable, and right away when the display did not draw the frame before or its items changed.
    void drawCompact(core::Engine& engine, const graphics2d::Renderer::Stats& rendering);

    static constexpr std::array<std::pair<std::string_view, Item>, kItemCount> kItemNames{{{"frameRate", Item::FrameRate}, {"drawCalls", Item::DrawCalls}, {"geometry", Item::Geometry}, {"memory", Item::Memory}, {"counters", Item::Counters}, {"physics", Item::Physics}, {"particles", Item::Particles}, {"audio", Item::Audio}}};

  private:
    static const std::array<std::pair<std::string_view, Mode>, 3> kModeNames;
    static constexpr double kRefreshSeconds = 0.25;
    static constexpr float kTextSize = 13.0F;
    static constexpr float kPadding = 6.0F;
    static constexpr float kMargin = 8.0F;

    // Writes the line of every item it shows from what the engine counts now.
    void format(core::Engine& engine, const graphics2d::Renderer::Stats& rendering);
    [[nodiscard]] static std::uint64_t countAlive(std::string_view counter);

    std::array<bool, kItemCount> showing{true, true, true, false, false, false, false, false};
    std::array<std::string, kItemCount> lines;
    std::size_t lineCount = 0;
    std::uint64_t drawnFrame = 0;
    double sinceRefresh = 0.0;
    bool itemsChanged = true;
};

} // namespace haylen::debug
