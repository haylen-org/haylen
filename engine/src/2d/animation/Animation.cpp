#include "haylen/2d/animation/Animation.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

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
        animation.frames.push_back({.source = {origin.x, origin.y, options.frameSize.x, options.frameSize.y}, .originalSize = options.frameSize, .duration = 1.0F / options.framesPerSecond});
    }
    return animation;
}

float Animation::getDuration() const noexcept {
    float total = 0.0F;
    for (const SpriteFrame& frame : frames) {
        total += frame.duration;
    }
    return total;
}

float Animation::getCycleDuration() const noexcept {
    const float total = getDuration();
    if (loop != Loop::PingPong || frames.size() <= 1) {
        return total;
    }
    // The way back skips the two end frames so they do not show twice in a row.
    return total + std::max(0.0F, total - frames.front().duration - frames.back().duration);
}

std::size_t Animation::frameAt(float seconds) const noexcept {
    const float total = getDuration();
    if (frames.size() <= 1 || total <= 0.0F) {
        return 0;
    }

    float time = std::max(0.0F, seconds);
    bool reversed = false;
    switch (loop) {
    case Loop::Once:
        if (time >= total) {
            return frames.size() - 1;
        }
        break;
    case Loop::Loop:
        time = std::fmod(time, total);
        break;
    case Loop::PingPong:
        time = std::fmod(time, getCycleDuration());
        if (time >= total) {
            time -= total;
            reversed = true;
        }
        break;
    }

    if (reversed) {
        for (std::size_t index = frames.size() - 2; index > 0; --index) {
            if (time < frames[index].duration) {
                return index;
            }
            time -= frames[index].duration;
        }
        return 1;
    }

    for (std::size_t index = 0; index < frames.size(); ++index) {
        if (time < frames[index].duration) {
            return index;
        }
        time -= frames[index].duration;
    }
    return frames.size() - 1;
}

} // namespace haylen::animation2d
