#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "haylen/2d/animation/StateGraph.hpp"
#include "haylen/core/Signal.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::animation2d {

class AnimatedBatch;
class Animator;

// Runs a state graph on a target that holds the animations its states name: an animator, or one sprite of an animated batch. A player keeps the values of the parameters and the state, picks the animation of directional states by their vector, and takes the first transition whose source, exit time and condition allow it on each update, consuming the triggers its condition reads. It switches the animations of its target and never advances it, so the target updates as it always does.
class StateGraphPlayer final {
  public:
    struct BatchSprite {
        std::shared_ptr<AnimatedBatch> batch;
        std::size_t index = 0;
    };

    using Target = std::variant<std::shared_ptr<Animator>, BatchSprite>;

    // Starts in the initial state. Throws `std::invalid_argument` when the target lacks an animation a state plays.
    StateGraphPlayer(std::shared_ptr<const StateGraph> stateGraph, Target playTarget);

    // Each throws `std::invalid_argument` for a parameter the graph lacks or of another kind.
    void setNumber(std::string_view name, float value);
    void setBoolean(std::string_view name, bool value);
    void setVector(std::string_view name, math::Vec2 value);
    void trigger(std::string_view name);
    [[nodiscard]] float getNumber(std::string_view name) const;
    [[nodiscard]] bool getBoolean(std::string_view name) const;
    [[nodiscard]] math::Vec2 getVector(std::string_view name) const;

    // Takes at most one transition and keeps the direction of a directional state in step with its vector. The time counts how long the state has lasted.
    void update(float deltaSeconds);

    // Enters a state at once, from its start. Throws `std::invalid_argument` for a state the graph lacks.
    void enter(std::string_view state);

    [[nodiscard]] const std::string& getState() const noexcept {
        return graph->getStates()[current].name;
    }
    [[nodiscard]] float getStateTime() const noexcept {
        return stateTime;
    }
    [[nodiscard]] const std::string& getAnimation() const noexcept {
        return playing;
    }
    [[nodiscard]] const StateGraph& getGraph() const noexcept {
        return *graph;
    }

    // Emits the state left and the state entered.
    core::Signal<std::string_view, std::string_view> stateChanged;

  private:
    [[nodiscard]] const StateGraph::Parameter& find(std::string_view name, StateGraph::Kind kind) const;
    [[nodiscard]] bool hasAnimation(const std::string& name) const;
    [[nodiscard]] float getTargetTime() const;
    [[nodiscard]] float getTargetCycle() const;

    // Plays an animation on the target from its start, or at the time the animation before it reached.
    void playOnTarget(const std::string& name, bool keepTime);
    void enterState(std::size_t state);
    [[nodiscard]] const std::string& animationOf(const StateGraph::State& state) const noexcept;

    std::shared_ptr<const StateGraph> graph;
    Target target;
    std::vector<float> values;
    std::size_t current = 0;
    std::size_t direction = static_cast<std::size_t>(StateGraph::Direction::Down);
    std::string playing;
    float stateTime = 0.0F;
};

} // namespace haylen::animation2d
