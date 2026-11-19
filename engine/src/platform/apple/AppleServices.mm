#import "platform/apple/AppleServices.hpp"

#include <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>
#endif

#include <algorithm>
#include <string>
#include <utility>

#include "content/FolderStoreFiles.hpp"
#include "content/ReleasePackage.hpp"
#include "haylen/content/Bootstrap.hpp"
#include "haylen/io/Package.hpp"
#include "io/CompositePackage.hpp"
#include "platform/MemoryWarning.hpp"
#include "platform/NativeWindow.hpp"
#include "platform/Player.hpp"
#import "platform/apple/AppleAccessibility.hpp"
#import "platform/apple/AppleBackgroundAudio.hpp"
#import "platform/apple/AppleBattery.hpp"
#import "platform/apple/AppleBridge.hpp"
#import "platform/apple/AppleCutouts.hpp"
#import "platform/apple/AppleDesktop.hpp"
#import "platform/apple/AppleDesktopService.hpp"
#import "platform/apple/AppleDisplays.hpp"
#import "platform/apple/AppleFold.hpp"
#import "platform/apple/AppleFonts.hpp"
#import "platform/apple/AppleGamepads.hpp"
#import "platform/apple/AppleKeyboard.hpp"
#import "platform/apple/AppleLanguages.hpp"
#import "platform/apple/AppleNetwork.hpp"
#import "platform/apple/AppleOrientation.hpp"
#import "platform/apple/ApplePlugins.hpp"
#import "platform/apple/ApplePowerSaving.hpp"
#import "platform/apple/AppleScreens.hpp"
#import "platform/apple/AppleStatusBar.hpp"
#import "platform/apple/AppleStrings.hpp"
#import "platform/apple/AppleSystem.hpp"
#import "platform/apple/AppleSystemGestures.hpp"
#import "platform/apple/AppleTimeZone.hpp"
#import "platform/apple/AppleVideoDecoder.hpp"
#import "platform/apple/CatalystWindow.hpp"
#import "platform/apple/HaylenAccessibilityView.h"

