#include "platform/sdl/Gamepads.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace haylen::platform::sdl {

Gamepads::~Gamepads() {
    for (SDL_Gamepad* pad : opened) {
        SDL_CloseGamepad(pad);
    }
}

void Gamepads::handle(const SDL_Event& event) {
    if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
        if (SDL_Gamepad* pad = SDL_OpenGamepad(event.gdevice.which); pad != nullptr) {
            opened.push_back(pad);
        }
        return;
    }
    if (event.type != SDL_EVENT_GAMEPAD_REMOVED) {
        return;
    }

    SDL_Gamepad* pad = SDL_GetGamepadFromID(event.gdevice.which);
    const auto found = std::ranges::find(opened, pad);
    if (pad == nullptr || found == opened.end()) {
        return;
    }
    opened.erase(found);
    SDL_CloseGamepad(pad);
}

void Gamepads::poll(std::span<input::GamepadState> states) {
    connected.assign(opened.begin(), opened.end());
    slots.update(connected);

    for (std::size_t index = 0; index < states.size(); ++index) {
        SDL_Gamepad* pad = find(slots.getController(index));
        if (pad == nullptr) {
            states[index] = {};
            continue;
        }
        read(pad, states[index]);
    }
}

void Gamepads::rumble(std::size_t slot, float low, float high, float seconds) const {
    SDL_Gamepad* pad = find(slots.getController(slot));
    if (pad == nullptr) {
        return;
    }
    const auto strength = [](float value) { return static_cast<Uint16>(std::lround(std::clamp(value, 0.0F, 1.0F) * 65535.0F)); };
    const double milliseconds = std::min(std::round(static_cast<double>(seconds) * 1000.0), static_cast<double>(std::numeric_limits<Uint32>::max()));
    SDL_RumbleGamepad(pad, strength(low), strength(high), static_cast<Uint32>(std::max(milliseconds, 0.0)));
}

input::GamepadLabels Gamepads::getLabels(SDL_GamepadButtonLabel south) noexcept {
    switch (south) {
    case SDL_GAMEPAD_BUTTON_LABEL_A:
        return input::GamepadLabels::Letters;
    case SDL_GAMEPAD_BUTTON_LABEL_B:
        return input::GamepadLabels::SwappedLetters;
    case SDL_GAMEPAD_BUTTON_LABEL_CROSS:
        return input::GamepadLabels::Shapes;
    default:
        return input::GamepadLabels::Unknown;
    }
}

float Gamepads::readStick(Sint16 value) noexcept {
    return value < 0 ? static_cast<float>(value) / 32768.0F : static_cast<float>(value) / 32767.0F;
}

float Gamepads::readTrigger(Sint16 value) noexcept {
    return std::clamp(static_cast<float>(value) / 32767.0F, 0.0F, 1.0F);
}

SDL_Gamepad* Gamepads::find(const void* identity) const noexcept {
    const auto found = std::ranges::find_if(opened, [identity](const SDL_Gamepad* pad) { return pad == identity; });
    return found != opened.end() ? *found : nullptr;
}

void Gamepads::read(SDL_Gamepad* pad, input::GamepadState& state) {
    const char* name = SDL_GetGamepadName(pad);
    state.connected = true;
    state.name = name != nullptr ? name : "Controller";
    state.labels = getLabels(SDL_GetGamepadButtonLabel(pad, SDL_GAMEPAD_BUTTON_SOUTH));
    for (const auto& [source, button] : kButtons) {
        state.buttons[static_cast<std::size_t>(button)] = SDL_GetGamepadButton(pad, source);
    }
    state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftX)] = readStick(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX));
    state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftY)] = readStick(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY));
    state.axes[static_cast<std::size_t>(input::GamepadAxis::RightX)] = readStick(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX));
    state.axes[static_cast<std::size_t>(input::GamepadAxis::RightY)] = readStick(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY));
    state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftTrigger)] = readTrigger(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
    state.axes[static_cast<std::size_t>(input::GamepadAxis::RightTrigger)] = readTrigger(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
}

} // namespace haylen::platform::sdl
