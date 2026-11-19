#include "platform/android/AndroidServices.hpp"

#include <android/multinetwork.h>
#include <dlfcn.h>

#include <cstdint>
#include <span>
#include <string>
#include <utility>

#include "content/FolderStoreFiles.hpp"
#include "content/ReleasePackage.hpp"
#include "haylen/content/Bootstrap.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/io/Package.hpp"
#include "platform/NativeWindow.hpp"
#include "platform/SystemFontReader.hpp"
#include "platform/android/AndroidActivity.hpp"
#include "platform/android/AndroidAssetPackage.hpp"
#include "platform/android/AndroidAssetReader.hpp"
#include "platform/android/AndroidDeviceInfo.hpp"
#include "platform/android/AndroidDisplays.hpp"
#include "platform/android/AndroidFonts.hpp"
#include "platform/android/AndroidGamepads.hpp"
#include "platform/android/AndroidMouseWheel.hpp"
#include "platform/android/AndroidVideoDecoder.hpp"
#include "platform/android/JavaBridge.hpp"
#include "varn/http/HttpClientFailureNote.h"

namespace haylen::platform {

port::Services AndroidServices::create() {
    const auto services = std::make_shared<AndroidServices>();
    return {.platform = services, .displays = services, .screen = services, .bridge = services, .permissions = services, .network = services, .media = services, .accessibility = services, .video = services, .fonts = services};
}

std::string_view AndroidServices::getName() const noexcept {
    return "android";
}

// Every activity of the process starts the runtime anew, and the first one may have ended before.
// Android drops the packets of a request to the local network while the app does not hold the permission for it, so the HTTP requests that fail say so while the person has not granted it, which is the state of the permission and no guess about the address.
void AndroidServices::initialize() {
    AndroidActivity::holdSplashScreen();
    AndroidGamepads::enableAxes();
    AndroidMouseWheel::enableAxes();

    // clang-format off
    varn::http::client::FailureNote::install([](const std::string&) {
        return JavaBridge::isPermissionHeld(Permission::Kind::LocalNetwork) ? std::string() : std::string("Access to the local network is not granted, so Android blocks a request to the local network until \"system.requestPermission('localNetwork')\" grants it.");
    });
    // clang-format on
}

void AndroidServices::shutdown() noexcept {}

// Script errors may quote bytes that are not UTF-8, which reach the plugins replaced instead of failing the report.
void AndroidServices::reportError(const core::Json& report) {
    JavaBridge::reportError(report.dump(-1, ' ', false, core::Json::error_handler_t::replace));
}

std::vector<std::string> AndroidServices::getNativePlugins() {
    return JavaBridge::getPlugins();
}

// A protected release is a flat folder of manifests and shards, while a development build ships the package with its index.
std::shared_ptr<io::Package> AndroidServices::openBundledPackage() {
    AAssetManager* manager = AndroidActivity::getNative().assetManager;
    const AndroidAssetReader::Handle manifest(AAssetManager_open(manager, ("app/" + std::string(content::ReleasePackage::kAppManifestFile)).c_str(), AASSET_MODE_UNKNOWN), &AAsset_close);
    if (manifest != nullptr) {
        return content::ReleasePackage::openBundled(AndroidAssetPackage::openFlat(manager, "app"), content::Bootstrap::find(), [](std::string_view identifier) { return std::make_unique<content::FolderStoreFiles>(getContentStoreDirectory(identifier)); });
    }

    return AndroidAssetPackage::openIndexed(manager, "app");
}

std::filesystem::path AndroidServices::getUserDataDirectory(std::string_view identifier) {
    return std::filesystem::path(AndroidActivity::getNative().internalDataPath) / std::string(identifier);
}

// Downloaded content comes back with a download, so it lives in the folder of the app that Android leaves out of its backups, next to the files folder.
std::filesystem::path AndroidServices::getContentStoreDirectory(std::string_view identifier) {
    return std::filesystem::path(AndroidActivity::getNative().internalDataPath).parent_path() / "no_backup" / (std::string(identifier) + "-content");
}

void AndroidServices::persistUserData() {}

// The window of the activity fills its display.
math::Rect AndroidServices::getWindowFrame() {
    return {0.0F, 0.0F, NativeWindow::getFramebufferSize().x / NativeWindow::getScale(), NativeWindow::getFramebufferSize().y / NativeWindow::getScale()};
}

std::vector<Display> AndroidServices::getDisplays() {
    return AndroidDisplays::getDisplays();
}

std::string AndroidServices::getWindowDisplayId() {
    return AndroidDisplays::getWindowDisplayId();
}

// Every display of the device is in the list, so none waits for the permission "screens".
bool AndroidServices::hasUnlistedDisplays() {
    return false;
}

math::Insets AndroidServices::getSafeAreaInsets() {
    return AndroidActivity::toFramebuffer(AndroidActivity::getSafeAreaInsets());
}

bool AndroidServices::hasPointerDevice() const noexcept {
    return !AndroidActivity::isTelevision();
}

Orientation AndroidServices::getOrientation() {
    return AndroidActivity::getOrientation();
}

void AndroidServices::lockOrientation(Orientation value) {
    JavaBridge::lockOrientation(static_cast<int>(value));
}

void AndroidServices::setStatusBar(Window::StatusBar value) {
    JavaBridge::setStatusBar(static_cast<int>(value));
}

void AndroidServices::setSystemGestures(Window::SystemGestures value) {
    JavaBridge::setSystemGestures(static_cast<int>(value));
}

float AndroidServices::getStatusBarHeight() {
    return AndroidActivity::getStatusBarHeight() * NativeWindow::getScale();
}

std::optional<Fold> AndroidServices::getFold() {
    std::optional<Fold> fold = AndroidActivity::getFold();
    if (fold) {
        fold->bounds = AndroidActivity::toFramebuffer(fold->bounds);
    }

    return fold;
}

void AndroidServices::getCutouts(std::vector<math::Rect>& cutouts) {
    AndroidActivity::getCutouts(cutouts);

    for (math::Rect& cutout : cutouts) {
        cutout = AndroidActivity::toFramebuffer(cutout);
    }
}

void AndroidServices::dispatch(std::uint64_t id, std::string_view method, std::string_view paramsJson, std::span<const std::vector<std::byte>> buffers, std::span<const varn::dataview::DataView> views) {
    JavaBridge::dispatch(id, method, paramsJson, buffers, views);
}

void AndroidServices::cancel(std::uint64_t id) {
    JavaBridge::cancel(id);
}

SystemInfo AndroidServices::getSystemInfo() {
    return AndroidDeviceInfo::toSystemInfo(JavaBridge::getSystemInfo());
}

void AndroidServices::checkPermission(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback) {
    JavaBridge::checkPermission(kind, std::move(callback));
}

void AndroidServices::requestPermission(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback) {
    JavaBridge::requestPermission(kind, std::move(callback));
}

// Android 17 records why it dropped the packets of a socket, which Local Network Protection does for an app that may not reach the local network. Older systems lack the function, which the library of the app looks up while it runs, since it starts on Android 8.1.
std::string AndroidServices::explainBlockedSocket(std::intptr_t socket) {
    using ReadBlockedReason = std::int32_t (*)(int socket);
    static const auto readBlockedReason = reinterpret_cast<ReadBlockedReason>(dlsym(RTLD_DEFAULT, "android_getnetworkblockedreason"));
    if (readBlockedReason == nullptr || readBlockedReason(static_cast<int>(socket)) != ANDROID_NETWORK_BLOCKED_REASON_LNP) {
        return {};
    }

    return "Android blocked the connection, because the app may not reach the local network without the permission \"android.permission.ACCESS_LOCAL_NETWORK\". Ask the person for it with \"system.requestPermission('localNetwork')\".";
}

// Android is the only platform where the project of an app declares network access, through the permission `INTERNET` of its manifest.
std::string AndroidServices::getNetworkRequirement() {
    return JavaBridge::getNetworkRequirement();
}

// The process keeps running in the background with the frames of the runtime, and the media session keeps it in the foreground while what plays shows as playing, so Android never freezes it.
void AndroidServices::setBackgroundAudio(bool value) {
    JavaBridge::setBackgroundAudio(value);
}

void AndroidServices::setNowPlaying(const std::optional<MediaSession::NowPlaying>& value) {
    if (!value) {
        JavaBridge::updateMediaSession({}, {});
        return;
    }

    JavaBridge::updateMediaSession(MediaSession::writeNowPlaying(*value), value->artwork);
}

bool AndroidServices::isTreeWanted() {
    return JavaBridge::isAccessibilityWanted();
}

void AndroidServices::update(std::span<const AccessibilityElement> elements, std::uint64_t focused) {
    JavaBridge::updateAccessibility(AccessibilityElement::writeTree(elements, focused));
}

void AndroidServices::announce(std::string_view text, bool interrupt) {
    JavaBridge::announce(text, interrupt);
}

// Android 8.1 lacks the media data source that the decoder reads the package through, so it plays no video.
std::optional<SystemFont> AndroidServices::findSystemFont(char32_t codePoint) {
    return AndroidFonts::find(codePoint);
}

std::vector<std::uint8_t> AndroidServices::readSystemFont(const SystemFont& font) {
    return SystemFontReader::read(font.file);
}

std::unique_ptr<VideoDecoder> AndroidServices::openVideo(std::shared_ptr<io::PackageReader> file, std::string_view path) {
    if (!AndroidVideoDecoder::isAvailable()) {
        return nullptr;
    }

    return std::make_unique<AndroidVideoDecoder>(std::move(file), path);
}

void AndroidServices::openScreen(const ScreenRequest& request) {
    JavaBridge::openScreen(request);
}

void AndroidServices::cancelScreen(std::uint64_t id) {
    JavaBridge::cancelScreen(id);
}

} // namespace haylen::platform
