#include "haylen/2d/animation/Animator.hpp"

#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

#include "haylen/2d/graphics/Sprite.hpp"
#include "haylen/math/Geometry.hpp"

namespace haylen::animation2d {

void Animator::add(std::string name, Animation animation) {
    if (name.empty() || animation.frames.empty() || !animation.texture.isValid()) {
        throw std::invalid_argument("An animation needs a name, a texture and at least one frame.");
    }
    animation.validate();

    // Replacing the animation that plays keeps its time, so its frame is looked up again among the new frames.
    if (name == current) {
        frame = animation.frameAt(time);
    }
    animations.insert_or_assign(std::move(name), std::move(animation));
}

bool Animator::has(std::string_view name) const {
    return animations.contains(name);
}

const Animation& Animator::getAnimation(std::string_view name) const {
    const auto found = animations.find(name);
    if (found == animations.end()) {
        throw std::invalid_argument("The animation \"" + std::string(name) + "\" does not exist in this animator.");
    }
    return found->second;
}

void Animator::play(std::string_view name, bool restart) {
    (void)getAnimation(name);
    pending.clear();
    if (current == name && !restart) {
        playing = true;
        return;
    }

    current = std::string(name);
    time = 0.0F;
    frame = 0;
    ++plays;
    playing = true;
    entered = false;
    finishReported = false;
}

void Animator::queue(std::string_view name) {
    (void)getAnimation(name);
    if (current.empty() || isFinished()) {
        play(name, true);
        return;
    }
    pending.emplace_back(name);
}

void Animator::stop() noexcept {
    playing = false;
}

void Animator::update(float deltaSeconds) {
    if (!playing || current.empty()) {
        return;
    }
    const Animation& active = getAnimation(current);
    if (active.countsTicks()) {
        throw std::logic_error("The animation \"" + current + "\" counts ticks, so it advances with \"step\".");
    }
    advance(active, deltaSeconds);
}

void Animator::step(std::uint32_t ticks) {
    if (!playing || current.empty()) {
        return;
    }
    const Animation& active = getAnimation(current);
    if (!active.countsTicks()) {
        throw std::logic_error("The animation \"" + current + "\" counts seconds, so it cannot step by ticks.");
    }
    advance(active, static_cast<float>(ticks));
}

// Shows a frame, reports it and runs its events, unless a callback already switched animations.
void Animator::enter(std::size_t shown) {
    frame = shown;
    const std::string name = current;
    const std::uint64_t played = plays;
    if (onFrame) {
        onFrame(name, frame);
    }
    const std::vector<std::string> events = getAnimation(name).frames[shown].events;
    for (std::size_t index = 0; index < events.size() && onEvent && plays == played; ++index) {
        onEvent(name, events[index], shown);
    }
}

void Animator::advance(const Animation& active, float amount) {
    const std::uint64_t played = plays;
    if (!entered) {
        entered = true;
        const std::vector<std::string> events = active.frames[frame].events;
        for (std::size_t index = 0; index < events.size() && onEvent; ++index) {
            onEvent(current, events[index], frame);
            if (plays != played) {
                return;
            }
        }
    }

    const float previous = time;
    time += amount * speed;
    const float cycle = active.getCycleDuration();
    if (time < previous) {
        const std::size_t shown = active.frameAt(time);
        if (shown != frame) {
            enter(shown);
        }
    } else {
        // Every change of frame between the two times is entered in order, and a step of a looping animation longer than a few passes keeps only its last ones.
        const bool repeats = active.loop != Animation::Loop::Once && cycle > 0.0F;
        const float cursor = repeats && time - previous > cycle * kMaxPasses ? time - cycle * kMaxPasses : previous;
        for (Animation::Change change = active.getNextChange(cursor); change.time <= time && plays == played; change = active.getNextChange(change.time)) {
            if (change.frame != frame) {
                enter(change.frame);
            }
        }
    }
    if (plays != played) {
        return;
    }

    if (isFinished() && !finishReported) {
        finishReported = true;
        playing = false;
        if (onFinish) {
            onFinish(current);
        }
    }

    // The next queued animation takes over when the time crosses the end of a pass, unless a callback already switched animations.
    const bool passEnded = cycle <= 0.0F || std::floor(time / cycle) > std::floor(previous / cycle);
    if (!pending.empty() && passEnded && plays == played) {
        std::deque<std::string> rest = std::move(pending);
        const std::string next = std::move(rest.front());
        rest.pop_front();
        play(next, true);
        pending = std::move(rest);
    }
}

const SpriteFrame* Animator::getShown() const {
    return current.empty() ? nullptr : &getAnimation(current).frames[frame];
}

void Animator::apply(graphics2d::Sprite& sprite) const {
    const SpriteFrame* shown = getShown();
    if (shown == nullptr) {
        return;
    }
    sprite.texture = getAnimation(current).texture;
    sprite.source = shown->source;
    sprite.size = shown->source.getSize();
    const math::Vec2 anchor = shown->pivot.value_or(pivot * shown->originalSize);
    math::Vec2 placed = (anchor - shown->offset) / shown->source.getSize();
    if (sprite.flip.horizontal) {
        placed.x = 1.0F - placed.x;
    }
    if (sprite.flip.vertical) {
        placed.y = 1.0F - placed.y;
    }
    sprite.pivot = placed;
}

// The point goes from the untrimmed frame to the drawn quad, mirrored as the sprite flips, then around the pivot of the sprite at its size, scale and rotation.
math::Vec2 Animator::place(math::Vec2 point, const graphics2d::Sprite& sprite) const {
    const SpriteFrame& shown = *getShown();
    const math::Vec2 sourceSize = shown.source.getSize();
    math::Vec2 share = (point - shown.offset) / sourceSize;
    if (sprite.flip.horizontal) {
        share.x = 1.0F - share.x;
    }
    if (sprite.flip.vertical) {
        share.y = 1.0F - share.y;
    }
    const math::Vec2 size = (sprite.size.isZero() ? sourceSize : sprite.size) * sprite.scale;
    return sprite.position + ((share - sprite.pivot) * size).rotated(sprite.rotation);
}

std::optional<math::Rect> Animator::getBox(std::string_view name, const graphics2d::Sprite& sprite) const {
    const SpriteFrame* shown = getShown();
    const SpriteFrame::Box* box = shown != nullptr ? shown->findBox(name) : nullptr;
    if (box == nullptr) {
        return std::nullopt;
    }
    const math::Rect& area = box->area;
    const std::array<math::Vec2, 4> corners{place(area.getMin(), sprite), place({area.getRight(), area.y}, sprite), place(area.getMax(), sprite), place({area.x, area.getBottom()}, sprite)};
    return math::Geometry::bounds(corners);
}

std::optional<math::Vec2> Animator::getPoint(std::string_view name, const graphics2d::Sprite& sprite) const {
    const SpriteFrame* shown = getShown();
    const SpriteFrame::Point* point = shown != nullptr ? shown->findPoint(name) : nullptr;
    if (point == nullptr) {
        return std::nullopt;
    }
    return place(point->position, sprite);
}

bool Animator::isFinished() const noexcept {
    if (current.empty()) {
        return false;
    }
    const Animation& active = animations.find(current)->second;
    return active.loop == Animation::Loop::Once && time >= active.getDuration();
}

} // namespace haylen::animation2d