namespace haylen::platform {

// Only a Mac has a desktop of windows, which its own service follows.
port::Services AppleServices::create() {
    const auto services = std::make_shared<AppleServices>();
    port::Services registered{.platform = services, .displays = services, .screen = services, .bridge = services, .accessibility = services, .video = services, .fonts = services};
#if TARGET_OS_OSX
    registered.desktop = std::make_shared<AppleDesktopService>();
#else
    registered.backgroundAudio = std::make_shared<AppleBackgroundAudio>();
#endif
    return registered;
}

std::string_view AppleServices::getName() const noexcept {
#if TARGET_OS_OSX
    return "macos";
#elif TARGET_OS_TV
    return "tvos";
#else
    return "ios";
#endif
}

void AppleServices::initialize() {
    AppleNetwork::observe();
    AppleBattery::observe();
    ApplePowerSaving::observe();
    AppleTimeZone::observe();
    AppleAccessibility::observe();
    AppleLanguages::observe();
#if TARGET_OS_OSX
    // AppKit sends apps no memory warnings, so the runtime follows the memory pressure of the system with its level.
    static dispatch_source_t pressure = dispatch_source_create(DISPATCH_SOURCE_TYPE_MEMORYPRESSURE, 0, DISPATCH_MEMORYPRESSURE_WARN | DISPATCH_MEMORYPRESSURE_CRITICAL, dispatch_get_main_queue());
    dispatch_source_set_event_handler(pressure, ^{ MemoryWarning::raise((dispatch_source_get_data(pressure) & DISPATCH_MEMORYPRESSURE_CRITICAL) != 0 ? Event::MemoryLevel::Critical : Event::MemoryLevel::Warning); });
    dispatch_resume(pressure);
#else
    // The memory warnings of UIKit tell no level.
    [NSNotificationCenter.defaultCenter addObserverForName:UIApplicationDidReceiveMemoryWarningNotification object:nil queue:nil usingBlock:^(NSNotification*) { MemoryWarning::raise(Event::MemoryLevel::Warning); }];
    [NSNotificationCenter.defaultCenter addObserverForName:AVAudioSessionMediaServicesWereResetNotification object:nil queue:nil usingBlock:^(NSNotification*) { Player::postEvent({.type = Event::Type::AudioServicesReset}); }];
#endif
#if !TARGET_OS_OSX
    AppleKeyboard::observe();
#endif
#if TARGET_OS_MACCATALYST
    CatalystWindow::observe();
#endif
}

void AppleServices::shutdown() noexcept {
    AppleBridge::clearHandlers();
}

void AppleServices::reportError(const core::Json& report) {
    ApplePlugins::reportError(report);
}

std::vector<std::string> AppleServices::getNativePlugins() {
    return ApplePlugins::getIds();
}

std::shared_ptr<io::Package> AppleServices::openBundledPackage() {
    // Bundles keep the package in their resources, and a plain executable keeps it next to itself, which is where the main bundle points too.
    NSString* resources = NSBundle.mainBundle.resourcePath;
    const std::filesystem::path folder = std::filesystem::path(resources.UTF8String) / "app";
    if (std::filesystem::is_directory(folder)) {
        std::shared_ptr<io::Package> files = io::Package::openDirectory(folder);
#if TARGET_OS_OSX
        // The shards of the packs that a store installs on their own, such as the expansions of a desktop game, land in `packs` next to the app, outside its signed bundle.
        const std::filesystem::path packs = std::filesystem::path(NSBundle.mainBundle.bundlePath.UTF8String).parent_path() / "packs";
        if (std::filesystem::is_directory(packs)) {
            files = std::make_shared<io::CompositePackage>("app", std::vector<std::shared_ptr<const io::Package>>{files, io::Package::openDirectory(packs)});
        }
#endif
        return content::ReleasePackage::openBundled(std::move(files), content::Bootstrap::find(), [](std::string_view identifier) { return std::make_unique<content::FolderStoreFiles>(getContentStoreDirectory(identifier)); });
    }

    return io::Package::open(folder.string() + ".zip");
}

std::filesystem::path AppleServices::getUserDataDirectory(std::string_view identifier) {
#if TARGET_OS_TV
    // Apps on tvOS keep no permanent local files, so caches are the only writable place.
    const NSSearchPathDirectory directory = NSCachesDirectory;
#else
    const NSSearchPathDirectory directory = NSApplicationSupportDirectory;
#endif
    NSString* root = NSSearchPathForDirectoriesInDomains(directory, NSUserDomainMask, YES).firstObject;
    return std::filesystem::path(root.UTF8String) / std::string(identifier);
}

// Downloaded content comes back with a download, so it stays out of backups, and tvOS keeps it among the caches, which the system may clear.
std::filesystem::path AppleServices::getContentStoreDirectory(std::string_view identifier) {
#if TARGET_OS_TV
    const NSSearchPathDirectory directory = NSCachesDirectory;
#else
    const NSSearchPathDirectory directory = NSApplicationSupportDirectory;
#endif
    NSString* root = NSSearchPathForDirectoriesInDomains(directory, NSUserDomainMask, YES).firstObject;
    const std::filesystem::path folder = std::filesystem::path(root.UTF8String) / (std::string(identifier) + "-content");
    std::error_code error;
    std::filesystem::create_directories(folder, error);
    NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:folder.c_str()] isDirectory:YES];
    [url setResourceValue:@YES forKey:NSURLIsExcludedFromBackupKey error:nil];
    return folder;
}

void AppleServices::persistUserData() {}

