#include "platform/Services.hpp"

#include <android/multinetwork.h>
#include <android/native_window_jni.h>
#include <dlfcn.h>
#include <jni.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <variant>

#include "content/FolderStoreFiles.hpp"
#include "content/ReleasePackage.hpp"
#include "haylen/content/Bootstrap.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/io/Package.hpp"
#include "platform/AccessibilityRelay.hpp"
#include "platform/BridgeRelay.hpp"
#include "platform/DialogRelay.hpp"
#include "platform/LinkRelay.hpp"
#include "platform/MemoryWarning.hpp"
#include "platform/NativeHost.hpp"
#include "platform/NativeWindow.hpp"
#include "platform/Player.hpp"
#include "platform/ScreenRelay.hpp"
#include "platform/android/AndroidActivity.hpp"
#include "platform/android/AndroidAssetPackage.hpp"
#include "platform/android/AndroidAssetReader.hpp"
#include "platform/android/AndroidBatteryStatus.hpp"
#include "platform/android/AndroidDeviceInfo.hpp"
#include "platform/android/AndroidDialogJson.hpp"
#include "platform/android/AndroidDisplays.hpp"
#include "platform/android/AndroidGamepads.hpp"
#include "platform/android/AndroidKeys.hpp"
#include "platform/android/AndroidMouseWheel.hpp"
#include "platform/android/AndroidOutputs.hpp"
#include "platform/android/AndroidTextInput.hpp"
#include "platform/android/AndroidVideoDecoder.hpp"
#include "platform/android/JavaBridge.hpp"
#include "varn/http/HttpClientFailureNote.h"

