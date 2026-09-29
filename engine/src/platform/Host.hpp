#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>

#include "graphics/DeviceSetup.hpp"
#include "graphics/FrameTarget.hpp"
#include "haylen/audio/Mixer.hpp"
#include "haylen/input/GamepadState.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/platform/Window.hpp"

namespace haylen::platform {

// Boundary between the portable engine and a platform. The Sokol runtime implements it for real devices and the headless host implements it for tests.
class Host : public Window {
  public:
    [[nodiscard]] virtual std::string_view getPlatformName() const noexcept = 0;
    [[nodiscard]] virtual graphics::DeviceSetup getGraphicsSetup() = 0;
    [[nodiscard]] virtual audio::Mixer::Setup getAudioSetup() const = 0;
    [[nodiscard]] virtual graphics::FrameTarget getFrameTarget() = 0;
    [[nodiscard]] virtual std::filesystem::path getUserDataDirectory(std::string_view identifier) = 0;
    virtual void persistUserData() = 0;
    [[nodiscard]] virtual math::Insets getSafeAreaInsets() const = 0;
    virtual void pollGamepads(std::span<input::GamepadState> gamepads) = 0;
    virtual void dispatchPlatformCall(std::uint64_t id, std::string_view method, std::string_view paramsJson) = 0;
    virtual void cancelPlatformCall(std::uint64_t id) = 0;
};

} // namespace haylen::platform
