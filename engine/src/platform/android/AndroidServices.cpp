#include "platform/Services.hpp"

#include <jni.h>

#include <string>

#include "haylen/io/Package.hpp"
#include "platform/android/AndroidActivity.hpp"
#include "platform/android/AndroidAssetPackage.hpp"
#include "platform/android/AndroidGamepads.hpp"
#include "platform/android/AndroidKeys.hpp"
#include "platform/android/AndroidTextInput.hpp"
#include "platform/android/JavaBridge.hpp"
#include "platform/sokol/BridgeRelay.hpp"
#include "platform/sokol/MemoryWarning.hpp"
#include "platform/sokol/SokolRuntime.hpp"

namespace haylen::platform {

std::string_view Services::getName() noexcept {
    return "android";
}

void Services::initialize() {}

void Services::shutdown() noexcept {}

void Services::reportError(const lua::Error&) {}

std::shared_ptr<io::Package> Services::openBundledPackage() {
    return std::make_shared<AndroidAssetPackage>(AndroidActivity::getNative().assetManager, "app");
}

std::filesystem::path Services::getUserDataDirectory(std::string_view identifier) {
    return std::filesystem::path(AndroidActivity::getNative().internalDataPath) / std::string(identifier);
}

void Services::persistUserData() {}

void Services::setWindowResizable(bool) {}

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
    static AndroidTextInput input;
    return input;
}

void Services::dispatch(std::uint64_t id, std::string_view method, std::string_view paramsJson) {
    JavaBridge::dispatch(id, method, paramsJson);
}

} // namespace haylen::platform

extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    return haylen::platform::JavaBridge::load(vm);
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenBridge_nativeResolve(JNIEnv* env, jclass, jlong call, jboolean ok, jbyteArray result) {
    haylen::platform::BridgeRelay::resolve(static_cast<std::uint64_t>(call), ok == JNI_TRUE, haylen::platform::JavaBridge::toString(*env, result));
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenBridge_nativeEmit(JNIEnv* env, jclass, jbyteArray event, jbyteArray payload) {
    haylen::platform::BridgeRelay::emit(haylen::platform::JavaBridge::toString(*env, event), haylen::platform::JavaBridge::toString(*env, payload));
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
}
