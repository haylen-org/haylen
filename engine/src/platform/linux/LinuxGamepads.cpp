#include "platform/linux/LinuxGamepads.hpp"

#include <fcntl.h>
#include <linux/input.h>
#include <linux/joystick.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

#include "platform/GamepadVendors.hpp"

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
        joystick.descriptor = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (joystick.descriptor < 0) {
            continue;
        }
        std::array<char, 128> label{};
        ioctl(joystick.descriptor, JSIOCGNAME(label.size()), label.data());
        joystick.state = {.connected = true, .name = label.data(), .labels = readLabels(index)};
    }
}

std::string LinuxGamepads::getDeviceFolder(std::size_t slot) {
    return "/sys/class/input/js" + std::to_string(slot) + "/device";
}

input::GamepadLabels LinuxGamepads::readLabels(std::size_t slot) {
    std::ifstream file(getDeviceFolder(slot) + "/id/vendor");
    unsigned int vendor = 0;
    if (!(file >> std::hex >> vendor) || vendor > std::numeric_limits<std::uint16_t>::max()) {
        return input::GamepadLabels::Unknown;
    }
    return GamepadVendors::getLabels(static_cast<std::uint16_t>(vendor));
}

// The event device of a controller sits next to its joystick node in the folder of the device.
int LinuxGamepads::openFeedback(std::size_t slot) {
    std::error_code error;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(getDeviceFolder(slot), error)) {
        const std::string name = entry.path().filename().string();
        if (name.starts_with("event")) {
            return ::open(("/dev/input/" + name).c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
        }
    }
    return -1;
}

void LinuxGamepads::close(Joystick& joystick) noexcept {
    for (const int descriptor : {joystick.descriptor, joystick.feedback}) {
        if (descriptor >= 0) {
            ::close(descriptor);
        }
    }
    joystick = {};
}

void LinuxGamepads::release() noexcept {
    for (Joystick& joystick : joysticks) {
        close(joystick);
    }
}

void LinuxGamepads::rumble(std::size_t slot, float low, float high, float seconds) {
    if (slot >= joysticks.size() || joysticks[slot].descriptor < 0) {
        return;
    }
    Joystick& joystick = joysticks[slot];
    if (joystick.feedback < 0) {
        joystick.feedback = openFeedback(slot);
        if (joystick.feedback < 0) {
            return;
        }
    }

    if ((joystick.effect >= 0 && !play(joystick.feedback, joystick.effect, false)) || (low <= 0.0F && high <= 0.0F)) {
        return;
    }
    ff_effect effect{};
    effect.type = FF_RUMBLE;
    effect.id = static_cast<std::int16_t>(joystick.effect);
    effect.u.rumble.strong_magnitude = static_cast<std::uint16_t>(std::lround(low * 65535.0F));
    effect.u.rumble.weak_magnitude = static_cast<std::uint16_t>(std::lround(high * 65535.0F));
    effect.replay.length = static_cast<std::uint16_t>(std::clamp(std::lround(seconds * 1000.0F), 1L, 65535L));
    if (ioctl(joystick.feedback, EVIOCSFF, &effect) < 0) {
        return;
    }
    joystick.effect = effect.id;
    play(joystick.feedback, effect.id, true);
}

bool LinuxGamepads::play(int descriptor, int effect, bool started) {
    input_event event{};
    event.type = EV_FF;
    event.code = static_cast<std::uint16_t>(effect);
    event.value = started ? 1 : 0;
    return write(descriptor, &event, sizeof(event)) == static_cast<ssize_t>(sizeof(event));
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
            // An empty queue ends the frame's events, a signal that interrupted the read tries again, and any other outcome means the controller went away.
            if (count < 0 && errno == EINTR) {
                continue;
            }
            if (count < 0 && errno == EAGAIN) {
                break;
            }
            close(joystick);
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
