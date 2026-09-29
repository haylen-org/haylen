#include "haylen/2d/animation/Animator.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

#include "haylen/2d/graphics/Sprite.hpp"

namespace haylen::animation2d {

void Animator::add(std::string name, Animation animation) {
    if (name.empty() || animation.frames.empty() || !animation.texture.isValid()) {
        throw std::invalid_argument("An animation needs a name, a texture and at least one frame.");
    }

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
        throw std::invalid_argument("Unknown animation: " + std::string(name));
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
    playing = true;
    finishReported = false;
}

void Animator::queue(std::string_view name) {
    (void)getAnimation(name);
    if (current.empty() || isFinished()) {
        play(name);
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
    const float previous = time;
    time += deltaSeconds * speed;
    const std::size_t shown = active.frameAt(time);
    if (shown != frame) {
        frame = shown;
        if (onFrame) {
            onFrame(current, frame);
        }
    }

    if (isFinished() && !finishReported) {
        finishReported = true;
        playing = false;
        if (onFinish) {
            onFinish(current);
        }
    }

    // The next queued animation takes over when the time crosses the end of a pass, unless a callback already switched animations.
    const float cycle = active.getCycleDuration();
    const bool passEnded = cycle <= 0.0F || std::floor(time / cycle) > std::floor(previous / cycle);
    if (!pending.empty() && passEnded && &active == &getAnimation(current)) {
        std::deque<std::string> rest = std::move(pending);
        const std::string next = std::move(rest.front());
        rest.pop_front();
        play(next, true);
        pending = std::move(rest);
    }
}

void Animator::apply(graphics2d::Sprite& sprite) const {
    if (current.empty()) {
        return;
    }

    const Animation& active = getAnimation(current);
    const SpriteFrame& shown = active.frames[frame];
    sprite.texture = active.texture;
    sprite.source = shown.source;
    sprite.size = shown.source.getSize();
    sprite.pivot = (pivot * shown.originalSize - shown.offset) / shown.source.getSize();
}

bool Animator::isFinished() const noexcept {
    if (current.empty()) {
        return false;
    }
    const Animation& active = animations.find(current)->second;
    return active.loop == Animation::Loop::Once && time >= active.getDuration();
}

} // namespace haylen::animation2d
