#include "haylen/input/Controls.hpp"

namespace haylen::input {

const std::array<std::pair<std::string_view, Key>, 120> Controls::kKeyNames = {{
    {"space", Key::Space}, {"apostrophe", Key::Apostrophe}, {"comma", Key::Comma}, {"minus", Key::Minus}, {"period", Key::Period}, {"slash", Key::Slash}, {"0", Key::Num0}, {"1", Key::Num1}, {"2", Key::Num2}, {"3", Key::Num3}, {"4", Key::Num4}, {"5", Key::Num5}, {"6", Key::Num6}, {"7", Key::Num7}, {"8", Key::Num8}, {"9", Key::Num9}, {"semicolon", Key::Semicolon}, {"equal", Key::Equal}, {"a", Key::A}, {"b", Key::B}, {"c", Key::C}, {"d", Key::D}, {"e", Key::E}, {"f", Key::F}, {"g", Key::G}, {"h", Key::H}, {"i", Key::I}, {"j", Key::J}, {"k", Key::K}, {"l", Key::L}, {"m", Key::M}, {"n", Key::N}, {"o", Key::O}, {"p", Key::P}, {"q", Key::Q}, {"r", Key::R}, {"s", Key::S}, {"t", Key::T}, {"u", Key::U}, {"v", Key::V}, {"w", Key::W}, {"x", Key::X}, {"y", Key::Y}, {"z", Key::Z}, {"leftBracket", Key::LeftBracket}, {"backslash", Key::Backslash}, {"rightBracket", Key::RightBracket}, {"graveAccent", Key::GraveAccent}, {"world1", Key::World1}, {"world2", Key::World2}, {"escape", Key::Escape}, {"enter", Key::Enter}, {"tab", Key::Tab}, {"backspace", Key::Backspace}, {"insert", Key::Insert}, {"delete", Key::Delete}, {"right", Key::Right}, {"left", Key::Left}, {"down", Key::Down}, {"up", Key::Up}, {"pageUp", Key::PageUp}, {"pageDown", Key::PageDown}, {"home", Key::Home}, {"end", Key::End}, {"capsLock", Key::CapsLock}, {"scrollLock", Key::ScrollLock}, {"numLock", Key::NumLock}, {"printScreen", Key::PrintScreen}, {"pause", Key::Pause}, {"f1", Key::F1}, {"f2", Key::F2}, {"f3", Key::F3}, {"f4", Key::F4}, {"f5", Key::F5}, {"f6", Key::F6}, {"f7", Key::F7}, {"f8", Key::F8}, {"f9", Key::F9}, {"f10", Key::F10}, {"f11", Key::F11}, {"f12", Key::F12}, {"f13", Key::F13}, {"f14", Key::F14}, {"f15", Key::F15}, {"f16", Key::F16}, {"f17", Key::F17}, {"f18", Key::F18}, {"f19", Key::F19}, {"f20", Key::F20}, {"f21", Key::F21}, {"f22", Key::F22}, {"f23", Key::F23}, {"f24", Key::F24}, {"f25", Key::F25}, {"keypad0", Key::Keypad0}, {"keypad1", Key::Keypad1}, {"keypad2", Key::Keypad2}, {"keypad3", Key::Keypad3}, {"keypad4", Key::Keypad4}, {"keypad5", Key::Keypad5}, {"keypad6", Key::Keypad6}, {"keypad7", Key::Keypad7}, {"keypad8", Key::Keypad8}, {"keypad9", Key::Keypad9}, {"keypadDecimal", Key::KeypadDecimal}, {"keypadDivide", Key::KeypadDivide}, {"keypadMultiply", Key::KeypadMultiply}, {"keypadSubtract", Key::KeypadSubtract}, {"keypadAdd", Key::KeypadAdd}, {"keypadEnter", Key::KeypadEnter}, {"keypadEqual", Key::KeypadEqual}, {"leftShift", Key::LeftShift}, {"leftControl", Key::LeftControl}, {"leftAlt", Key::LeftAlt}, {"leftSuper", Key::LeftSuper}, {"rightShift", Key::RightShift}, {"rightControl", Key::RightControl}, {"rightAlt", Key::RightAlt}, {"rightSuper", Key::RightSuper}, {"menu", Key::Menu},
}};

const std::array<std::string_view, Controls::kMouseButtonCount> Controls::kMouseButtonNames = {"left", "right", "middle"};

const std::array<std::string_view, Controls::kGamepadButtonCount> Controls::kGamepadButtonNames = {
    "south", "east", "west", "north", "leftShoulder", "rightShoulder", "back", "start", "guide", "leftStick", "rightStick", "dpadUp", "dpadDown", "dpadLeft", "dpadRight",
};

const std::array<std::string_view, Controls::kGamepadAxisCount> Controls::kGamepadAxisNames = {
    "leftX", "leftY", "rightX", "rightY", "leftTrigger", "rightTrigger",
};

template <typename Enum, std::size_t Size> std::optional<Enum> Controls::findIndexed(const std::array<std::string_view, Size>& names, std::string_view name) noexcept {
    for (std::size_t index = 0; index < names.size(); ++index) {
        if (names[index] == name) {
            return static_cast<Enum>(index);
        }
    }
    return std::nullopt;
}

std::optional<Key> Controls::keyFromName(std::string_view name) noexcept {
    for (const auto& [candidate, key] : kKeyNames) {
        if (candidate == name) {
            return key;
        }
    }
    return std::nullopt;
}

std::string_view Controls::keyName(Key key) noexcept {
    for (const auto& [name, candidate] : kKeyNames) {
        if (candidate == key) {
            return name;
        }
    }
    return "unknown";
}

std::optional<MouseButton> Controls::mouseButtonFromName(std::string_view name) noexcept {
    return findIndexed<MouseButton>(kMouseButtonNames, name);
}

std::string_view Controls::mouseButtonName(MouseButton button) noexcept {
    return kMouseButtonNames[static_cast<std::size_t>(button)];
}

std::optional<GamepadButton> Controls::gamepadButtonFromName(std::string_view name) noexcept {
    return findIndexed<GamepadButton>(kGamepadButtonNames, name);
}

std::string_view Controls::gamepadButtonName(GamepadButton button) noexcept {
    return kGamepadButtonNames[static_cast<std::size_t>(button)];
}

std::optional<GamepadAxis> Controls::gamepadAxisFromName(std::string_view name) noexcept {
    return findIndexed<GamepadAxis>(kGamepadAxisNames, name);
}

std::string_view Controls::gamepadAxisName(GamepadAxis axis) noexcept {
    return kGamepadAxisNames[static_cast<std::size_t>(axis)];
}

} // namespace haylen::input
