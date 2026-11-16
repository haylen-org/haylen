#include "platform/worker/Gamepads.hpp"

#include <algorithm>
#include <charconv>

#include "haylen/input/GamepadAxis.hpp"
#include "platform/GamepadVendors.hpp"

namespace haylen::platform::worker {

void Gamepads::update(std::span<const Record> records) {
    current.clear();
    connected.clear();

    for (const Record& record : records) {
        if (record.index >= kMaxIndices) {
            continue;
        }

        current.push_back(record);
        connected.push_back(&identities[record.index]);
    }

    slots.update(connected);
}

void Gamepads::poll(std::span<input::GamepadState> states) const {
    for (std::size_t slot = 0; slot < states.size(); ++slot) {
        const std::optional<std::size_t> index = getIndex(slot);
        const auto found = index ? std::ranges::find(current, *index, &Record::index) : current.end();
        if (found == current.end()) {
            states[slot] = {};
            continue;
        }

        read(*found, states[slot]);
    }
}

std::optional<std::size_t> Gamepads::getIndex(std::size_t slot) const noexcept {
    const void* controller = slots.getController(slot);
    if (controller == nullptr) {
        return std::nullopt;
    }

    return static_cast<std::size_t>(static_cast<const std::uint8_t*>(controller) - identities.data());
}

std::optional<std::uint16_t> Gamepads::findVendor(std::string_view id) noexcept {
    constexpr std::string_view kVendor = "Vendor: ";
    if (const std::size_t at = id.find(kVendor); at != std::string_view::npos) {
        return readHex(id.substr(at + kVendor.size(), 4));
    }

    if (id.size() > 4 && id[4] == '-') {
        return readHex(id.substr(0, 4));
    }

    if (id.find("XInput") != std::string_view::npos) {
        return kXInputVendor;
    }

    return std::nullopt;
}

std::optional<std::uint16_t> Gamepads::readHex(std::string_view text) noexcept {
    std::uint16_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, 16);
    if (error != std::errc{} || end != text.data() + text.size() || text.size() != 4) {
        return std::nullopt;
    }

    return value;
}

void Gamepads::read(const Record& record, input::GamepadState& state) {
    const std::optional<std::uint16_t> vendor = findVendor(record.id);
    state.connected = true;
    state.name.assign(record.id);
    state.labels = vendor ? GamepadVendors::getLabels(*vendor) : input::GamepadLabels::Unknown;

    for (const auto& [place, button] : kButtons) {
        state.buttons[static_cast<std::size_t>(button)] = record.buttons[place] >= 0.5F;
    }

    state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftX)] = std::clamp(record.axes[0], -1.0F, 1.0F);
    state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftY)] = std::clamp(record.axes[1], -1.0F, 1.0F);
    state.axes[static_cast<std::size_t>(input::GamepadAxis::RightX)] = std::clamp(record.axes[2], -1.0F, 1.0F);
    state.axes[static_cast<std::size_t>(input::GamepadAxis::RightY)] = std::clamp(record.axes[3], -1.0F, 1.0F);
    state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftTrigger)] = std::clamp(record.buttons[kLeftTrigger], 0.0F, 1.0F);
    state.axes[static_cast<std::size_t>(input::GamepadAxis::RightTrigger)] = std::clamp(record.buttons[kRightTrigger], 0.0F, 1.0F);
}

} // namespace haylen::platform::worker