// Windows of iPhones, iPads and TVs fill their screen.
math::Rect AppleServices::getWindowFrame() {
#if TARGET_OS_OSX
    return AppleDesktop::getFrame();
#else
    return {0.0F, 0.0F, NativeWindow::getFramebufferSize().x / NativeWindow::getScale(), NativeWindow::getFramebufferSize().y / NativeWindow::getScale()};
#endif
}

std::vector<Display> AppleServices::getDisplays() {
#if TARGET_OS_OSX
    return AppleDesktop::getDisplays();
#else
    return AppleDisplays::getDisplays();
#endif
}

std::string AppleServices::getWindowDisplayId() {
#if TARGET_OS_OSX
    return AppleDesktop::getWindowDisplayId();
#else
    return AppleDisplays::getWindowDisplayId();
#endif
}

// UIKit tells nothing of an external display whose scene it does not connect, so an iPhone or an iPad without `window.externalDisplays` of `haylen.json` cannot tell that one is there, and no request lists it.
bool AppleServices::hasUnlistedDisplays() {
    return false;
}

void AppleServices::requestEveryScreen(std::function<void(SystemAnswer answer)> callback) {
    callback({.state = SystemAnswer::State::Granted});
}

math::Insets AppleServices::getSafeAreaInsets() {
    const float scale = NativeWindow::getScale();
#if TARGET_OS_OSX
    // The camera housing of a notched display covers the top of its screen, which only a fullscreen window or a borderless window placed there reaches, and a fullscreen window that the system keeps below the housing does not.
    NSWindow* window = (__bridge NSWindow*)NativeWindow::getHandle();
    NSScreen* screen = window.screen;
    if (screen == nil || screen.safeAreaInsets.top <= 0.0) {
        return {};
    }

    const NSRect content = [window contentRectForFrameRect:window.frame];
    const CGFloat covered = NSMaxY(content) - (NSMaxY(screen.frame) - screen.safeAreaInsets.top);
    return {.top = static_cast<float>(std::clamp<CGFloat>(covered, 0.0, NSHeight(content))) * scale};
#else
    UIWindow* window = (__bridge UIWindow*)NativeWindow::getHandle();
    if (window == nil) {
        return {};
    }

    UIEdgeInsets insets = window.safeAreaInsets;
    // The controls of a window of iPadOS sit in its top corner, so the safe area starts below them.
    if (@available(iOS 26.0, tvOS 26.0, visionOS 26.0, *)) {
        insets = [window edgeInsetsForLayoutRegion:[UIViewLayoutRegion safeAreaLayoutRegionWithCornerAdaptation:UIViewLayoutRegionAdaptivityAxisVertical]];
    }

    return {.left = static_cast<float>(insets.left) * scale, .top = static_cast<float>(insets.top) * scale, .right = static_cast<float>(insets.right) * scale, .bottom = static_cast<float>(insets.bottom) * scale};
#endif
}

bool AppleServices::hasPointerDevice() const noexcept {
    return TARGET_OS_TV == 0;
}

// Only iPhones and iPads turn their screen, while Macs, Mac Catalyst windows and TVs count as landscape.
Orientation AppleServices::getOrientation() {
#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
    return AppleOrientation::get();
#else
    return Orientation::Landscape;
#endif
}

void AppleServices::lockOrientation([[maybe_unused]] Orientation value) {
#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
    AppleOrientation::lock(value);
#endif
}

// Only iPhones and iPads show a status bar, while Macs, Mac Catalyst windows and TVs have none over the app.
void AppleServices::setStatusBar([[maybe_unused]] Window::StatusBar value) {
#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
    AppleStatusBar::set(value);
#endif
}

// Only iPhones and iPads take swipes at the edges of the screen and show a home indicator, while Macs, Mac Catalyst windows, TVs and Apple Vision Pro have neither.
void AppleServices::setSystemGestures([[maybe_unused]] Window::SystemGestures value) {
#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
    AppleSystemGestures::set(value);
#endif
}