namespace haylen::platform {

std::string_view Services::getName() noexcept {
    return "android";
}

// Every activity of the process starts the runtime anew, and the first one may have ended before.
// Android drops the packets of a request to the local network while the app does not hold the permission for it, so the HTTP requests that fail say so while the person has not granted it, which is the state of the permission and no guess about the address.
void Services::initialize() {
    AndroidActivity::holdSplashScreen();
    AndroidGamepads::enableAxes();
    AndroidMouseWheel::enableAxes();

    // clang-format off
    varn::http::client::FailureNote::install([](const std::string&) {
        return JavaBridge::isPermissionHeld(Permission::Kind::LocalNetwork) ? std::string() : std::string("Access to the local network is not granted, so Android blocks a request to the local network until \"system.requestPermission('localNetwork')\" grants it.");
    });
    // clang-format on
}

void Services::shutdown() noexcept {}

// Script errors may quote bytes that are not UTF-8, which reach the plugins replaced instead of failing the report.
void Services::reportError(const core::Json& report) {
    JavaBridge::reportError(report.dump(-1, ' ', false, core::Json::error_handler_t::replace));
}

std::vector<std::string> Services::getNativePlugins() {
    return JavaBridge::getPlugins();
}

// A protected release is a flat folder of manifests and shards, while a development build ships the package with its index.
std::shared_ptr<io::Package> Services::openBundledPackage() {
    AAssetManager* manager = AndroidActivity::getNative().assetManager;
    const AndroidAssetReader::Handle manifest(AAssetManager_open(manager, ("app/" + std::string(content::ReleasePackage::kAppManifestFile)).c_str(), AASSET_MODE_UNKNOWN), &AAsset_close);
    if (manifest != nullptr) {
        return content::ReleasePackage::openBundled(AndroidAssetPackage::openFlat(manager, "app"), content::Bootstrap::find(), [](std::string_view identifier) { return std::make_unique<content::FolderStoreFiles>(getContentStoreDirectory(identifier)); });
    }
    return AndroidAssetPackage::openIndexed(manager, "app");
}

std::filesystem::path Services::getUserDataDirectory(std::string_view identifier) {
    return std::filesystem::path(AndroidActivity::getNative().internalDataPath) / std::string(identifier);
}

// Downloaded content comes back with a download, so it lives in the folder of the app that Android leaves out of its backups, next to the files folder.
std::filesystem::path Services::getContentStoreDirectory(std::string_view identifier) {
    return std::filesystem::path(AndroidActivity::getNative().internalDataPath).parent_path() / "no_backup" / (std::string(identifier) + "-content");
}

void Services::persistUserData() {}

bool Services::hasDesktop() noexcept {
    return false;
}

void Services::setWindowStyle(const WindowStyle&) {}

void Services::setWindowBackground(const math::Color&) {}

// The window of the activity fills its display.
math::Rect Services::getWindowFrame() {
    return {0.0F, 0.0F, NativeWindow::getFramebufferSize().x / NativeWindow::getScale(), NativeWindow::getFramebufferSize().y / NativeWindow::getScale()};
}

void Services::setWindowFrame(const math::Rect&) {}

std::vector<Display> Services::getDisplays() {
    return AndroidDisplays::getDisplays();
}

std::string Services::getWindowDisplayId() {
    return AndroidDisplays::getWindowDisplayId();
}

void Services::setMousePassthrough(Window::Passthrough, std::span<const math::Polygon::Outline>) {}

void Services::startWindowDrag() {}

void Services::watchWindow() {}

void Services::updateWindow() {}

math::Insets Services::getSafeAreaInsets() {
    return AndroidActivity::toFramebuffer(AndroidActivity::getSafeAreaInsets());
}

bool Services::hasPointerDevice() noexcept {
    return !AndroidActivity::isTelevision();
}

Orientation Services::getOrientation() {
    return AndroidActivity::getOrientation();
}

void Services::lockOrientation(Orientation value) {
    JavaBridge::lockOrientation(static_cast<int>(value));
}

void Services::setStatusBar(Window::StatusBar value) {
    JavaBridge::setStatusBar(static_cast<int>(value));
}

void Services::setSystemGestures(Window::SystemGestures value) {
    JavaBridge::setSystemGestures(static_cast<int>(value));
}

float Services::getStatusBarHeight() {
    return AndroidActivity::getStatusBarHeight() * NativeWindow::getScale();
}

std::optional<Fold> Services::getFold() {
    std::optional<Fold> fold = AndroidActivity::getFold();
    if (fold) {
        fold->bounds = AndroidActivity::toFramebuffer(fold->bounds);
    }
    return fold;
}

void Services::getCutouts(std::vector<math::Rect>& cutouts) {
    AndroidActivity::getCutouts(cutouts);
    for (math::Rect& cutout : cutouts) {
        cutout = AndroidActivity::toFramebuffer(cutout);
    }
}

void Services::endSplashScreen(float fadeOutSeconds) {
    AndroidActivity::endSplashScreen(fadeOutSeconds);
}

TextInput& Services::getTextInput() {
    static AndroidTextInput& input = *new AndroidTextInput();
    return input;
}

void Services::dispatch(std::uint64_t id, std::string_view method, std::string_view paramsJson, std::span<const std::vector<std::byte>> buffers, std::span<const varn::dataview::DataView> views) {
    JavaBridge::dispatch(id, method, paramsJson, buffers, views);
}

void Services::cancel(std::uint64_t id) {
    JavaBridge::cancel(id);
}

SystemInfo Services::getSystemInfo() {
    return AndroidDeviceInfo::toSystemInfo(JavaBridge::getSystemInfo());
}

void Services::checkPermission(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback) {
    JavaBridge::checkPermission(kind, std::move(callback));
}

void Services::requestPermission(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback) {
    JavaBridge::requestPermission(kind, std::move(callback));
}

// Android 17 records why it dropped the packets of a socket, which Local Network Protection does for an app that may not reach the local network. Older systems lack the function, which the library of the app looks up while it runs, since it starts on Android 8.1.
std::string Services::explainBlockedSocket(std::intptr_t socket) {
    using ReadBlockedReason = std::int32_t (*)(int socket);
    static const auto readBlockedReason = reinterpret_cast<ReadBlockedReason>(dlsym(RTLD_DEFAULT, "android_getnetworkblockedreason"));
    if (readBlockedReason == nullptr || readBlockedReason(static_cast<int>(socket)) != ANDROID_NETWORK_BLOCKED_REASON_LNP) {
        return {};
    }
    return "Android blocked the connection, because the app may not reach the local network without the permission \"android.permission.ACCESS_LOCAL_NETWORK\". Ask the person for it with \"system.requestPermission('localNetwork')\".";
}

// Android hands the links to the activity that runs, which reports them.
bool Services::forwardLink(std::string_view, std::string_view) {
    return false;
}

void Services::listenForLinks(std::string_view) {}

// The process keeps running in the background with the frames of the runtime, and the media session keeps it in the foreground while what plays shows as playing, so Android never freezes it.
void Services::setBackgroundAudio(bool value) {
    JavaBridge::setBackgroundAudio(value);
}

void Services::setNowPlaying(const std::optional<MediaSession::NowPlaying>& value) {
    if (!value) {
        JavaBridge::updateMediaSession({}, {});
        return;
    }
    JavaBridge::updateMediaSession(MediaSession::writeNowPlaying(*value), value->artwork);
}

bool Services::isAccessibilityTreeWanted() {
    return JavaBridge::isAccessibilityWanted();
}

void Services::updateAccessibility(std::span<const AccessibilityElement> elements, std::uint64_t focused) {
    JavaBridge::updateAccessibility(AccessibilityElement::writeTree(elements, focused));
}

void Services::announce(std::string_view text, bool interrupt) {
    JavaBridge::announce(text, interrupt);
}

// Android 8.1 lacks the media data source that the decoder reads the package through, so it plays no video.
std::unique_ptr<VideoDecoder> Services::openVideo(std::shared_ptr<io::PackageReader> file, std::string_view path) {
    if (!AndroidVideoDecoder::isAvailable()) {
        return nullptr;
    }
    return std::make_unique<AndroidVideoDecoder>(std::move(file), path);
}

// Script texts may hold bytes that are not UTF-8, which reach the dialog replaced instead of failing it.
void Services::showDialog(std::uint64_t id, const DialogRequest& request, const std::filesystem::path& folder) {
    const auto show = [id](const core::Json& dialog, std::span<const std::uint8_t> data) { JavaBridge::showDialog(id, dialog.dump(-1, ' ', false, core::Json::error_handler_t::replace), data); };
    if (const auto* message = std::get_if<DialogRequest::Message>(&request.dialog)) {
        show(AndroidDialogJson::describe(*message), {});
    } else if (const auto* files = std::get_if<DialogRequest::OpenFiles>(&request.dialog)) {
        show(AndroidDialogJson::describe(*files, folder), {});
    } else if (const auto* save = std::get_if<DialogRequest::SaveFile>(&request.dialog)) {
        show(AndroidDialogJson::describe(*save), save->data);
    } else {
        DialogRelay::resolve(id, {.failure = DialogResult::Failure{.code = DialogResult::Code::Unsupported, .message = "Android folders have no paths, so Android opens no folders."}});
    }
}

void Services::cancelDialog(std::uint64_t id) {
    JavaBridge::cancelDialog(id);
}

void Services::openScreen(const ScreenRequest& request) {
    JavaBridge::openScreen(request);
}

void Services::cancelScreen(std::uint64_t id) {
    JavaBridge::cancelScreen(id);
}

HaylenNativeWindow Services::getNativeWindow() {
    return {};
}

} // namespace haylen::platform

extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    return haylen::platform::JavaBridge::load(vm);
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenBridge_nativeResolve(JNIEnv* env, jclass, jlong call, jboolean ok, jbyteArray result, jobjectArray buffers) {
    haylen::platform::BridgeRelay::resolve(static_cast<std::uint64_t>(call), ok == JNI_TRUE, haylen::platform::JavaBridge::toString(*env, result), haylen::platform::JavaBridge::toBuffers(*env, buffers));
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenBridge_nativeEmit(JNIEnv* env, jclass, jbyteArray event, jbyteArray payload, jobjectArray buffers, jboolean retain, jboolean batched) {
    haylen::platform::BridgeRelay::emit(haylen::platform::JavaBridge::toString(*env, event), haylen::platform::JavaBridge::toString(*env, payload), haylen::platform::JavaBridge::toBuffers(*env, buffers), {.retain = retain == JNI_TRUE, .batched = batched == JNI_TRUE});
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenOverlayLayer_nativeReserveInsets(JNIEnv* env, jclass, jbyteArray key, jint left, jint top, jint right, jint bottom) {
    haylen::platform::NativeHost::getNativeViews().reserveInsets(haylen::platform::JavaBridge::toString(*env, key), {.left = static_cast<float>(left), .top = static_cast<float>(top), .right = static_cast<float>(right), .bottom = static_cast<float>(bottom)});
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenOverlayLayer_nativeReleaseInsets(JNIEnv* env, jclass, jbyteArray key) {
    haylen::platform::NativeHost::getNativeViews().releaseInsets(haylen::platform::JavaBridge::toString(*env, key));
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenPluginContext_nativeCoverApp(JNIEnv*, jclass) {
    haylen::platform::NativeHost::getNativeViews().coverApp();
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenPluginContext_nativeUncoverApp(JNIEnv*, jclass) {
    haylen::platform::NativeHost::getNativeViews().uncoverApp();
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenActivity_nativeCutouts(JNIEnv* env, jclass, jintArray edges) {
    const jsize count = env->GetArrayLength(edges);
    std::vector<jint> values(static_cast<std::size_t>(count));
    env->GetIntArrayRegion(edges, 0, count, values.data());
    std::vector<haylen::math::Rect> cutouts;
    for (std::size_t index = 0; index + 3 < values.size(); index += 4) {
        cutouts.push_back(haylen::math::Rect::fromMinMax({static_cast<float>(values[index]), static_cast<float>(values[index + 1])}, {static_cast<float>(values[index + 2]), static_cast<float>(values[index + 3])}));
    }
    haylen::platform::AndroidActivity::setCutouts(std::move(cutouts));
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenActivity_nativeSafeArea(JNIEnv*, jclass, jint left, jint top, jint right, jint bottom, jint statusBar) {
    haylen::platform::AndroidActivity::setSafeAreaInsets({.left = static_cast<float>(left), .top = static_cast<float>(top), .right = static_cast<float>(right), .bottom = static_cast<float>(bottom)});
    haylen::platform::AndroidActivity::setStatusBarHeight(static_cast<float>(statusBar));
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenActivity_nativeTelevision(JNIEnv*, jclass, jboolean television) {
    haylen::platform::AndroidActivity::setTelevision(television == JNI_TRUE);
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenActivity_nativeLink(JNIEnv* env, jclass, jbyteArray url) {
    haylen::platform::LinkRelay::receive(haylen::platform::JavaBridge::toString(*env, url));
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenActivity_nativeLowMemory(JNIEnv*, jclass, jboolean critical) {
    haylen::platform::MemoryWarning::raise(critical == JNI_TRUE ? haylen::platform::Event::MemoryLevel::Critical : haylen::platform::Event::MemoryLevel::Warning);
}

JNIEXPORT jfloat JNICALL Java_dev_haylen_HaylenSplash_nativeFadeOut(JNIEnv*, jclass) {
    return haylen::platform::AndroidActivity::getSplashFadeOut();
}

JNIEXPORT jboolean JNICALL Java_dev_haylen_HaylenSplash_nativeFramePresented(JNIEnv*, jclass) {
    return haylen::platform::AndroidActivity::isFramePresented() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenActivity_nativeKeyboard(JNIEnv*, jclass, jint x, jint y, jint width, jint height) {
    haylen::platform::AndroidTextInput::receiveKeyboard(x, y, width, height);
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenActivity_nativeDensity(JNIEnv*, jclass, jfloat density) {
    haylen::platform::AndroidActivity::setDensity(density);
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenActivity_nativeOrientation(JNIEnv*, jclass, jboolean portrait) {
    haylen::platform::AndroidActivity::setOrientation(portrait == JNI_TRUE ? haylen::platform::Orientation::Portrait : haylen::platform::Orientation::Landscape);
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenEditText_nativeTextEdited(JNIEnv* env, jclass, jlong field, jlong revision, jbyteArray text, jint selectionStart, jint selectionEnd, jint compositionStart, jint compositionEnd) {
    haylen::platform::AndroidTextInput::receiveEdit(*env, field, revision, text, selectionStart, selectionEnd, compositionStart, compositionEnd);
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenEditText_nativeTextAction(JNIEnv*, jclass, jlong field, jint action) {
    haylen::platform::AndroidTextInput::receiveAction(field, action);
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenAudioFocus_nativeInterruption(JNIEnv*, jclass, jboolean began) {
    haylen::platform::Player::postEvent({.type = began == JNI_TRUE ? haylen::platform::Event::Type::InterruptionBegan : haylen::platform::Event::Type::InterruptionEnded});
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenNetwork_nativeNetwork(JNIEnv*, jclass, jboolean online) {
    haylen::platform::Player::postEvent({.type = haylen::platform::Event::Type::NetworkChanged, .online = online == JNI_TRUE});
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenActivity_nativeControllerAdded(JNIEnv* env, jclass, jint device, jbyteArray name, jint vendor) {
    haylen::platform::AndroidGamepads::add(device, haylen::platform::JavaBridge::toString(*env, name), static_cast<std::uint16_t>(vendor));
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenActivity_nativeControllerRemoved(JNIEnv*, jclass, jint device) {
    haylen::platform::AndroidGamepads::remove(device);
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenActivity_nativeBack(JNIEnv*, jclass) {
    haylen::platform::AndroidKeys::receiveBack();
}

JNIEXPORT jboolean JNICALL Java_dev_haylen_HaylenActivity_nativeBackHeld(JNIEnv*, jclass, jboolean pressed) {
    return haylen::platform::AndroidKeys::receiveBackHeld(pressed == JNI_TRUE) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenPermissions_nativeAnswered(JNIEnv* env, jclass, jlong request, jbyteArray answer) {
    haylen::platform::JavaBridge::answerPermission(request, haylen::platform::JavaBridge::toString(*env, answer));
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenActivity_nativeTheme(JNIEnv*, jclass, jboolean dark) {
    haylen::platform::NativeHost::getSystemState().setTheme(dark == JNI_TRUE ? haylen::platform::Theme::Dark : haylen::platform::Theme::Light);
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenActivity_nativeLanguages(JNIEnv* env, jclass, jbyteArray languages) {
    haylen::platform::NativeHost::getSystemState().setLanguages(haylen::platform::AndroidDeviceInfo::toLanguages(haylen::platform::JavaBridge::toString(*env, languages)));
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenAccessibility_nativeAccessibility(JNIEnv*, jclass, jfloat textScale, jboolean reducedMotion, jboolean highContrast, jboolean screenReader) {
    haylen::platform::NativeHost::getSystemState().setAccessibility({.textScale = textScale, .reducedMotion = reducedMotion == JNI_TRUE, .highContrast = highContrast == JNI_TRUE, .reducedTransparency = false, .screenReader = screenReader == JNI_TRUE});
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenAccessibilityTree_nativeWanted(JNIEnv*, jclass, jboolean wanted) {
    haylen::platform::JavaBridge::setAccessibilityWanted(wanted == JNI_TRUE);
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenAccessibilityTree_nativePerform(JNIEnv*, jclass, jlong element, jint action) {
    haylen::platform::AccessibilityRelay::perform({.element = static_cast<std::uint64_t>(element), .action = static_cast<haylen::platform::AccessibilityElement::Action>(action)});
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenDisplays_nativeDisplays(JNIEnv* env, jclass, jbyteArray json, jint windowDisplay) {
    haylen::platform::AndroidDisplays::receive(haylen::platform::JavaBridge::toString(*env, json), windowDisplay);
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenOutputs_nativeSurface(JNIEnv* env, jclass, jint number, jobject surface, jint width, jint height, jfloat density) {
    ANativeWindow* window = surface != nullptr ? ANativeWindow_fromSurface(env, surface) : nullptr;
    haylen::platform::AndroidOutputs::receiveSurface(static_cast<std::uint32_t>(number), window, width, height, density);
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenOutputs_nativeEnded(JNIEnv* env, jclass, jint number, jint ending, jbyteArray reason) {
    haylen::platform::AndroidOutputs::receiveEnding(static_cast<std::uint32_t>(number), static_cast<haylen::platform::Output::Ending>(ending), haylen::platform::JavaBridge::toString(*env, reason));
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenOutputs_nativeTouch(JNIEnv* env, jclass, jint number, jint type, jintArray ids, jfloatArray points, jint changed) {
    const jsize count = env->GetArrayLength(ids);
    std::array<jint, haylen::platform::Event::kMaxTouchPoints> touchIds{};
    std::array<jfloat, haylen::platform::Event::kMaxTouchPoints * 2> touchPoints{};
    const jsize kept = std::min(count, static_cast<jsize>(haylen::platform::Event::kMaxTouchPoints));
    env->GetIntArrayRegion(ids, 0, kept, touchIds.data());
    env->GetFloatArrayRegion(points, 0, kept * 2, touchPoints.data());
    const std::array<haylen::platform::Event::Type, 4> types{haylen::platform::Event::Type::TouchBegan, haylen::platform::Event::Type::TouchMoved, haylen::platform::Event::Type::TouchEnded, haylen::platform::Event::Type::TouchCancelled};
    haylen::platform::AndroidOutputs::receiveTouch(static_cast<std::uint32_t>(number), types.at(static_cast<std::size_t>(type)), std::span<const int>(touchIds.data(), static_cast<std::size_t>(kept)), std::span<const float>(touchPoints.data(), static_cast<std::size_t>(kept) * 2), changed);
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenFold_nativeFold(JNIEnv*, jclass, jboolean present, jint left, jint top, jint right, jint bottom, jboolean vertical, jboolean halfOpened, jboolean separating, jboolean occluding) {
    haylen::platform::AndroidActivity::receiveFold({.present = present == JNI_TRUE, .left = left, .top = top, .right = right, .bottom = bottom, .vertical = vertical == JNI_TRUE, .halfOpened = halfOpened == JNI_TRUE, .separating = separating == JNI_TRUE, .occluding = occluding == JNI_TRUE});
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenMediaSession_nativeCommand(JNIEnv*, jclass, jint command, jdouble position) {
    haylen::platform::MediaSession::receive(static_cast<haylen::platform::MediaSession::Command>(command), position);
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenBattery_nativeBattery(JNIEnv*, jclass, jboolean present, jint level, jint scale, jint status) {
    haylen::platform::NativeHost::getSystemState().setBattery(haylen::platform::AndroidBatteryStatus::toBattery(present == JNI_TRUE, level, scale, status));
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenPowerSaving_nativePowerSaving(JNIEnv*, jclass, jboolean enabled) {
    haylen::platform::NativeHost::getSystemState().setPowerSaving(enabled == JNI_TRUE);
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenTimeZone_nativeTimeZone(JNIEnv* env, jclass, jbyteArray zone) {
    haylen::platform::NativeHost::getSystemState().setTimeZone(haylen::platform::JavaBridge::toString(*env, zone));
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenDialogs_nativeButton(JNIEnv*, jclass, jlong id, jint button) {
    haylen::platform::DialogRelay::resolve(static_cast<std::uint64_t>(id), {.button = static_cast<std::size_t>(button)});
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenDialogs_nativeFiles(JNIEnv* env, jclass, jlong id, jbyteArray json) {
    haylen::platform::DialogRelay::resolve(static_cast<std::uint64_t>(id), {.files = haylen::platform::AndroidDialogJson::readFiles(haylen::platform::JavaBridge::toString(*env, json))});
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenDialogs_nativeSaved(JNIEnv* env, jclass, jlong id, jbyteArray name) {
    haylen::platform::DialogRelay::resolve(static_cast<std::uint64_t>(id), {.saved = haylen::platform::DialogResult::File{.name = haylen::platform::JavaBridge::toString(*env, name)}});
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenDialogs_nativeDismissed(JNIEnv*, jclass, jlong id) {
    haylen::platform::DialogRelay::resolve(static_cast<std::uint64_t>(id), {});
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenDialogs_nativeFailed(JNIEnv* env, jclass, jlong id, jbyteArray code, jbyteArray message) {
    haylen::platform::DialogRelay::resolve(static_cast<std::uint64_t>(id), {.failure = haylen::platform::AndroidDialogJson::readFailure(haylen::platform::JavaBridge::toString(*env, code), haylen::platform::JavaBridge::toString(*env, message))});
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenScreen_nativeFinish(JNIEnv* env, jclass, jlong id, jboolean ok, jbyteArray json, jobjectArray buffers) {
    haylen::platform::ScreenRelay::finish(static_cast<std::uint64_t>(id), ok == JNI_TRUE, haylen::platform::JavaBridge::toString(*env, json), haylen::platform::JavaBridge::toBuffers(*env, buffers));
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenScreen_nativeRestore(JNIEnv* env, jclass, jbyteArray plugin, jbyteArray name, jbyteArray state, jboolean ok, jbyteArray json, jobjectArray buffers) {
    haylen::platform::ScreenRelay::restore(haylen::platform::JavaBridge::toString(*env, plugin), haylen::platform::JavaBridge::toString(*env, name), haylen::platform::JavaBridge::toString(*env, state), ok == JNI_TRUE, haylen::platform::JavaBridge::toString(*env, json), haylen::platform::JavaBridge::toBuffers(*env, buffers));
}
}
