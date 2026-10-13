#include "haylen/2d/animation/StateGraphPlayer.hpp"

#include <stdexcept>
#include <utility>

#include "haylen/2d/animation/AnimatedBatch.hpp"
#include "haylen/2d/animation/Animator.hpp"
#include "haylen/2d/animation/Skeleton.hpp"

namespace haylen::animation2d {

StateGraphPlayer::StateGraphPlayer(std::shared_ptr<const StateGraph> stateGraph, Target playTarget) : graph(std::move(stateGraph)), target(std::move(playTarget)), values(graph->getValueCount()) {
    if (const auto* animator = std::get_if<std::shared_ptr<Animator>>(&target); animator != nullptr && !*animator) {
        throw std::invalid_argument("A state graph player needs an animator or an animated batch.");
    }
    if (const auto* sprite = std::get_if<BatchSprite>(&target); sprite != nullptr && (!sprite->batch || sprite->index >= sprite->batch->size())) {
        throw std::invalid_argument("A state graph player needs a sprite of its animated batch.");
    }
    if (const auto* track = std::get_if<SkeletonTrack>(&target); track != nullptr && !track->skeleton) {
        throw std::invalid_argument("A state graph player needs a skeleton.");
    }
    for (const std::string& name : graph->getAnimationNames()) {
        if (!hasAnimation(name)) {
            throw std::invalid_argument("The state graph plays the animation \"" + name + "\", which its target lacks.");
        }
    }
    for (const StateGraph::Parameter& parameter : graph->getParameters()) {
        values[parameter.slot] = parameter.value[0];
        if (parameter.kind == StateGraph::Kind::Vector) {
            values[parameter.slot + 1] = parameter.value[1];
        }
    }
    current = graph->getInitial();
    const StateGraph::State& start = graph->getStates()[current];
    if (start.by) {
        direction = graph->pickDirection(start, values, direction);
    }
    playOnTarget(animationOf(start), false, 0.0F);
}

const StateGraph::Parameter& StateGraphPlayer::find(std::string_view name, StateGraph::Kind kind) const {
    const StateGraph::Parameter* parameter = graph->findParameter(name);
    if (parameter == nullptr) {
        throw std::invalid_argument("The state graph has no parameter \"" + std::string(name) + "\".");
    }
    if (parameter->kind != kind) {
        throw std::invalid_argument("The parameter \"" + std::string(name) + "\" is a " + std::string(StateGraph::kindName(parameter->kind)) + ", not a " + std::string(StateGraph::kindName(kind)) + ".");
    }
    return *parameter;
}

void StateGraphPlayer::setNumber(std::string_view name, float value) {
    values[find(name, StateGraph::Kind::Number).slot] = value;
}

void StateGraphPlayer::setBoolean(std::string_view name, bool value) {
    values[find(name, StateGraph::Kind::Boolean).slot] = value ? 1.0F : 0.0F;
}

void StateGraphPlayer::setVector(std::string_view name, math::Vec2 value) {
    const std::size_t slot = find(name, StateGraph::Kind::Vector).slot;
    values[slot] = value.x;
    values[slot + 1] = value.y;
}

void StateGraphPlayer::trigger(std::string_view name) {
    values[find(name, StateGraph::Kind::Trigger).slot] = 1.0F;
}

float StateGraphPlayer::getNumber(std::string_view name) const {
    return values[find(name, StateGraph::Kind::Number).slot];
}

bool StateGraphPlayer::getBoolean(std::string_view name) const {
    return values[find(name, StateGraph::Kind::Boolean).slot] != 0.0F;
}

math::Vec2 StateGraphPlayer::getVector(std::string_view name) const {
    const std::size_t slot = find(name, StateGraph::Kind::Vector).slot;
    return {values[slot], values[slot + 1]};
}

bool StateGraphPlayer::hasAnimation(const std::string& name) const {
    if (const auto* animator = std::get_if<std::shared_ptr<Animator>>(&target)) {
        return (*animator)->has(name);
    }
    if (const auto* sprite = std::get_if<BatchSprite>(&target)) {
        return sprite->batch->hasAnimation(name);
    }
    return std::get<SkeletonTrack>(target).skeleton->getData().findAnimation(name) != nullptr;
}

float StateGraphPlayer::getTargetTime() const {
    if (const auto* animator = std::get_if<std::shared_ptr<Animator>>(&target)) {
        return (*animator)->getTime();
    }
    if (const auto* sprite = std::get_if<BatchSprite>(&target)) {
        return sprite->batch->getTime(sprite->index);
    }
    const SkeletonTrack& track = std::get<SkeletonTrack>(target);
    return track.skeleton->getTrackTime(track.track);
}

float StateGraphPlayer::getTargetCycle() const {
    if (const auto* animator = std::get_if<std::shared_ptr<Animator>>(&target)) {
        return (*animator)->getAnimation(playing).getCycleDuration();
    }
    if (const auto* sprite = std::get_if<BatchSprite>(&target)) {
        return sprite->batch->getCycleDuration(sprite->index);
    }
    return std::get<SkeletonTrack>(target).skeleton->getData().findAnimation(playing)->duration;
}

void StateGraphPlayer::playOnTarget(const std::string& name, bool keepTime, float fade) {
    const float time = keepTime ? getTargetTime() : 0.0F;
    playing = name;
    if (const auto* animator = std::get_if<std::shared_ptr<Animator>>(&target)) {
        (*animator)->play(name, true);
        if (keepTime) {
            (*animator)->seek(time);
        }
        return;
    }
    if (const auto* sprite = std::get_if<BatchSprite>(&target)) {
        sprite->batch->play(sprite->index, name, true);
        if (keepTime) {
            sprite->batch->seek(sprite->index, time);
        }
        return;
    }
    const SkeletonTrack& track = std::get<SkeletonTrack>(target);
    track.skeleton->play(name, {.track = track.track, .loop = true, .fade = fade});
    if (keepTime) {
        track.skeleton->seekTrack(track.track, time);
    }
}

const std::string& StateGraphPlayer::animationOf(const StateGraph::State& state) const noexcept {
    return state.by ? state.directions[direction] : state.animation;
}

// A directional state keeps the direction the player faced in the state before, until its vector turns it.
void StateGraphPlayer::enterState(std::size_t state, float fade) {
    const std::string from = getState();
    current = state;
    stateTime = 0.0F;
    const StateGraph::State& entered = graph->getStates()[current];
    if (entered.by) {
        direction = graph->pickDirection(entered, values, entered.directions[direction].empty() ? static_cast<std::size_t>(StateGraph::Direction::Down) : direction);
    }
    playOnTarget(animationOf(entered), false, fade);
    stateChanged.emit(from, entered.name);
}

void StateGraphPlayer::enter(std::string_view state) {
    const std::optional<std::size_t> found = graph->findState(state);
    if (!found) {
        throw std::invalid_argument("The state graph has no state \"" + std::string(state) + "\".");
    }
    enterState(*found, 0.0F);
}

void StateGraphPlayer::update(float deltaSeconds) {
    stateTime += deltaSeconds;
    const StateGraph::State& state = graph->getStates()[current];
    if (state.by) {
        const std::size_t picked = graph->pickDirection(state, values, direction);
        if (picked != direction) {
            direction = picked;
            playOnTarget(animationOf(state), true, 0.0F);
        }
    }

    // The first transition that may fire wins, and it consumes the triggers its condition reads.
    for (const StateGraph::Transition& transition : graph->getTransitions()) {
        if (transition.from ? *transition.from != current : transition.to == current) {
            continue;
        }
        if (transition.exitTime > 0.0F && getTargetTime() < transition.exitTime * getTargetCycle()) {
            continue;
        }
        if (transition.condition && !graph->evaluate(*transition.condition, values)) {
            continue;
        }
        for (const std::size_t slot : transition.triggers) {
            values[slot] = 0.0F;
        }
        enterState(transition.to, transition.fade);
        return;
    }
}

} // namespace haylen::animation2d