float AppleServices::getStatusBarHeight() {
#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
    return AppleStatusBar::getHeight();
#else
    return 0.0F;
#endif
}

// Apple devices have no folds, and iPadOS reports none for the windows of an iPad.
// A foldable iPhone reports its fold from iOS 27.1 on, and every other Apple device has none.
std::optional<Fold> AppleServices::getFold() {
#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
    return AppleFold::get();
#else
    return std::nullopt;
#endif
}

// A notched Mac reports the areas beside the housing of its camera, which lies between them along the top of the screen, and a window that reaches the top of the screen has it over its content. iPhones report the regions that elements of the screen occlude, which `AppleCutouts` follows.
void AppleServices::getCutouts(std::vector<math::Rect>& cutouts) {
    cutouts.clear();
#if TARGET_OS_OSX
    NSWindow* window = (__bridge NSWindow*)NativeWindow::getHandle();
    NSScreen* screen = window.screen;
    if (screen == nil || screen.safeAreaInsets.top <= 0.0) {
        return;
    }

    const NSRect left = screen.auxiliaryTopLeftArea;
    const NSRect right = screen.auxiliaryTopRightArea;
    const NSRect housing = NSMakeRect(NSMaxX(left), NSMaxY(screen.frame) - screen.safeAreaInsets.top, NSMinX(right) - NSMaxX(left), screen.safeAreaInsets.top);
    const NSRect content = [window contentRectForFrameRect:window.frame];
    const NSRect covered = NSIntersectionRect(housing, content);
    if (NSIsEmptyRect(covered)) {
        return;
    }

    const float scale = NativeWindow::getScale();
    const auto x = static_cast<float>(NSMinX(covered) - NSMinX(content));
    const auto y = static_cast<float>(NSMaxY(content) - NSMaxY(covered));
    cutouts.push_back({x * scale, y * scale, static_cast<float>(NSWidth(covered)) * scale, static_cast<float>(NSHeight(covered)) * scale});
#elif TARGET_OS_IOS && !TARGET_OS_MACCATALYST
    AppleCutouts::get(cutouts);
#endif
}

// The parameters reach the handler with a VarnDataView of their own for every view, which the decode takes while the bridge still holds the views.
void AppleServices::dispatch(std::uint64_t call, std::string_view method, std::string_view paramsJson, std::span<const std::vector<std::byte>> buffers, std::span<const varn::dataview::DataView>) {
    AppleBridge::dispatch(call, method, paramsJson, buffers);
}

void AppleServices::cancel(std::uint64_t call) {
    AppleBridge::cancel(call);
}

SystemInfo AppleServices::getSystemInfo() {
    return AppleSystem::getInfo();
}

bool AppleServices::isTreeWanted() {
    return HaylenAccessibilityView.shared.wanted;
}

void AppleServices::update(std::span<const AccessibilityElement> elements, std::uint64_t focused) {
    [HaylenAccessibilityView.shared updateElements:elements focused:focused];
}

void AppleServices::announce(std::string_view text, bool interrupt) {
    [HaylenAccessibilityView.shared announce:AppleStrings::toNSString(text) interrupt:interrupt];
}

std::optional<SystemFont> AppleServices::findSystemFont(char32_t codePoint) {
    return AppleFonts::find(codePoint);
}

std::vector<std::uint8_t> AppleServices::readSystemFont(const SystemFont& font) {
    return AppleFonts::read(font);
}

std::unique_ptr<VideoDecoder> AppleServices::openVideo(std::shared_ptr<io::PackageReader> file, std::string_view path) {
    return std::make_unique<AppleVideoDecoder>(std::move(file), path);
}

// Native libraries of the macOS player open the screens they registered before the platform is asked for them.
void AppleServices::openScreen(const ScreenRequest& request) {
    AppleScreens::open(request);
}

void AppleServices::cancelScreen(std::uint64_t id) {
    AppleScreens::cancel(id);
}

} // namespace haylen::platform
