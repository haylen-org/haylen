#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
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

    // A grid with ticks per frame counts fixed steps instead of seconds.
    struct GridOptions {
        math::Vec2 frameSize{};
        std::vector<int> cells;
        float framesPerSecond = 10.0F;
        std::uint32_t ticksPerFrame = 0;
        Loop loop = Loop::Loop;
        math::Vec2 margin{};
        math::Vec2 spacing{};
    };

    graphics::Texture texture;
    std::vector<SpriteFrame> frames;
    Loop loop = Loop::Loop;

    // Resolves the loop names `loop`, `once` and `pingPong`.
    [[nodiscard]] static std::optional<Loop> loopFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view loopName(Loop mode) noexcept;

    // Cuts equally sized cells numbered from zero, left to right and top to bottom. An empty cell list uses every cell of the texture.
    [[nodiscard]] static Animation fromGrid(graphics::Texture image, const GridOptions& options);

    // An animation counts ticks when its frames last whole fixed steps, and seconds otherwise. Throws `std::invalid_argument` when some frames count ticks and others seconds, or a frame lasts no time.
    void validate() const;
    [[nodiscard]] bool countsTicks() const noexcept {
        return !frames.empty() && frames.front().ticks > 0;
    }

    // Adds a named event to a frame counted from zero. Throws `std::out_of_range` for a frame the animation lacks.
    void addEvent(std::size_t frame, std::string name);

    // Times are in seconds, or in ticks for an animation that counts them.
    [[nodiscard]] float getLength(std::size_t frame) const noexcept;
    [[nodiscard]] float getDuration() const noexcept;

    // Returns the length of one full pass, which on ping-pong animations covers the way forward and the way back.
    [[nodiscard]] float getCycleDuration() const noexcept;

    // Returns the frame index shown after the given time, following the loop mode.
    [[nodiscard]] std::size_t frameAt(float time) const noexcept;

    // The next change of the frame shown: when it happens and the frame it shows.
    struct Change {
        float time = 0.0F;
        std::size_t frame = 0;
    };

    // Returns the first change after the given time, at infinity once a one-shot animation shows its last frame for good.
    [[nodiscard]] Change getNextChange(float time) const noexcept;

  private:
    static const std::array<std::pair<std::string_view, Loop>, 3> kLoopNames;
};

} // namespace haylen::animation2d
