// The JNI entry points of the runtime: `JNI_OnLoad` and the native methods of the Java classes of the `haylen` Android library, which the virtual machine looks up by their JNI names in the exports of the library, so they hold no class and each one forwards to a class. No C++ code calls them, so they all live in this file, and every library that links the runtime takes its `JNI_OnLoad`, which keeps the whole file in the library.
#include <android/native_window_jni.h>
#include <jni.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "haylen/platform/Event.hpp"
#include "haylen/platform/MediaSession.hpp"
#include "platform/AccessibilityRelay.hpp"
#include "platform/BridgeRelay.hpp"
#include "platform/CrashGuard.hpp"
#include "platform/LinkRelay.hpp"
#include "platform/MemoryWarning.hpp"
#include "platform/NativeHost.hpp"
#include "platform/Player.hpp"
#include "platform/ScreenRelay.hpp"
#include "platform/Services.hpp"
#include "platform/android/AndroidActivity.hpp"
#include "platform/android/AndroidBatteryStatus.hpp"
#include "platform/android/AndroidDeviceInfo.hpp"
#include "platform/android/AndroidDisplays.hpp"
#include "platform/android/AndroidGamepads.hpp"
#include "platform/android/AndroidKeys.hpp"
#include "platform/android/AndroidMouseWheel.hpp"
#include "platform/android/AndroidOutputs.hpp"
#include "platform/android/AndroidServices.hpp"
#include "platform/android/AndroidStreams.hpp"
#include "platform/android/AndroidTextInput.hpp"
#include "platform/android/JavaBridge.hpp"

extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    haylen::platform::CrashGuard::install();
    haylen::platform::Services::install(haylen::platform::AndroidServices::create());
    return haylen::platform::JavaBridge::load(vm);
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenBridge_nativeResolve(JNIEnv* env, jclass, jlong call, jboolean ok, jbyteArray result, jobjectArray buffers) {
    haylen::platform::BridgeRelay::resolve(static_cast<std::uint64_t>(call), ok == JNI_TRUE, haylen::platform::JavaBridge::toString(*env, result), haylen::platform::JavaBridge::toBuffers(*env, buffers));
}

JNIEXPORT jlong JNICALL Java_dev_haylen_HaylenPayload_nativeShare(JNIEnv*, jclass, jlong handle) {
    return haylen::platform::JavaBridge::shareView(handle);
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

JNIEXPORT void JNICALL Java_dev_haylen_HaylenActivity_nativeTouchpadScroll(JNIEnv*, jclass, jint phase, jfloat x, jfloat y, jfloat deltaX, jfloat deltaY) {
    haylen::platform::AndroidMouseWheel::receiveTouchpadScroll(phase, x, y, deltaX, deltaY);
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

JNIEXPORT void JNICALL Java_dev_haylen_HaylenScreen_nativeFinish(JNIEnv* env, jclass, jlong id, jboolean ok, jbyteArray json, jobjectArray buffers) {
    haylen::platform::ScreenRelay::finish(static_cast<std::uint64_t>(id), ok == JNI_TRUE, haylen::platform::JavaBridge::toString(*env, json), haylen::platform::JavaBridge::toBuffers(*env, buffers));
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenScreen_nativeRestore(JNIEnv* env, jclass, jbyteArray plugin, jbyteArray name, jbyteArray state, jboolean ok, jbyteArray json, jobjectArray buffers) {
    haylen::platform::ScreenRelay::restore(haylen::platform::JavaBridge::toString(*env, plugin), haylen::platform::JavaBridge::toString(*env, name), haylen::platform::JavaBridge::toString(*env, state), ok == JNI_TRUE, haylen::platform::JavaBridge::toString(*env, json), haylen::platform::JavaBridge::toBuffers(*env, buffers));
}

JNIEXPORT jlong JNICALL Java_dev_haylen_HaylenVideoStream_nativeOpen(JNIEnv* env, jclass, jbyteArray plugin, jbyteArray name, jint format, jint width, jint height) {
    return haylen::platform::AndroidStreams::openVideo(*env, plugin, name, format, width, height);
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenVideoStream_nativePushBuffer(JNIEnv* env, jclass, jlong stream, jobject pixels, jint offset, jint length, jint width, jint height, jint stride, jdouble timestamp) {
    haylen::platform::AndroidStreams::pushVideoBuffer(*env, stream, pixels, offset, length, width, height, stride, timestamp);
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenVideoStream_nativePushArray(JNIEnv* env, jclass, jlong stream, jbyteArray pixels, jint offset, jint length, jint width, jint height, jint stride, jdouble timestamp) {
    haylen::platform::AndroidStreams::pushVideoArray(*env, stream, pixels, offset, length, width, height, stride, timestamp);
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenVideoStream_nativePushBitmap(JNIEnv* env, jclass, jlong stream, jobject bitmap, jdouble timestamp) {
    haylen::platform::AndroidStreams::pushBitmap(*env, stream, bitmap, timestamp);
}

JNIEXPORT jlong JNICALL Java_dev_haylen_HaylenAudioStream_nativeOpen(JNIEnv* env, jclass, jbyteArray plugin, jbyteArray name, jint sampleRate, jint channels, jint format, jint capacity) {
    return haylen::platform::AndroidStreams::openAudio(*env, plugin, name, sampleRate, channels, format, capacity);
}

JNIEXPORT jint JNICALL Java_dev_haylen_HaylenAudioStream_nativePushFloats(JNIEnv* env, jclass, jlong stream, jfloatArray samples, jint frames) {
    return haylen::platform::AndroidStreams::pushFloats(*env, stream, samples, frames);
}

JNIEXPORT jint JNICALL Java_dev_haylen_HaylenAudioStream_nativePushShorts(JNIEnv* env, jclass, jlong stream, jshortArray samples, jint frames) {
    return haylen::platform::AndroidStreams::pushShorts(*env, stream, samples, frames);
}
}
