#pragma once

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/input/Controls.hpp"
#include "haylen/input/GamepadAxis.hpp"
#include "haylen/input/GamepadButton.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/input/Key.hpp"
#include "haylen/input/MouseButton.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::input {

class VirtualInput;

// Maps named gameplay actions to devices in definition order. Buttons report edges, axes report [-1, 1] and vectors report a length up to 1.
class ActionMap final {
  public:
    // One physical or virtual input written as `key:w`, `mouse:left`, `button:south`, `axis:leftY-`, `stick:left`, `virtual:attack` or `virtualStick:move`.
    struct Binding {
        enum class Source : std::uint8_t {
            Key,
            MouseButton,
            GamepadButton,
            GamepadAxis,
            GamepadStick,
            VirtualButton,
            VirtualStick,
        };

        Source source = Source::Key;
        Key key = Key::Unknown;
        MouseButton mouseButton = MouseButton::Left;
        GamepadButton gamepadButton = GamepadButton::South;
        GamepadAxis gamepadAxis = GamepadAxis::LeftX;
        float direction = 1.0F;
        bool rightStick = false;
        std::string name;

        [[nodiscard]] static std::optional<Binding> parse(std::string_view text);
        [[nodiscard]] std::string toString() const;
        [[nodiscard]] bool operator==(const Binding&) const = default;
    };

    // One action of an action map document, such as `{"name": "jump", "type": "button", "bindings": ["key:space"], "buffer": 0.12}`. The buffer keeps a press available to `consumePress` for that many seconds of fixed steps after the step that first saw it.
    struct Action {
        enum class Type : std::uint8_t {
            Button,
            Axis,
            Vector,
        };

        std::string name;
        Type type = Type::Button;
        std::vector<Binding> bindings;
        std::vector<Binding> positive;
        std::vector<Binding> negative;
        std::vector<Binding> up;
        std::vector<Binding> down;
        std::vector<Binding> left;
        std::vector<Binding> right;
        float buffer = 0.0F;

        [[nodiscard]] static Action fromJson(const core::Json& json);
        [[nodiscard]] core::Json toJson() const;

        [[nodiscard]] static std::optional<Type> typeFromName(std::string_view text) noexcept;
        [[nodiscard]] static std::string_view typeName(Type value) noexcept;
        [[nodiscard]] bool operator==(const Action&) const = default;

      private:
        static const std::array<std::pair<std::string_view, Type>, 3> kTypeNames;
    };

    // The keys, gamepad buttons and gamepad axes the UI answers itself in the next frame: every key while a text field edits, and the bindings of the navigation actions it handles, such as `cancel` while a popup is open and the directions while a control has the focus.
    struct Capture {
        bool keyboard = false;
        std::bitset<Controls::kKeyCount> keys;
        std::bitset<Controls::kGamepadButtonCount> buttons;
        std::bitset<Controls::kGamepadAxisCount> axes;
    };

    void load(const core::Json& document);
    [[nodiscard]] core::Json save() const;

    void define(Action action);
    void remove(std::string_view name);
    void clear() noexcept;
    [[nodiscard]] const Action* findAction(std::string_view name) const noexcept;
    [[nodiscard]] std::vector<std::string> getNames() const;

    void setGamepadIndex(std::optional<std::size_t> value) noexcept {
        gamepadIndex = value;
    }
    // Takes a threshold above 0 and up to 1 and throws `std::invalid_argument` for any other value.
    void setPressThreshold(float value);
    // Blocked input, such as during a scene change or while the app is halted, reads every action as up. An action still down when input returns stays up until its bindings let go, so a key held across a scene change never reads as a second press.
    void update(const Input& input, const VirtualInput& virtualInput, bool blocked);

    // A key or gamepad button pressed while the UI captures it stays with the UI until it is released, and every binding reads it as up meanwhile, and so does a mouse button pressed while the UI owns the pointer. A press the game took stays with the game the same way, also when the pointer passes over the UI. An axis is pressed while it leans past the press threshold, and a captured axis also holds the stick it belongs to.
    void setCapture(const Capture& value) noexcept {
        capture = value;
    }
    [[nodiscard]] bool isKeyCaptured(Key key) const noexcept;
    [[nodiscard]] bool isMouseButtonCaptured(MouseButton button) const noexcept;
    [[nodiscard]] bool isGamepadButtonCaptured(std::size_t index, GamepadButton button) const noexcept;
    [[nodiscard]] bool isGamepadAxisCaptured(std::size_t index, GamepadAxis axis) const noexcept;

