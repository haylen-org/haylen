#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/input/GamepadButton.hpp"
#include "haylen/input/GamepadState.hpp"
#include "platform/GamepadSlots.hpp"

namespace haylen::platform::worker {

// The controllers of the Gamepad API, which only the page reads, each one in the slot it took when it connected. The page posts the controllers that the browser lays out in the standard mapping of the Gamepad API, whose buttons and axes have fixed places, and the browser gives the others no layout the engine could read.
class Gamepads final {
  public:
    static constexpr std::size_t kButtonCount = 17;
    static constexpr std::size_t kAxisCount = 4;

    // The index of the browser for one controller while it stays connected, the `id` the browser describes it with, the values of its buttons from 0 to 1 and the positions of its sticks from -1 to 1, with down and right positive.
    struct Record {
        std::size_t index = 0;
        std::string_view id;
        std::array<float, kButtonCount> buttons{};
        std::array<float, kAxisCount> axes{};
    };

    // Takes the controllers connected now, and keeps every slot of a controller that stays connected.
    void update(std::span<const Record> records);

    void poll(std::span<input::GamepadState> states) const;

    // Returns the index of the browser of the controller in a slot, or nothing while the slot is free.
    [[nodiscard]] std::optional<std::size_t> getIndex(std::size_t slot) const noexcept;

    // Returns the USB vendor id that the browser writes into the `id` of a controller, as `Vendor: 054c` in Chromium and as `054c-` at its start in Firefox and Safari, and the vendor of the controllers of XInput, which Chromium names by that API alone.
    [[nodiscard]] static std::optional<std::uint16_t> findVendor(std::string_view id) noexcept;

  private:
    // The most indices of the browser that the page follows, which no browser exceeds, since browsers keep few controllers.
    static constexpr std::size_t kMaxIndices = 16;
    static constexpr std::uint16_t kXInputVendor = 0x045E;

    static constexpr std::array<std::pair<std::size_t, input::GamepadButton>, 15> kButtons{{
        {0, input::GamepadButton::South},
        {1, input::GamepadButton::East},
        {2, input::GamepadButton::West},
        {3, input::GamepadButton::North},
        {4, input::GamepadButton::LeftShoulder},
        {5, input::GamepadButton::RightShoulder},
        {8, input::GamepadButton::Back},
        {9, input::GamepadButton::Start},
        {10, input::GamepadButton::LeftStick},
        {11, input::GamepadButton::RightStick},
        {12, input::GamepadButton::DpadUp},
        {13, input::GamepadButton::DpadDown},
        {14, input::GamepadButton::DpadLeft},
        {15, input::GamepadButton::DpadRight},
        {16, input::GamepadButton::Guide},
    }};
    static constexpr std::size_t kLeftTrigger = 6;
    static constexpr std::size_t kRightTrigger = 7;

    [[nodiscard]] static std::optional<std::uint16_t> readHex(std::string_view text) noexcept;
    static void read(const Record& record, input::GamepadState& state);

    // The identity of each index of the browser, which the slots tell controllers apart by.
    std::array<std::uint8_t, kMaxIndices> identities{};
    std::vector<const void*> connected;
    std::vector<Record> current;
    GamepadSlots slots;
};

} // namespace haylen::platform::worker
