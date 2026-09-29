#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "graphics/DeviceSetup.hpp"
#include "graphics/FrameTarget.hpp"
#include "haylen/audio/Mixer.hpp"
#include "haylen/core/Json.hpp"
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

    // The screen edges that native views of plugins reserve, the largest reservation on each edge in framebuffer pixels, which the engine adds to the safe area.
    [[nodiscard]] virtual math::Insets getReservedInsets() const = 0;

    // Whether native UI of plugins covers the app, such as a full screen ad or a sign-in form.
    [[nodiscard]] virtual bool isAppCovered() const = 0;

    // The ids of the plugins whose native part the platform loaded.
    [[nodiscard]] virtual std::vector<std::string> getNativePlugins() const = 0;

    // Hands the error that stopped the app to native code as the JSON report of lua::Error::toJson, with its message, file, line, traceback and frames.
    virtual void reportError(const core::Json& report) = 0;
};

} // namespace haylen::platform
