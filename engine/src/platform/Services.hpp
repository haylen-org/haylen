#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>

#include "haylen/input/GamepadState.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/platform/Orientation.hpp"
#include "haylen/platform/TextInput.hpp"

namespace haylen::io {
class Package;
}

namespace haylen::lua {
class Error;
}

namespace haylen::platform {

// Services every platform folder implements for the Sokol runtime. Exactly one implementation is compiled into a runtime build.
class Services final {
  public:
    [[nodiscard]] static std::string_view getName() noexcept;

    static void initialize();
    static void shutdown() noexcept;

    // Tells the host environment about a failure that stopped the app, beyond the log and the error screen. The web runtime passes it to the page with its stack.
    static void reportError(const lua::Error& error);

    // Opens the package that ships with the app when the command line names none.
    [[nodiscard]] static std::shared_ptr<io::Package> openBundledPackage();

    [[nodiscard]] static std::filesystem::path getUserDataDirectory(std::string_view identifier);
    static void persistUserData();

    // Lets the player resize the window, or keeps it at its size. Platforms whose window always fills the screen ignore it.
    static void setWindowResizable(bool value);

    // Returns the insets of the screen area that UI must avoid, in framebuffer pixels.
    [[nodiscard]] static math::Insets getSafeAreaInsets();

    // Whether the device has a mouse or a touch screen, which TVs lack.
    [[nodiscard]] static bool hasPointerDevice() noexcept;

    static void pollGamepads(std::span<input::GamepadState> gamepads);

    // Returns Landscape or Portrait. Desktop windows always count as landscape.
    [[nodiscard]] static Orientation getOrientation();
    static void lockOrientation(Orientation value);

    // Returns the text input of the platform, which lives as long as the process.
    [[nodiscard]] static TextInput& getTextInput();

    // Hands a call to the native handler registry, which answers through the bridge relay. Methods without a handler fail with an error result.
    static void dispatch(std::uint64_t id, std::string_view method, std::string_view paramsJson);
};

} // namespace haylen::platform