    [[nodiscard]] bool isDown(std::string_view name) const noexcept;
    [[nodiscard]] bool isPressed(std::string_view name) const noexcept;
    [[nodiscard]] bool isReleased(std::string_view name) const noexcept;
    [[nodiscard]] float getValue(std::string_view name) const noexcept;
    [[nodiscard]] math::Vec2 getVector(std::string_view name) const noexcept;

    // Moves the edges that arrived since the previous fixed step into the step state, which the engine does at the start of every fixed step. A press reaches exactly one step, the first one after it, and a release that arrived together with its press reaches the step after that one. Buffered presses age by the seconds of the step.
    void beginStep(float stepSeconds) noexcept;
    [[nodiscard]] bool isStepPressed(std::string_view name) const noexcept;
    [[nodiscard]] bool isStepReleased(std::string_view name) const noexcept;
    // Whether the action is down in this step, or was pressed since the previous one, so a tap shorter than a step still holds it for one step.
    [[nodiscard]] bool isStepDown(std::string_view name) const noexcept;

    // Returns `true` once for each press, from the frame of the press until the end of the first fixed step after it and for the buffer of the action beyond that step, and takes the press so nothing reads it again.
    bool consumePress(std::string_view name) noexcept;

  private:
    // Seconds that the age of a buffered press may pass its buffer by, so a buffer that is a whole number of steps keeps its last step despite rounding.
    static constexpr float kBufferTolerance = 1.0e-4F;

    struct State {
        Action action;
        bool down = false;
        bool pressed = false;
        bool released = false;
        bool held = false;
        float value = 0.0F;
        math::Vec2 vector{};
        bool pendingPress = false;
        bool pendingRelease = false;
        bool releaseAfterPress = false;
        bool stepDown = false;
        bool stepPressed = false;
        bool stepReleased = false;
        bool buffered = false;
        bool bufferStepped = false;
        float bufferAge = 0.0F;
    };

    void trackStepEdges(State& state, bool blocked) noexcept;

    // Lua hands an empty table over as an empty object, which reads as an empty list.
    [[nodiscard]] static bool isList(const core::Json& value) noexcept;
    [[nodiscard]] static std::vector<Binding> parseBindings(const core::Json& action, const char* field);
    [[nodiscard]] static core::Json saveBindings(const std::vector<Binding>& list);

    // Rejects binding lists the type of the action never reads, so a misplaced binding never goes unnoticed.
    static void validate(const Action& action);

    [[nodiscard]] float getBindingValue(const Binding& binding, const Input& input, const VirtualInput& virtualInput) const noexcept;
    [[nodiscard]] math::Vec2 getBindingVector(const Binding& binding, const Input& input, const VirtualInput& virtualInput) const noexcept;
    [[nodiscard]] float getStrongest(const std::vector<Binding>& list, const Input& input, const VirtualInput& virtualInput) const noexcept;
    [[nodiscard]] const State* find(std::string_view name) const noexcept;
    [[nodiscard]] bool isStickCaptured(std::size_t index, bool right) const noexcept;
    void holdCaptured(const Input& input) noexcept;

    std::vector<State> actions;
    std::optional<std::size_t> gamepadIndex;
    float pressThreshold = 0.5F;
    Capture capture;
    std::bitset<Controls::kKeyCount> capturedKeys;
    std::bitset<Controls::kMouseButtonCount> capturedMouseButtons;
    std::array<std::bitset<Controls::kGamepadButtonCount>, Input::kMaxGamepads> capturedButtons;
    std::array<std::bitset<Controls::kGamepadAxisCount>, Input::kMaxGamepads> capturedAxes;
    std::array<std::bitset<Controls::kGamepadAxisCount>, Input::kMaxGamepads> leaningAxes;
};

} // namespace haylen::input
