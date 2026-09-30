#include "platform/Services.hpp"

#include <jni.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <thread>
#include <utility>

#include "haylen/core/Json.hpp"
#include "haylen/io/Package.hpp"
#include "platform/BridgeRelay.hpp"
#include "platform/DialogRelay.hpp"
#include "platform/ScreenRelay.hpp"
#include "platform/android/AndroidActivity.hpp"
#include "platform/android/AndroidAssetPackage.hpp"
#include "platform/android/AndroidGamepads.hpp"
#include "platform/android/AndroidKeys.hpp"
#include "platform/android/AndroidTextInput.hpp"
#include "platform/android/JavaBridge.hpp"
#include "platform/sokol/MemoryWarning.hpp"
#include "platform/sokol/SokolHost.hpp"
#include "platform/sokol/SokolRuntime.hpp"
#include "sokol_app.h"

namespace haylen::platform {

std::string_view Services::getName() noexcept {
    return "android";
}

// Every activity of the process starts the runtime anew, and the first one may have ended before.
void Services::initialize() {
    AndroidActivity::holdSplashScreen();
    AndroidGamepads::enableAxes();
}

void Services::shutdown() noexcept {}

// Script errors may quote bytes that are not UTF-8, which reach the plugins replaced instead of failing the report.
void Services::reportError(const core::Json& report) {
    JavaBridge::reportError(report.dump(-1, ' ', false, core::Json::error_handler_t::replace));
}

std::vector<std::string> Services::getNativePlugins() {
    return JavaBridge::getPlugins();
}

std::shared_ptr<io::Package> Services::openBundledPackage() {
    return std::make_shared<AndroidAssetPackage>(AndroidActivity::getNative().assetManager, "app");
}

std::filesystem::path Services::getUserDataDirectory(std::string_view identifier) {
    return std::filesystem::path(AndroidActivity::getNative().internalDataPath) / std::string(identifier);
}

void Services::persistUserData() {}

bool Services::hasDesktop() noexcept {
    return false;
}

void Services::setWindowStyle(const WindowStyle&) {}

// The window of the activity fills the screen, which is its only monitor.
math::Rect Services::getWindowFrame() {
    return {0.0F, 0.0F, sapp_widthf() / sapp_dpi_scale(), sapp_heightf() / sapp_dpi_scale()};
}

void Services::setWindowFrame(const math::Rect&) {}

std::vector<Monitor> Services::getMonitors() {
    const math::Rect screen = getWindowFrame();
    return {{.name = "screen", .bounds = screen, .workArea = screen, .scale = sapp_dpi_scale(), .primary = true}};
}

void Services::setMousePassthrough(Window::Passthrough, std::span<const math::Polygon::Outline>) {}

void Services::startWindowDrag() {}

void Services::watchWindow() {}

void Services::updateWindow() {}

math::Insets Services::getSafeAreaInsets() {
    return AndroidActivity::getSafeAreaInsets();
}

bool Services::hasPointerDevice() noexcept {
    return !AndroidActivity::isTelevision();
}

void Services::pollGamepads(std::span<input::GamepadState> gamepads) {
    AndroidGamepads::poll(gamepads);
}

Orientation Services::getOrientation() {
    return AndroidActivity::getOrientation();
}

void Services::lockOrientation(Orientation value) {
    JavaBridge::lockOrientation(static_cast<int>(value));
}

TextInput& Services::getTextInput() {
    static AndroidTextInput& input = *new AndroidTextInput();
    return input;
}

void Services::dispatch(std::uint64_t id, std::string_view method, std::string_view paramsJson, std::span<const std::vector<std::byte>> buffers) {
    JavaBridge::dispatch(id, method, paramsJson, buffers);
}

void Services::cancel(std::uint64_t id) {
    JavaBridge::cancel(id);
}

SystemInfo Services::getSystemInfo() {
    const core::Json device = core::Json::parse(JavaBridge::getSystemInfo());
    SystemInfo info;
    info.os = SystemInfo::Os::Android;
    info.deviceKind = AndroidActivity::isTelevision() ? SystemInfo::DeviceKind::Tv : SystemInfo::DeviceKind::Phone;
    info.osVersion = device.value("osVersion", "");
    info.deviceModel = device.value("deviceModel", "");
    info.cpuCores = static_cast<int>(std::thread::hardware_concurrency());
    info.locale = device.value("locale", "");
    return info;
}

void Services::openUrl(const std::string& url, std::function<void(bool opened)> callback) {
    JavaBridge::openUrl(url, std::move(callback));
}

// A vibration of Android lasts at least a millisecond.
void Services::vibrate(float seconds) {
    JavaBridge::vibrate(std::max<std::int64_t>(1, std::lround(seconds * 1000.0F)));
}

void Services::showDialog(std::uint64_t id, const DialogRequest&, const std::filesystem::path&) {
    DialogRelay::resolve(id, {.failure = DialogResult::Failure{.code = DialogResult::Code::Unsupported, .message = "Native dialogs are not implemented on Android yet."}});
}

void Services::cancelDialog(std::uint64_t) {}

// The screens of Android plugins arrive with a later version of the Android library.
void Services::openScreen(const ScreenRequest& request) {
    ScreenRelay::finish(request.id, false, core::Json{{"message", "The screens of plugins are not implemented on Android yet."}, {"code", "unsupported"}}.dump());
}

void Services::cancelScreen(std::uint64_t) {}

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
    haylen::platform::SokolHost::getNativeViews().reserveInsets(haylen::platform::JavaBridge::toString(*env, key), {.left = static_cast<float>(left), .top = static_cast<float>(top), .right = static_cast<float>(right), .bottom = static_cast<float>(bottom)});
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenOverlayLayer_nativeReleaseInsets(JNIEnv* env, jclass, jbyteArray key) {
    haylen::platform::SokolHost::getNativeViews().releaseInsets(haylen::platform::JavaBridge::toString(*env, key));
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenPluginContext_nativeCoverApp(JNIEnv*, jclass) {
    haylen::platform::SokolHost::getNativeViews().coverApp();
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenPluginContext_nativeUncoverApp(JNIEnv*, jclass) {
    haylen::platform::SokolHost::getNativeViews().uncoverApp();
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenActivity_nativeSafeArea(JNIEnv*, jclass, jint left, jint top, jint right, jint bottom) {
    haylen::platform::AndroidActivity::setSafeAreaInsets({.left = static_cast<float>(left), .top = static_cast<float>(top), .right = static_cast<float>(right), .bottom = static_cast<float>(bottom)});
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenActivity_nativeTelevision(JNIEnv*, jclass, jboolean television) {
    haylen::platform::AndroidActivity::setTelevision(television == JNI_TRUE);
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenActivity_nativeLowMemory(JNIEnv*, jclass) {
    haylen::platform::MemoryWarning::raise();
}

JNIEXPORT jboolean JNICALL Java_dev_haylen_HaylenSplash_nativeFramePresented(JNIEnv*, jclass) {
    return haylen::platform::AndroidActivity::isFramePresented() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenActivity_nativeKeyboard(JNIEnv*, jclass, jint x, jint y, jint width, jint height) {
    haylen::platform::AndroidTextInput::receiveKeyboard(x, y, width, height);
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
    haylen::platform::SokolRuntime::postEvent({.type = began == JNI_TRUE ? haylen::platform::Event::Type::InterruptionBegan : haylen::platform::Event::Type::InterruptionEnded});
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenNetwork_nativeNetwork(JNIEnv*, jclass, jboolean online) {
    haylen::platform::SokolRuntime::postEvent({.type = haylen::platform::Event::Type::NetworkChanged, .online = online == JNI_TRUE});
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenActivity_nativeControllerRemoved(JNIEnv*, jclass, jint device) {
    haylen::platform::AndroidGamepads::remove(device);
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenActivity_nativeBack(JNIEnv*, jclass) {
    haylen::platform::AndroidKeys::receiveBack();
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenActivity_nativeUrlOpened(JNIEnv*, jclass, jlong request, jboolean opened) {
    haylen::platform::JavaBridge::answerUrl(request, opened == JNI_TRUE);
}
}
