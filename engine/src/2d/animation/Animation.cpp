#include "haylen/2d/animation/Animation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace haylen::animation2d {

const std::array<std::pair<std::string_view, Animation::Loop>, 3> Animation::kLoopNames{{{"loop", Loop::Loop}, {"once", Loop::Once}, {"pingPong", Loop::PingPong}}};

std::optional<Animation::Loop> Animation::loopFromName(std::string_view name) noexcept {
    for (const auto& [text, mode] : kLoopNames) {
        if (text == name) {
            return mode;
        }
    }
    return std::nullopt;
}

std::string_view Animation::loopName(Loop mode) noexcept {
    for (const auto& [text, value] : kLoopNames) {
        if (value == mode) {
            return text;
        }
    }
    return kLoopNames.front().first;
}

Animation Animation::fromGrid(graphics::Texture image, const GridOptions& options) {
    if (!image.isValid() || options.frameSize.x <= 0.0F || options.frameSize.y <= 0.0F || options.framesPerSecond <= 0.0F) {
        throw std::invalid_argument("A grid animation needs a texture, a positive frame size and a positive frame rate.");
    }

    const math::Vec2 step = options.frameSize + options.spacing;
    const math::Vec2 usable = image.getSize() - options.margin * 2.0F + options.spacing;
    const int columns = static_cast<int>(std::floor(usable.x / step.x));
    const int rows = static_cast<int>(std::floor(usable.y / step.y));
    const int cellCount = columns * rows;
    if (cellCount <= 0) {
        throw std::invalid_argument("The frame size does not fit the texture.");
    }

    std::vector<int> cells = options.cells;
    if (cells.empty()) {
        for (int cell = 0; cell < cellCount; ++cell) {
            cells.push_back(cell);
        }
    }

    Animation animation{.texture = std::move(image), .loop = options.loop};
    for (const int cell : cells) {
        if (cell < 0 || cell >= cellCount) {
            throw std::out_of_range("A grid animation refers to a cell outside the texture.");
        }
        const math::Vec2 origin = options.margin + math::Vec2{static_cast<float>(cell % columns) * step.x, static_cast<float>(cell / columns) * step.y};
        animation.frames.push_back({.source = {origin.x, origin.y, options.frameSize.x, options.frameSize.y}, .originalSize = options.frameSize, .duration = 1.0F / options.framesPerSecond, .ticks = options.ticksPerFrame});
    }
    return animation;
}

void Animation::validate() const {
    const bool ticked = countsTicks();
    for (const SpriteFrame& frame : frames) {
        if ((frame.ticks > 0) != ticked) {
            throw std::invalid_argument("Every frame of an animation counts ticks, or none does.");
        }
        if (!ticked && !(frame.duration > 0.0F && std::isfinite(frame.duration))) {
            throw std::invalid_argument("A frame lasts a positive number of seconds.");
        }
    }
}

void Animation::addEvent(std::size_t frame, std::string name) {
    if (frame >= frames.size()) {
        throw std::out_of_range("The animation has no frame " + std::to_string(frame + 1) + ".");
    }
    frames[frame].events.push_back(std::move(name));
}

float Animation::getLength(std::size_t frame) const noexcept {
    return frames[frame].ticks > 0 ? static_cast<float>(frames[frame].ticks) : frames[frame].duration;
}

float Animation::getDuration() const noexcept {
    float total = 0.0F;
    for (std::size_t index = 0; index < frames.size(); ++index) {
        total += getLength(index);
    }
    return total;
}

float Animation::getCycleDuration() const noexcept {
    const float total = getDuration();
    if (loop != Loop::PingPong || frames.size() <= 1) {
        return total;
    }
    // The way back skips the two end frames so they do not show twice in a row.
    return total + std::max(0.0F, total - getLength(0) - getLength(frames.size() - 1));
}

std::size_t Animation::frameAt(float time) const noexcept {
    const float total = getDuration();
    if (frames.size() <= 1 || total <= 0.0F) {
        return 0;
    }

    float local = std::max(0.0F, time);
    bool reversed = false;
    switch (loop) {
    case Loop::Once:
        if (local >= total) {
            return frames.size() - 1;
        }
        break;
    case Loop::Loop:
        local = std::fmod(local, total);
        break;
    case Loop::PingPong:
        local = std::fmod(local, getCycleDuration());
        if (local >= total) {
            local -= total;
            reversed = true;
        }
        break;
    }

    if (reversed) {
        for (std::size_t index = frames.size() - 2; index > 0; --index) {
            if (local < getLength(index)) {
                return index;
            }
            local -= getLength(index);
        }
        return 1;
    }

    for (std::size_t index = 0; index < frames.size(); ++index) {
        if (local < getLength(index)) {
            return index;
        }
        local -= getLength(index);
    }
    return frames.size() - 1;
}

// The frames of one pass follow each other in a known order, forward and on ping-pong animations back, so the next change is the end of the frame the time falls in, which shows the next frame of the order.
Animation::Change Animation::getNextChange(float time) const noexcept {
    const float cycle = getCycleDuration();
    const std::size_t count = frames.size();
    if (count <= 1 || cycle <= 0.0F) {
        return {.time = std::numeric_limits<float>::infinity(), .frame = 0};
    }
    const float clamped = std::max(0.0F, time);
    if (loop == Loop::Once && clamped >= getDuration() - getLength(count - 1)) {
        return {.time = std::numeric_limits<float>::infinity(), .frame = count - 1};
    }

    // Rounding can place the time at the end of the pass that the division picks, so the change then comes from the pass after it, and a change is always later than the time.
    float start = loop == Loop::Once ? 0.0F : std::floor(clamped / cycle) * cycle;
    const std::size_t steps = loop == Loop::PingPong ? count * 2 - 2 : count;
    const auto frameOf = [count](std::size_t step) { return step < count ? step : count * 2 - 2 - step; };
    for (int pass = 0; pass < 2; ++pass) {
        float end = start;
        for (std::size_t step = 0; step < steps; ++step) {
            end += getLength(frameOf(step));
            if (end > clamped) {
                return {.time = end, .frame = step + 1 < steps ? frameOf(step + 1) : 0};
            }
        }
        start = end;
    }
    return {.time = std::nextafter(clamped, std::numeric_limits<float>::infinity()), .frame = frameOf(1)};
}

} // namespace haylen::animation2d
