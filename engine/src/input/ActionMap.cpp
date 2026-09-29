#include "haylen/input/ActionMap.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <tuple>
#include <utility>

#include "haylen/core/JsonValidator.hpp"
#include "haylen/input/Controls.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/input/VirtualInput.hpp"

namespace haylen::input {

std::optional<ActionMap::Binding> ActionMap::Binding::parse(std::string_view text) {
    const std::size_t separator = text.find(':');
    if (separator == std::string_view::npos) {
        return std::nullopt;
    }

    const std::string_view kind = text.substr(0, separator);
    std::string_view value = text.substr(separator + 1);
    Binding binding;

    if (kind == "key") {
        const std::optional<Key> found = Controls::keyFromName(value);
        if (!found) {
            return std::nullopt;
        }
        binding.source = Source::Key;
        binding.key = *found;
        return binding;
    }

    if (kind == "mouse") {
        const std::optional<MouseButton> found = Controls::mouseButtonFromName(value);
        if (!found) {
            return std::nullopt;
        }
        binding.source = Source::MouseButton;
        binding.mouseButton = *found;
        return binding;
    }

    if (kind == "button") {
        const std::optional<GamepadButton> found = Controls::gamepadButtonFromName(value);
        if (!found) {
            return std::nullopt;
        }
        binding.source = Source::GamepadButton;
        binding.gamepadButton = *found;
        return binding;
    }

    if (kind == "axis") {
        if (value.empty() || (value.back() != '+' && value.back() != '-')) {
            return std::nullopt;
        }
        binding.direction = value.back() == '+' ? 1.0F : -1.0F;
        value.remove_suffix(1);
        const std::optional<GamepadAxis> found = Controls::gamepadAxisFromName(value);
        if (!found) {
            return std::nullopt;
        }
        binding.source = Source::GamepadAxis;
        binding.gamepadAxis = *found;
        return binding;
    }

    if (kind == "stick") {
        if (value != "left" && value != "right") {
            return std::nullopt;
        }
        binding.source = Source::GamepadStick;
        binding.rightStick = value == "right";
        return binding;
    }

    if ((kind == "virtual" || kind == "virtual_stick") && !value.empty()) {
        binding.source = kind == "virtual" ? Source::VirtualButton : Source::VirtualStick;
        binding.name = std::string(value);
        return binding;
    }
    return std::nullopt;
}

std::string ActionMap::Binding::toString() const {
    switch (source) {
    case Source::Key:
        return "key:" + std::string(Controls::keyName(key));
    case Source::MouseButton:
        return "mouse:" + std::string(Controls::mouseButtonName(mouseButton));
    case Source::GamepadButton:
        return "button:" + std::string(Controls::gamepadButtonName(gamepadButton));
    case Source::GamepadAxis:
        return "axis:" + std::string(Controls::gamepadAxisName(gamepadAxis)) + (direction > 0.0F ? "+" : "-");
    case Source::GamepadStick:
        return rightStick ? "stick:right" : "stick:left";
    case Source::VirtualButton:
        return "virtual:" + name;
    case Source::VirtualStick:
        return "virtual_stick:" + name;
    }
    return {};
}

std::optional<ActionMap::Action::Type> ActionMap::Action::typeFromName(std::string_view text) noexcept {
    if (text == "button") {
        return Type::Button;
    }
    if (text == "axis") {
        return Type::Axis;
    }
    if (text == "vector") {
        return Type::Vector;
    }
    return std::nullopt;
}

std::string_view ActionMap::Action::typeName(Type value) noexcept {
    switch (value) {
    case Type::Button:
        return "button";
    case Type::Axis:
        return "axis";
    case Type::Vector:
        return "vector";
    }
    return "button";
}

ActionMap::Action ActionMap::Action::fromJson(const core::Json& json) {
    core::JsonValidator::requireKnownKeys(json, {"name", "type", "bindings", "positive", "negative", "up", "down", "left", "right"}, "an action");
    const auto nameField = json.find("name");
    const auto typeField = json.find("type");
    if (nameField == json.end() || !nameField->is_string() || typeField == json.end() || !typeField->is_string()) {
        throw std::invalid_argument("An action needs a name and a type.");
    }
    const std::optional<Type> actionType = typeFromName(typeField->get<std::string>());
    if (!actionType) {
        throw std::invalid_argument("Invalid action type: " + typeField->get<std::string>());
    }

    return {
        .name = nameField->get<std::string>(),
        .type = *actionType,
        .bindings = parseBindings(json, "bindings"),
        .positive = parseBindings(json, "positive"),
        .negative = parseBindings(json, "negative"),
        .up = parseBindings(json, "up"),
        .down = parseBindings(json, "down"),
        .left = parseBindings(json, "left"),
        .right = parseBindings(json, "right"),
    };
}

core::Json ActionMap::Action::toJson() const {
    core::Json json = {{"name", name}, {"type", typeName(type)}};
    const std::pair<const char*, const std::vector<Binding>*> fields[] = {
        {"bindings", &bindings}, {"positive", &positive}, {"negative", &negative}, {"up", &up}, {"down", &down}, {"left", &left}, {"right", &right},
    };
    for (const auto& [field, list] : fields) {
        if (!list->empty()) {
            json[field] = saveBindings(*list);
        }
    }
    return json;
}

bool ActionMap::isList(const core::Json& value) noexcept {
    return value.is_array() || (value.is_object() && value.empty());
}

std::vector<ActionMap::Binding> ActionMap::parseBindings(const core::Json& action, const char* field) {
    std::vector<Binding> bindings;
    const auto list = action.find(field);
    if (list == action.end()) {
        return bindings;
    }
    if (!isList(*list)) {
        throw std::invalid_argument(std::string("The ") + field + " of an action must be a list of bindings.");
    }

    for (const core::Json& entry : *list) {
        const std::string text = entry.is_string() ? entry.get<std::string>() : entry.dump();
        const std::optional<Binding> binding = entry.is_string() ? Binding::parse(text) : std::nullopt;
        if (!binding) {
            throw std::invalid_argument("Invalid input binding: " + text);
        }
        bindings.push_back(*binding);
    }
    return bindings;
}

core::Json ActionMap::saveBindings(const std::vector<Binding>& list) {
    core::Json saved = core::Json::array();
    for (const Binding& binding : list) {
        saved.push_back(binding.toString());
    }
    return saved;
}

void ActionMap::validate(const Action& action) {
    const bool axis = action.type == Action::Type::Axis;
    const bool vector = action.type == Action::Type::Vector;
    const std::tuple<const char*, const std::vector<Binding>*, bool> fields[] = {
        {"bindings", &action.bindings, !axis}, {"positive", &action.positive, axis}, {"negative", &action.negative, axis}, {"up", &action.up, vector}, {"down", &action.down, vector}, {"left", &action.left, vector}, {"right", &action.right, vector},
    };
    for (const auto& [field, list, read] : fields) {
        if (!read && !list->empty()) {
            throw std::invalid_argument("The " + std::string(Action::typeName(action.type)) + " action " + action.name + " does not read " + field + ".");
        }
    }

    if (!vector) {
        return;
    }
    for (const Binding& binding : action.bindings) {
        if (binding.source != Binding::Source::GamepadStick && binding.source != Binding::Source::VirtualStick) {
            throw std::invalid_argument("The vector action " + action.name + " takes only sticks in bindings, not " + binding.toString() + ".");
        }
    }
}

void ActionMap::load(const core::Json& document) {
    core::JsonValidator::requireKnownKeys(document, {"actions"}, "the action map");
    const auto list = document.find("actions");
    if (list == document.end() || !isList(*list)) {
        throw std::invalid_argument("The action map needs a list of actions.");
    }

    std::vector<State> loaded;
    for (const core::Json& entry : *list) {
        Action action = Action::fromJson(entry);
        validate(action);
        loaded.push_back(State{.action = std::move(action)});
    }
    actions = std::move(loaded);
}

core::Json ActionMap::save() const {
    core::Json list = core::Json::array();
    for (const State& state : actions) {
        list.push_back(state.action.toJson());
    }
    return {{"actions", std::move(list)}};
}

void ActionMap::define(Action action) {
    validate(action);
    const auto existing = std::find_if(actions.begin(), actions.end(), [&action](const State& state) { return state.action.name == action.name; });
    if (existing != actions.end()) {
        *existing = State{.action = std::move(action)};
        return;
    }
    actions.push_back(State{.action = std::move(action)});
}

void ActionMap::remove(std::string_view name) {
    std::erase_if(actions, [name](const State& state) { return state.action.name == name; });
}

void ActionMap::clear() noexcept {
    actions.clear();
}

const ActionMap::Action* ActionMap::findAction(std::string_view name) const noexcept {
    const State* state = find(name);
    return state == nullptr ? nullptr : &state->action;
}

std::vector<std::string> ActionMap::getNames() const {
    std::vector<std::string> names;
    names.reserve(actions.size());
    for (const State& state : actions) {
        names.push_back(state.action.name);
    }
    return names;
}

float ActionMap::getBindingValue(const Binding& binding, const Input& input, const VirtualInput& virtualInput) const noexcept {
    // clang-format off
    const auto anyGamepad = [&](auto&& read) {
        if (gamepadIndex) {
            return read(*gamepadIndex);
        }
        float strongestValue = 0.0F;
        for (std::size_t index = 0; index < Input::kMaxGamepads; ++index) {
            strongestValue = std::max(strongestValue, read(index));
        }
        return strongestValue;
    };
    // clang-format on

    switch (binding.source) {
    case Binding::Source::Key:
        return input.isKeyDown(binding.key) || input.isKeyPressed(binding.key) ? 1.0F : 0.0F;
    case Binding::Source::MouseButton:
        return !input.isPointerCaptured() && (input.isMouseDown(binding.mouseButton) || input.isMousePressed(binding.mouseButton)) ? 1.0F : 0.0F;
    case Binding::Source::GamepadButton:
        return anyGamepad([&](std::size_t index) { return input.isGamepadDown(index, binding.gamepadButton) ? 1.0F : 0.0F; });
    case Binding::Source::GamepadAxis:
        return anyGamepad([&](std::size_t index) { return std::max(0.0F, input.getGamepadAxis(index, binding.gamepadAxis) * binding.direction); });
    case Binding::Source::VirtualButton:
        return virtualInput.isButtonDown(binding.name) ? 1.0F : 0.0F;
    case Binding::Source::GamepadStick:
    case Binding::Source::VirtualStick:
        return getBindingVector(binding, input, virtualInput).getLength();
    }
    return 0.0F;
}

math::Vec2 ActionMap::getBindingVector(const Binding& binding, const Input& input, const VirtualInput& virtualInput) const noexcept {
    if (binding.source == Binding::Source::VirtualStick) {
        return virtualInput.getStick(binding.name);
    }
    if (binding.source != Binding::Source::GamepadStick) {
        return {};
    }
    if (gamepadIndex) {
        return input.getGamepadStick(*gamepadIndex, binding.rightStick);
    }

    math::Vec2 strongestStick{};
    for (std::size_t index = 0; index < Input::kMaxGamepads; ++index) {
        const math::Vec2 stick = input.getGamepadStick(index, binding.rightStick);
        if (stick.getLengthSquared() > strongestStick.getLengthSquared()) {
            strongestStick = stick;
        }
    }
    return strongestStick;
}

float ActionMap::getStrongest(const std::vector<Binding>& list, const Input& input, const VirtualInput& virtualInput) const noexcept {
    float result = 0.0F;
    for (const Binding& binding : list) {
        result = std::max(result, getBindingValue(binding, input, virtualInput));
    }
    return result;
}

void ActionMap::update(const Input& input, const VirtualInput& virtualInput) {
    for (State& state : actions) {
        const Action& action = state.action;
        const bool wasDown = state.down;

        switch (action.type) {
        case Action::Type::Button:
            state.value = getStrongest(action.bindings, input, virtualInput);
            state.vector = {};
            break;
        case Action::Type::Axis:
            state.value = std::clamp(getStrongest(action.positive, input, virtualInput) - getStrongest(action.negative, input, virtualInput), -1.0F, 1.0F);
            state.vector = {};
            break;
        case Action::Type::Vector: {
            math::Vec2 vector{
                getStrongest(action.right, input, virtualInput) - getStrongest(action.left, input, virtualInput),
                getStrongest(action.down, input, virtualInput) - getStrongest(action.up, input, virtualInput),
            };
            for (const Binding& binding : action.bindings) {
                vector += getBindingVector(binding, input, virtualInput);
            }
            state.vector = vector.clampedLength(1.0F);
            state.value = state.vector.getLength();
            break;
        }
        }

        state.down = std::fabs(state.value) >= pressThreshold;
        state.pressed = state.down && !wasDown;
        state.released = !state.down && wasDown;
    }
}

const ActionMap::State* ActionMap::find(std::string_view name) const noexcept {
    const auto found = std::find_if(actions.begin(), actions.end(), [name](const State& state) { return state.action.name == name; });
    return found == actions.end() ? nullptr : &*found;
}

bool ActionMap::isDown(std::string_view name) const noexcept {
    const State* state = find(name);
    return state != nullptr && state->down;
}

bool ActionMap::isPressed(std::string_view name) const noexcept {
    const State* state = find(name);
    return state != nullptr && state->pressed;
}

bool ActionMap::isReleased(std::string_view name) const noexcept {
    const State* state = find(name);
    return state != nullptr && state->released;
}

float ActionMap::getValue(std::string_view name) const noexcept {
    const State* state = find(name);
    return state == nullptr ? 0.0F : state->value;
}

math::Vec2 ActionMap::getVector(std::string_view name) const noexcept {
    const State* state = find(name);
    return state == nullptr ? math::Vec2{} : state->vector;
}

} // namespace haylen::input
