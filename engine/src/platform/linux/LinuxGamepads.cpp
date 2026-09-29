#include "platform/linux/LinuxGamepads.hpp"

#include <fcntl.h>
#include <linux/joystick.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <string>

namespace haylen::platform {

std::array<LinuxGamepads::Joystick, input::Input::kMaxGamepads>& LinuxGamepads::joysticks = *new std::array<Joystick, input::Input::kMaxGamepads>();
std::chrono::steady_clock::time_point LinuxGamepads::nextScan{};

void LinuxGamepads::scan() {
    for (std::size_t index = 0; index < joysticks.size(); ++index) {
        Joystick& joystick = joysticks[index];
        if (joystick.descriptor >= 0) {
            continue;
        }
        const std::string path = "/dev/input/js" + std::to_string(index);
        joystick.descriptor = open(path.c_str(), O_RDONLY | O_NONBLOCK);
        if (joystick.descriptor < 0) {
            continue;
        }
        std::array<char, 128> label{};
        ioctl(joystick.descriptor, JSIOCGNAME(label.size()), label.data());
        joystick.state = {.connected = true, .name = label.data()};
    }
}

void LinuxGamepads::release() noexcept {
    for (Joystick& joystick : joysticks) {
        if (joystick.descriptor >= 0) {
            close(joystick.descriptor);
        }
        joystick = {};
    }
}

void LinuxGamepads::poll(std::span<input::GamepadState> states) {
    const auto now = std::chrono::steady_clock::now();
    if (now >= nextScan) {
        scan();
        nextScan = now + std::chrono::seconds(1);
    }

    for (std::size_t index = 0; index < states.size(); ++index) {
        Joystick& joystick = joysticks[index];
        while (joystick.descriptor >= 0) {
            js_event event{};
            const ssize_t count = read(joystick.descriptor, &event, sizeof(event));
            if (count == static_cast<ssize_t>(sizeof(event))) {
                apply(joystick.state, event);
                continue;
            }
            // An empty queue ends the frame's events, and any other outcome means the controller went away.
            if (count < 0 && errno == EAGAIN) {
                break;
            }
            close(joystick.descriptor);
            joystick = {};
        }
        states[index] = joystick.state;
    }
}

// The joystick interface numbers controls the way the xpad driver lays them out for standard controllers.
void LinuxGamepads::apply(input::GamepadState& state, const js_event& event) {
    const float value = event.value < 0 ? static_cast<float>(event.value) / 32768.0F : static_cast<float>(event.value) / 32767.0F;
    if ((event.type & ~JS_EVENT_INIT) == JS_EVENT_BUTTON) {
        constexpr std::array<input::GamepadButton, 11> kButtons{input::GamepadButton::South, input::GamepadButton::East, input::GamepadButton::West, input::GamepadButton::North, input::GamepadButton::LeftShoulder, input::GamepadButton::RightShoulder, input::GamepadButton::Back, input::GamepadButton::Start, input::GamepadButton::Guide, input::GamepadButton::LeftStick, input::GamepadButton::RightStick};
        if (event.number < kButtons.size()) {
            state.buttons[static_cast<std::size_t>(kButtons[event.number])] = event.value != 0;
        }
        return;
    }
    switch (event.number) {
    case 0:
        state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftX)] = value;
        break;
    case 1:
        state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftY)] = value;
        break;
    case 2:
        state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftTrigger)] = (value + 1.0F) * 0.5F;
        break;
    case 3:
        state.axes[static_cast<std::size_t>(input::GamepadAxis::RightX)] = value;
        break;
    case 4:
        state.axes[static_cast<std::size_t>(input::GamepadAxis::RightY)] = value;
        break;
    case 5:
        state.axes[static_cast<std::size_t>(input::GamepadAxis::RightTrigger)] = (value + 1.0F) * 0.5F;
        break;
    case 6:
        state.buttons[static_cast<std::size_t>(input::GamepadButton::DpadLeft)] = value < -0.5F;
        state.buttons[static_cast<std::size_t>(input::GamepadButton::DpadRight)] = value > 0.5F;
        break;
    case 7:
        state.buttons[static_cast<std::size_t>(input::GamepadButton::DpadUp)] = value < -0.5F;
        state.buttons[static_cast<std::size_t>(input::GamepadButton::DpadDown)] = value > 0.5F;
        break;
    default:
        break;
    }
}

} // namespace haylen::platform
