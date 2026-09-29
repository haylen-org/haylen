#include "platform/Services.hpp"

#include <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

#include <string>

#include "haylen/io/Package.hpp"
#import "platform/apple/AppleBridge.hpp"
#import "platform/apple/AppleDesktop.hpp"
#import "platform/apple/AppleGamepads.hpp"
#import "platform/apple/AppleNetwork.hpp"
#import "platform/apple/AppleOrientation.hpp"
#import "platform/apple/ApplePlugins.hpp"
#import "platform/apple/AppleTextInput.hpp"
#import "platform/apple/CatalystInput.hpp"
#import "platform/apple/CatalystWindow.hpp"
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
    CatalystWindow::observe();
#endif
}

void Services::shutdown() noexcept {
    AppleBridge::clearHandlers();
}

void Services::reportError(const core::Json& report) {
    ApplePlugins::reportError(report);
}

std::vector<std::string> Services::getNativePlugins() {
    return ApplePlugins::getIds();
}

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

bool Services::hasDesktop() noexcept {
    return TARGET_OS_OSX != 0;
}

void Services::setWindowStyle([[maybe_unused]] const WindowStyle& value) {
#if TARGET_OS_OSX
    AppleDesktop::setStyle(value);
#endif
}

// Windows of iPhones, iPads and TVs fill their screen, which is their only monitor.
math::Rect Services::getWindowFrame() {
#if TARGET_OS_OSX
    return AppleDesktop::getFrame();
#else
    return {0.0F, 0.0F, sapp_widthf() / sapp_dpi_scale(), sapp_heightf() / sapp_dpi_scale()};
#endif
}

void Services::setWindowFrame([[maybe_unused]] const math::Rect& value) {
#if TARGET_OS_OSX
    AppleDesktop::setFrame(value);
#endif
}

std::vector<Monitor> Services::getMonitors() {
#if TARGET_OS_OSX
    return AppleDesktop::getMonitors();
#else
    const math::Rect screen = getWindowFrame();
    return {{.name = "screen", .bounds = screen, .workArea = screen, .scale = sapp_dpi_scale(), .primary = true}};
#endif
}

void Services::setMousePassthrough([[maybe_unused]] Window::Passthrough mode, [[maybe_unused]] std::span<const math::Polygon::Outline> regions) {
#if TARGET_OS_OSX
    AppleDesktop::setPassthrough(mode, regions);
#endif
}

void Services::startWindowDrag() {
#if TARGET_OS_OSX
    AppleDesktop::startDrag();
#endif
}

void Services::watchWindow() {
#if TARGET_OS_OSX
    AppleDesktop::watch();
#endif
}

void Services::updateWindow() {
#if TARGET_OS_OSX
    AppleDesktop::update();
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
    AppleGamepads::poll(gamepads);
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
    static AppleTextInput& input = *new AppleTextInput();
    return input;
}

void Services::dispatch(std::uint64_t call, std::string_view method, std::string_view paramsJson) {
    AppleBridge::dispatch(call, method, paramsJson);
}

void Services::cancel(std::uint64_t call) {
    AppleBridge::cancel(call);
}

} // namespace haylen::platform
