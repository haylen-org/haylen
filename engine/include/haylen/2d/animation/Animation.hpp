#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/2d/animation/SpriteFrame.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::animation2d {

// Frames of one texture shown in order, each for its own duration.
struct Animation {
    enum class Loop : std::uint8_t {
        Loop,
        Once,
        PingPong,
    };

    struct GridOptions {
        math::Vec2 frameSize{};
        std::vector<int> cells;
        float framesPerSecond = 10.0F;
        Loop loop = Loop::Loop;
        math::Vec2 margin{};
        math::Vec2 spacing{};
    };

    graphics::Texture texture;
    std::vector<SpriteFrame> frames;
    Loop loop = Loop::Loop;

    // Resolves the loop names "loop", "once" and "ping_pong".
    [[nodiscard]] static std::optional<Loop> loopFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view loopName(Loop mode) noexcept;

    // Cuts equally sized cells numbered from zero, left to right and top to bottom. An empty cell list uses every cell of the texture.
    [[nodiscard]] static Animation fromGrid(graphics::Texture image, const GridOptions& options);

    [[nodiscard]] float getDuration() const noexcept;

    // Returns the length of one full pass, which on ping-pong animations covers the way forward and the way back.
    [[nodiscard]] float getCycleDuration() const noexcept;

    // Returns the frame index shown after the given time, following the loop mode.
    [[nodiscard]] std::size_t frameAt(float seconds) const noexcept;

  private:
    static const std::array<std::pair<std::string_view, Loop>, 3> kLoopNames;
};

} // namespace haylen::animation2d
