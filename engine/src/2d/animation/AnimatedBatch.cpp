#include "haylen/2d/animation/AnimatedBatch.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "haylen/core/JobSystem.hpp"

namespace haylen::animation2d {

AnimatedBatch::AnimatedBatch(graphics::Texture image) : batch(std::move(image)) {
    if (!batch.getTexture().isValid()) {
        throw std::invalid_argument("An animated batch needs a texture.");
    }
}

void AnimatedBatch::addAnimation(std::string name, Animation animation) {
    if (name.empty() || animation.frames.empty() || !(animation.texture == batch.getTexture())) {
        throw std::invalid_argument("An animation of an animated batch needs a name, at least one frame and the texture of the batch.");
    }
    animation.validate();
    if (!animations.empty() && animation.countsTicks() != ticks) {
        throw std::invalid_argument("The animations of an animated batch all count seconds or all count ticks.");
    }
    ticks = animation.countsTicks();

    const auto found = std::ranges::find(names, name);
    if (found != names.end()) {
        const auto slot = static_cast<std::size_t>(found - names.begin());
        animations[slot] = std::move(animation);
        for (std::size_t index = 0; index < states.size(); ++index) {
            if (states[index].animation == slot) {
                show(index);
            }
        }
        return;
    }
    names.push_back(std::move(name));
    animations.push_back(std::move(animation));
}

bool AnimatedBatch::hasAnimation(std::string_view name) const noexcept {
    return std::ranges::find(names, name) != names.end();
}

std::vector<std::string> AnimatedBatch::getAnimationNames() const {
    return names;
}

std::uint32_t AnimatedBatch::findAnimation(std::string_view name) const {
    const auto found = std::ranges::find(names, name);
    if (found == names.end()) {
        throw std::invalid_argument("The animation \"" + std::string(name) + "\" does not exist in this animated batch.");
    }
    return static_cast<std::uint32_t>(found - names.begin());
}

void AnimatedBatch::check(std::size_t index) const {
    if (index >= states.size()) {
        throw std::out_of_range("Animated batch index is out of range.");
    }
}

std::size_t AnimatedBatch::add(const graphics2d::SpriteInstance& sprite, std::string_view animation, float speed) {
    const std::uint32_t slot = findAnimation(animation);
    const std::size_t index = batch.add(sprite);
    states.push_back({.animation = slot, .speed = speed});
    show(index);
    return index;
}

void AnimatedBatch::remove(std::size_t index) {
    check(index);
    batch.remove(index);
    states.erase(states.begin() + static_cast<std::ptrdiff_t>(index));
}

void AnimatedBatch::clear() noexcept {
    batch.clear();
    states.clear();
    endedSprites.clear();
}

void AnimatedBatch::reserve(std::size_t count) {
    batch.reserve(count);
    states.reserve(count);
}

void AnimatedBatch::play(std::size_t index, std::string_view animation, bool restart) {
    check(index);
    const std::uint32_t slot = findAnimation(animation);
    State& state = states[index];
    if (state.animation == slot && !restart) {
        return;
    }
    state = {.animation = slot, .speed = state.speed, .scale = state.scale};
    show(index);
}

const std::string& AnimatedBatch::getAnimation(std::size_t index) const {
    check(index);
    return names[states[index].animation];
}

std::size_t AnimatedBatch::getFrame(std::size_t index) const {
    check(index);
    return states[index].frame;
}

float AnimatedBatch::getTime(std::size_t index) const {
    check(index);
    return states[index].time;
}

void AnimatedBatch::seek(std::size_t index, float value) {
    check(index);
    State& state = states[index];
    state.time = value;
    state.frame = static_cast<std::uint32_t>(animations[state.animation].frameAt(value));
    show(index);
}

float AnimatedBatch::getCycleDuration(std::size_t index) const {
    check(index);
    return animations[states[index].animation].getCycleDuration();
}

float AnimatedBatch::getSpeed(std::size_t index) const {
    check(index);
    return states[index].speed;
}

void AnimatedBatch::setSpeed(std::size_t index, float value) {
    check(index);
    states[index].speed = value;
}

math::Vec2 AnimatedBatch::getScale(std::size_t index) const {
    check(index);
    return states[index].scale;
}

void AnimatedBatch::setScale(std::size_t index, math::Vec2 value) {
    check(index);
    states[index].scale = value;
    show(index);
}

const graphics2d::SpriteInstance& AnimatedBatch::get(std::size_t index) const {
    return batch.get(index);
}

void AnimatedBatch::set(std::size_t index, const graphics2d::SpriteInstance& sprite) {
    check(index);
    batch.set(index, sprite);
    show(index);
}

void AnimatedBatch::setPartColors(std::size_t index, const graphics2d::PartColors& colors) {
    batch.setPartColors(index, colors);
}

graphics2d::PartColors AnimatedBatch::getPartColors(std::size_t index) const {
    return batch.getPartColors(index);
}

void AnimatedBatch::writeFields(std::span<const float> values, const graphics2d::SpriteLayout& layout, std::size_t first) {
    batch.writeFields(values, layout, first);
    const std::size_t end = std::min(states.size(), first + layout.getCount(values));
    for (std::size_t index = first; index < end; ++index) {
        show(index);
    }
}

void AnimatedBatch::readFields(std::span<float> values, const graphics2d::SpriteLayout& layout, std::size_t first) const {
    batch.readFields(values, layout, first);
}

void AnimatedBatch::update(float deltaSeconds, core::JobSystem* jobs) {
    if (ticks) {
        throw std::logic_error("The animations of the animated batch count ticks, so it advances with \"step\".");
    }
    advance(deltaSeconds, jobs);
}

void AnimatedBatch::step(std::uint32_t count, core::JobSystem* jobs) {
    if (!ticks && !animations.empty()) {
        throw std::logic_error("The animations of the animated batch count seconds, so it cannot step by ticks.");
    }
    advance(static_cast<float>(count), jobs);
}

void AnimatedBatch::advance(float amount, core::JobSystem* jobs) {
    // clang-format off
    const auto run = [this, amount](std::size_t begin, std::size_t end) {
        for (std::size_t index = begin; index < end; ++index) {
            State& state = states[index];
            const Animation& animation = animations[state.animation];
            state.time += amount * state.speed;
            state.justEnded = !state.ended && animation.loop == Animation::Loop::Once && state.time >= animation.getDuration();
            state.ended = state.ended || state.justEnded;
            const auto shown = static_cast<std::uint32_t>(animation.frameAt(state.time));
            if (shown != state.frame) {
                state.frame = shown;
                show(index);
            }
        }
    };
    // clang-format on
    if (jobs != nullptr && states.size() >= kParallelSprites) {
        jobs->parallelFor(0, states.size(), kParallelGrain, run);
    } else {
        run(0, states.size());
    }

    endedSprites.clear();
    for (std::size_t index = 0; index < states.size(); ++index) {
        if (states[index].justEnded) {
            endedSprites.push_back(index);
        }
    }
    if (!endedSprites.empty()) {
        completed.emit(endedSprites);
    }
}

// The pivot of the frame, or the middle of its untrimmed frame, lands where the sprite stands, mirrored in the quad where the sprite flips.
void AnimatedBatch::show(std::size_t index) {
    const State& state = states[index];
    const SpriteFrame& frame = animations[state.animation].frames[state.frame];
    graphics2d::SpriteInstance sprite = batch.get(index);
    sprite.source = frame.source;
    sprite.size = frame.source.getSize() * state.scale;
    math::Vec2 pivot = (frame.pivot.value_or(frame.originalSize * 0.5F) - frame.offset) / frame.source.getSize();
    if (sprite.flip.horizontal) {
        pivot.x = 1.0F - pivot.x;
    }
    if (sprite.flip.vertical) {
        pivot.y = 1.0F - pivot.y;
    }
    sprite.pivot = pivot;
    batch.set(index, sprite);
}

void AnimatedBatch::draw(graphics2d::Renderer& renderer, const graphics2d::DrawOrder& order) const {
    batch.draw(renderer, order);
}

} // namespace haylen::animation2d
