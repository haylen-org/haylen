#include "platform/Services.hpp"

#import <GameController/GameController.h>
#include <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

#include <string>

#include "haylen/io/Package.hpp"
#import "platform/apple/AppleBridge.hpp"
#import "platform/apple/AppleNetwork.hpp"
#import "platform/apple/AppleOrientation.hpp"
#import "platform/apple/AppleRemote.hpp"
#import "platform/apple/AppleTextInput.hpp"
#import "platform/apple/CatalystInput.hpp"
#include "platform/sokol/MemoryWarning.hpp"
#include "sokol_app.h"

namespace haylen::platform {

std::string_view Services::getName() noexcept {
#if TARGET_OS_OSX
    return "macos";
#elif TARGET_OS_TV
    return "tvos";
#else
    return "ios";
#endif
}

void Services::initialize() {
    AppleBridge::registerBuiltIns();
    AppleNetwork::observe();
#if !TARGET_OS_OSX
    [NSNotificationCenter.defaultCenter addObserverForName:UIApplicationDidReceiveMemoryWarningNotification object:nil queue:nil usingBlock:^(NSNotification*) { MemoryWarning::raise(); }];
#endif
#if TARGET_OS_MACCATALYST
    CatalystInput::observe();
#endif
}

void Services::shutdown() noexcept {
    AppleBridge::clearHandlers();
}

void Services::reportError(const lua::Error&) {}

std::shared_ptr<io::Package> Services::openBundledPackage() {
    // Bundles keep the package in their resources, and a plain executable keeps it next to itself, which is where the main bundle points too.
    NSString* resources = NSBundle.mainBundle.resourcePath;
    const std::filesystem::path folder = std::filesystem::path(resources.UTF8String) / "app";
    if (std::filesystem::is_directory(folder)) {
        return io::Package::open(folder);
    }
    return io::Package::open(folder.string() + ".zip");
}

std::filesystem::path Services::getUserDataDirectory(std::string_view identifier) {
#if TARGET_OS_TV
    // tvOS keeps no permanent local files, so caches are the only writable place.
    const NSSearchPathDirectory directory = NSCachesDirectory;
#else
    const NSSearchPathDirectory directory = NSApplicationSupportDirectory;
#endif
    NSString* root = NSSearchPathForDirectoriesInDomains(directory, NSUserDomainMask, YES).firstObject;
    return std::filesystem::path(root.UTF8String) / std::string(identifier);
}

void Services::persistUserData() {}

void Services::setWindowResizable([[maybe_unused]] bool value) {
#if TARGET_OS_OSX
    NSWindow* window = (__bridge NSWindow*)sapp_macos_get_window();
    window.styleMask = value ? (window.styleMask | NSWindowStyleMaskResizable) : (window.styleMask & ~NSWindowStyleMaskResizable);
#endif
}

math::Insets Services::getSafeAreaInsets() {
    const float scale = sapp_dpi_scale();
#if TARGET_OS_OSX
    // Only fullscreen windows reach the camera housing of notched displays.
    NSWindow* window = (__bridge NSWindow*)sapp_macos_get_window();
    if (window == nil || (window.styleMask & NSWindowStyleMaskFullScreen) == 0) {
        return {};
    }
    const NSEdgeInsets insets = window.screen.safeAreaInsets;
    return {.left = static_cast<float>(insets.left) * scale, .top = static_cast<float>(insets.top) * scale, .right = static_cast<float>(insets.right) * scale, .bottom = static_cast<float>(insets.bottom) * scale};
#else
    UIWindow* window = (__bridge UIWindow*)sapp_ios_get_window();
    if (window == nil) {
        return {};
    }
    const UIEdgeInsets insets = window.safeAreaInsets;
    return {.left = static_cast<float>(insets.left) * scale, .top = static_cast<float>(insets.top) * scale, .right = static_cast<float>(insets.right) * scale, .bottom = static_cast<float>(insets.bottom) * scale};
#endif
}

bool Services::hasPointerDevice() noexcept {
    return TARGET_OS_TV == 0;
}

void Services::pollGamepads(std::span<input::GamepadState> gamepads) {
    NSArray<GCController*>* controllers = GCController.controllers;
    for (std::size_t index = 0; index < gamepads.size(); ++index) {
        input::GamepadState& state = gamepads[index];
        GCController* controller = index < controllers.count ? controllers[index] : nil;
        GCExtendedGamepad* pad = controller.extendedGamepad;
        GCMicroGamepad* remote = pad == nil ? controller.microGamepad : nil;
        if (pad == nil && remote == nil) {
            state = {};
            continue;
        }

        state.connected = true;
        NSString* vendor = controller.vendorName;
        state.name = vendor != nil ? vendor.UTF8String : "Controller";
        if (remote != nil) {
            state.buttons = {};
            state.axes = {};
            AppleRemote::read(remote, state);
            continue;
        }
        const auto set = [&state](input::GamepadButton button, GCControllerButtonInput* control) { state.buttons[static_cast<std::size_t>(button)] = control != nil && control.pressed; };
        set(input::GamepadButton::South, pad.buttonA);
        set(input::GamepadButton::East, pad.buttonB);
        set(input::GamepadButton::West, pad.buttonX);
        set(input::GamepadButton::North, pad.buttonY);
        set(input::GamepadButton::LeftShoulder, pad.leftShoulder);
        set(input::GamepadButton::RightShoulder, pad.rightShoulder);
        set(input::GamepadButton::Back, pad.buttonOptions);
        set(input::GamepadButton::Start, pad.buttonMenu);
        set(input::GamepadButton::Guide, pad.buttonHome);
        set(input::GamepadButton::LeftStick, pad.leftThumbstickButton);
        set(input::GamepadButton::RightStick, pad.rightThumbstickButton);
        set(input::GamepadButton::DpadUp, pad.dpad.up);
        set(input::GamepadButton::DpadDown, pad.dpad.down);
        set(input::GamepadButton::DpadLeft, pad.dpad.left);
        set(input::GamepadButton::DpadRight, pad.dpad.right);

        // GameController reports up as positive, while the engine follows the screen with down as positive.
        state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftX)] = pad.leftThumbstick.xAxis.value;
        state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftY)] = -pad.leftThumbstick.yAxis.value;
        state.axes[static_cast<std::size_t>(input::GamepadAxis::RightX)] = pad.rightThumbstick.xAxis.value;
        state.axes[static_cast<std::size_t>(input::GamepadAxis::RightY)] = -pad.rightThumbstick.yAxis.value;
        state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftTrigger)] = pad.leftTrigger.value;
        state.axes[static_cast<std::size_t>(input::GamepadAxis::RightTrigger)] = pad.rightTrigger.value;
    }
}

// Only iPhones and iPads turn their screen, while Macs, Mac Catalyst windows and TVs count as landscape.
Orientation Services::getOrientation() {
#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
    return AppleOrientation::get();
#else
    return Orientation::Landscape;
#endif
}

void Services::lockOrientation([[maybe_unused]] Orientation value) {
#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
    AppleOrientation::lock(value);
#endif
}

TextInput& Services::getTextInput() {
    static AppleTextInput input;
    return input;
}

void Services::dispatch(std::uint64_t call, std::string_view method, std::string_view paramsJson) {
    AppleBridge::dispatch(call, method, paramsJson);
}

} // namespace haylen::platform
