#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/core/Json.hpp"

namespace haylen::animation2d {

// The states of an animated character and the rules that move between them, checked once: parameters (numbers, booleans, triggers and vectors), states that play an animation or pick one from a directional set by a vector parameter, and transitions with a condition, an exit time and a fade, from a state or from any state. Conditions compare parameters with numbers and join them with `and`, `or` and `not`, parsed once, so a player evaluates them without running scripts.
class StateGraph final {
  public:
    enum class Kind : std::uint8_t {
        Number,
        Boolean,
        Trigger,
        Vector,
    };

    struct Parameter {
        std::string name;
        Kind kind = Kind::Number;
        std::array<float, 2> value{};

        // Where the parameter keeps its value among the values of a player, two places for a vector.
        std::size_t slot = 0;
    };

    // The directions a set names: four, or eight with the diagonals.
    enum class Direction : std::uint8_t {
        Right,
        DownRight,
        Down,
        DownLeft,
        Left,
        UpLeft,
        Up,
        UpRight,
    };

    struct State {
        std::string name;
        std::string animation;

        // The animation of each direction, by the order of `Direction`, empty for the directions a four-way set leaves out, and the vector parameter that picks one.
        std::array<std::string, 8> directions{};
        std::optional<std::size_t> by;
        bool eightWays = false;
    };

    // A node of a parsed condition: a comparison of two operands, a joint of two conditions, a negation, a parameter or a number.
    struct Term {
        enum class Op : std::uint8_t {
            Constant,
            Value,
            Not,
            And,
            Or,
            Equal,
            NotEqual,
            Less,
            LessEqual,
            Greater,
            GreaterEqual,
        };

        Op op = Op::Constant;
        float constant = 0.0F;
        std::size_t slot = 0;
        std::size_t left = 0;
        std::size_t right = 0;
    };

    // A transition from a state, or from any other state without one, which fires when its condition holds, which is always without a condition, and once the animation of the state has played the exit time, a share of one pass.
    struct Transition {
        std::optional<std::size_t> from;
        std::size_t to = 0;
        std::optional<std::size_t> condition;
        std::vector<std::size_t> triggers;
        float exitTime = 0.0F;
        float fade = 0.0F;
    };

    // Reads a description and throws `std::invalid_argument` with a sentence that names the state, the transition or the parameter at fault.
    [[nodiscard]] static StateGraph parse(const core::Json& description);

    [[nodiscard]] static std::optional<Kind> kindFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view kindName(Kind value) noexcept;
    [[nodiscard]] static std::optional<Direction> directionFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view directionName(Direction value) noexcept;

    [[nodiscard]] const std::vector<Parameter>& getParameters() const noexcept {
        return parameters;
    }
    [[nodiscard]] const std::vector<State>& getStates() const noexcept {
        return states;
    }
    [[nodiscard]] const std::vector<Transition>& getTransitions() const noexcept {
        return transitions;
    }
    [[nodiscard]] std::size_t getInitial() const noexcept {
        return initial;
    }
    [[nodiscard]] std::size_t getValueCount() const noexcept {
        return valueCount;
    }

    [[nodiscard]] const Parameter* findParameter(std::string_view name) const noexcept;
    [[nodiscard]] std::optional<std::size_t> findState(std::string_view name) const noexcept;

    // Returns whether a parsed condition holds for the values of a player.
    [[nodiscard]] bool evaluate(std::size_t condition, const std::vector<float>& values) const noexcept;

    // Returns the animation a state plays for the vector of its set, keeping the current direction while the vector stays near its sector or is zero.
    [[nodiscard]] std::size_t pickDirection(const State& state, const std::vector<float>& values, std::size_t current) const noexcept;

    // Every animation name the states play, once each.
    [[nodiscard]] std::vector<std::string> getAnimationNames() const;

  private:
    static const std::array<std::pair<std::string_view, Kind>, 4> kKindNames;
    static const std::array<std::pair<std::string_view, Direction>, 8> kDirectionNames;

    // A vector must turn this share of half a sector past the edge of the sector of its current direction before the direction changes, which avoids flicker on the diagonals.
    static constexpr float kHysteresis = 0.25F;

    // Reads conditions in the grammar of transitions from left to right.
    class Parser;

    [[nodiscard]] float valueOf(std::size_t term, const std::vector<float>& values) const noexcept;

    std::vector<Parameter> parameters;
    std::vector<State> states;
    std::vector<Transition> transitions;
    std::vector<Term> terms;
    std::size_t initial = 0;
    std::size_t valueCount = 0;
};

} // namespace haylen::animation2d
