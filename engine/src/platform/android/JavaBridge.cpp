#include "platform/android/JavaBridge.hpp"

#include <format>
#include <stdexcept>
#include <string>
#include <utility>

#include "haylen/core/Json.hpp"
#include "haylen/core/Log.hpp"
#include "platform/BridgeRelay.hpp"
#include "platform/ScreenRelay.hpp"
#include "platform/android/AndroidOutputs.hpp"
#include "varn/dataview/DataViewJni.h"
#include "varn/http/AndroidHttpBridge.h"

namespace haylen::platform {

JavaVM* JavaBridge::javaVm = nullptr;
pthread_key_t JavaBridge::attachedThreads{};
std::vector<std::string>& JavaBridge::plugins = *new std::vector<std::string>();
jclass JavaBridge::byteArrayClass = nullptr;
jclass JavaBridge::bridgeClass = nullptr;
jmethodID JavaBridge::dispatchMethod = nullptr;
jmethodID JavaBridge::cancelMethod = nullptr;
jmethodID JavaBridge::setAppRunningMethod = nullptr;
jclass JavaBridge::pluginsClass = nullptr;
jmethodID JavaBridge::reportErrorMethod = nullptr;
jclass JavaBridge::editorClass = nullptr;
jmethodID JavaBridge::editMethod = nullptr;
jmethodID JavaBridge::finishMethod = nullptr;
jclass JavaBridge::activityClass = nullptr;
jmethodID JavaBridge::lockOrientationMethod = nullptr;
jmethodID JavaBridge::setStatusBarMethod = nullptr;
jmethodID JavaBridge::setSystemGesturesMethod = nullptr;
jmethodID JavaBridge::keepScreenOnMethod = nullptr;
jmethodID JavaBridge::captureBackMethod = nullptr;
jmethodID JavaBridge::systemInfoMethod = nullptr;
jmethodID JavaBridge::rumbleGamepadMethod = nullptr;
jmethodID JavaBridge::setClipboardMethod = nullptr;
jmethodID JavaBridge::clipboardMethod = nullptr;
jmethodID JavaBridge::networkRequirementMethod = nullptr;
jmethodID JavaBridge::developmentServerMethod = nullptr;
jclass JavaBridge::outputsClass = nullptr;
jmethodID JavaBridge::openOutputMethod = nullptr;
jmethodID JavaBridge::closeOutputMethod = nullptr;
jclass JavaBridge::permissionsClass = nullptr;
jmethodID JavaBridge::permissionStateMethod = nullptr;
jmethodID JavaBridge::permissionHeldMethod = nullptr;
jmethodID JavaBridge::requestPermissionMethod = nullptr;
jclass JavaBridge::screensClass = nullptr;
jmethodID JavaBridge::openScreenMethod = nullptr;
jmethodID JavaBridge::cancelScreenMethod = nullptr;
jclass JavaBridge::accessibilityClass = nullptr;
jmethodID JavaBridge::updateAccessibilityMethod = nullptr;
jmethodID JavaBridge::announceMethod = nullptr;
jclass JavaBridge::mediaSessionClass = nullptr;
jmethodID JavaBridge::updateMediaSessionMethod = nullptr;
jmethodID JavaBridge::setBackgroundAudioMethod = nullptr;
std::atomic<bool> JavaBridge::accessibilityWanted{false};
std::mutex& JavaBridge::permissionMutex = *new std::mutex();
std::unordered_map<std::int64_t, std::function<void(Permission::Answer)>>& JavaBridge::permissionCallbacks = *new std::unordered_map<std::int64_t, std::function<void(Permission::Answer)>>();
std::int64_t JavaBridge::nextPermission = 1;

jint JavaBridge::load(JavaVM* vm) {
    JNIEnv* env = nullptr;
    vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    javaVm = vm;
    pthread_key_create(&attachedThreads, &JavaBridge::detachThread);

    // A class or a method that is missing, such as one that R8 removed, stops the load with an engine error that names it, so no call ever runs with an exception pending.
    byteArrayClass = findClass(*env, "[B");
    bridgeClass = findClass(*env, "dev/haylen/HaylenBridge");
    pluginsClass = findClass(*env, "dev/haylen/HaylenPlugins");
    editorClass = findClass(*env, "dev/haylen/HaylenEditText");
    activityClass = findClass(*env, "dev/haylen/HaylenActivity");
    screensClass = findClass(*env, "dev/haylen/HaylenScreens");
    accessibilityClass = findClass(*env, "dev/haylen/HaylenAccessibilityTree");
    mediaSessionClass = findClass(*env, "dev/haylen/HaylenMediaSession");
    permissionsClass = findClass(*env, "dev/haylen/HaylenPermissions");
    outputsClass = findClass(*env, "dev/haylen/HaylenOutputs");
    if (byteArrayClass == nullptr || bridgeClass == nullptr || pluginsClass == nullptr || editorClass == nullptr || activityClass == nullptr || screensClass == nullptr || accessibilityClass == nullptr || mediaSessionClass == nullptr || permissionsClass == nullptr || outputsClass == nullptr) {
        return JNI_ERR;
    }
    dispatchMethod = findMethod(*env, bridgeClass, "dispatch", "(J[B[B[[B[J)V");
    cancelMethod = findMethod(*env, bridgeClass, "cancel", "(J)V");
    setAppRunningMethod = findMethod(*env, bridgeClass, "setAppRunning", "(Z)V");
    reportErrorMethod = findMethod(*env, pluginsClass, "reportError", "([B)V");
    editMethod = findMethod(*env, editorClass, "edit", "([B)V");
    finishMethod = findMethod(*env, editorClass, "finish", "()V");
    lockOrientationMethod = findMethod(*env, activityClass, "lockOrientation", "(I)V");
    setStatusBarMethod = findMethod(*env, activityClass, "setStatusBar", "(I)V");
    setSystemGesturesMethod = findMethod(*env, activityClass, "setSystemGestures", "(I)V");
    keepScreenOnMethod = findMethod(*env, activityClass, "keepScreenOn", "(Z)V");
    captureBackMethod = findMethod(*env, activityClass, "captureBack", "(Z)V");
    systemInfoMethod = findMethod(*env, activityClass, "systemInfo", "()[B");
    rumbleGamepadMethod = findMethod(*env, activityClass, "rumbleGamepad", "(IFFJ)V");
    setClipboardMethod = findMethod(*env, activityClass, "setClipboard", "([B)[B");
    clipboardMethod = findMethod(*env, activityClass, "clipboard", "()[B");
    networkRequirementMethod = findMethod(*env, activityClass, "networkRequirement", "()[B");
    developmentServerMethod = findMethod(*env, activityClass, "developmentServer", "()[B");
    openScreenMethod = findMethod(*env, screensClass, "open", "(J[B[B[B[[B[J[BZ)V");
    cancelScreenMethod = findMethod(*env, screensClass, "cancel", "(J)V");
    updateAccessibilityMethod = findMethod(*env, accessibilityClass, "update", "([B)V");
    announceMethod = findMethod(*env, accessibilityClass, "announce", "([BZ)V");
    updateMediaSessionMethod = findMethod(*env, mediaSessionClass, "update", "([B[B)V");
    setBackgroundAudioMethod = findMethod(*env, mediaSessionClass, "setBackground", "(Z)V");
    permissionStateMethod = findMethod(*env, permissionsClass, "state", "(J[B)V");
    permissionHeldMethod = findMethod(*env, permissionsClass, "isHeld", "([B)Z");
    openOutputMethod = findMethod(*env, outputsClass, "open", "(II[B)V");
    closeOutputMethod = findMethod(*env, outputsClass, "close", "(I)V");
    requestPermissionMethod = findMethod(*env, permissionsClass, "request", "(J[B)V");
    const jmethodID idsMethod = findMethod(*env, pluginsClass, "ids", "()[B");
    for (const jmethodID method : {dispatchMethod, cancelMethod, setAppRunningMethod, reportErrorMethod, editMethod, finishMethod, lockOrientationMethod, setStatusBarMethod, setSystemGesturesMethod, keepScreenOnMethod, captureBackMethod, systemInfoMethod, rumbleGamepadMethod, setClipboardMethod, clipboardMethod, networkRequirementMethod, developmentServerMethod, openScreenMethod, cancelScreenMethod, updateAccessibilityMethod, announceMethod, updateMediaSessionMethod, setBackgroundAudioMethod, permissionStateMethod, permissionHeldMethod, requestPermissionMethod, openOutputMethod, closeOutputMethod, idsMethod}) {
        if (method == nullptr) {
            return JNI_ERR;
        }
    }

    // The plugins load when the process starts, before any activity loads this library, so their list is final here.
    const JavaLocalRef ids(*env, static_cast<jbyteArray>(env->CallStaticObjectMethod(pluginsClass, idsMethod)));
    if (!clearException(*env, "dev.haylen.HaylenPlugins.ids")) {
        plugins = core::Json::parse(toString(*env, ids.get())).get<std::vector<std::string>>();
    }

    varn::http::client::AndroidHttpBridge::publish(vm);
    if (!varn::dataview::DataViewJni::publish(env)) {
        return JNI_ERR;
    }
    return JNI_VERSION_1_6;
}

jclass JavaBridge::findClass(JNIEnv& env, const char* name) {
    const JavaLocalRef found(env, env.FindClass(name));
    if (found.get() == nullptr) {
        env.ExceptionDescribe();
        env.ExceptionClear();
        core::Log::error("The Java class \"{}\" of the haylen library is missing, so the engine cannot start. Keep the classes of the library in the rules of R8.", name);
        return nullptr;
    }
    return static_cast<jclass>(env.NewGlobalRef(found.get()));
}

jmethodID JavaBridge::findMethod(JNIEnv& env, jclass owner, const char* name, const char* signature) {
    const jmethodID found = env.GetStaticMethodID(owner, name, signature);
    if (found == nullptr) {
        env.ExceptionDescribe();
        env.ExceptionClear();
        core::Log::error("The static Java method \"{}\" of the haylen library is missing, so the engine cannot start. Keep the classes of the library in the rules of R8.", name);
    }
    return found;
}

void JavaBridge::dispatch(std::uint64_t id, std::string_view method, std::string_view paramsJson, std::span<const std::vector<std::byte>> buffers, std::span<const varn::dataview::DataView> views) {
    JNIEnv& env = getEnv();
    {
        const JavaLocalRef methodBytes = toBytes(env, method);
        const JavaLocalRef paramsBytes = toBytes(env, paramsJson);
        const JavaLocalRef arrays = toByteArrays(env, buffers);
        const JavaLocalRef handles = toViewHandles(env, views);
        env.CallStaticVoidMethod(bridgeClass, dispatchMethod, static_cast<jlong>(id), methodBytes.get(), paramsBytes.get(), arrays.get(), handles.get());
    }

    // A call that the Java registry never took would wait forever, so it fails at once.
    if (clearException(env, "dev.haylen.HaylenBridge.dispatch")) {
        const std::string message = "The call \"" + std::string(method) + "\" failed, because the Java method \"dev.haylen.HaylenBridge.dispatch\" threw an exception.";
        BridgeRelay::resolve(id, false, core::Json{{"message", message}, {"code", "exception"}}.dump());
    }
}

void JavaBridge::cancel(std::uint64_t id) {
    JNIEnv& env = getEnv();
    env.CallStaticVoidMethod(bridgeClass, cancelMethod, static_cast<jlong>(id));
    clearException(env, "dev.haylen.HaylenBridge.cancel");
}

void JavaBridge::setAppRunning(bool value) {
    JNIEnv& env = getEnv();
    env.CallStaticVoidMethod(bridgeClass, setAppRunningMethod, static_cast<jboolean>(value ? JNI_TRUE : JNI_FALSE));
    clearException(env, "dev.haylen.HaylenBridge.setAppRunning");
}

void JavaBridge::reportError(std::string_view reportJson) {
    JNIEnv& env = getEnv();
    env.CallStaticVoidMethod(pluginsClass, reportErrorMethod, toBytes(env, reportJson).get());
    clearException(env, "dev.haylen.HaylenPlugins.reportError");
}

const std::vector<std::string>& JavaBridge::getPlugins() noexcept {
    return plugins;
}

void JavaBridge::editText(std::string_view fieldJson) {
    JNIEnv& env = getEnv();
    env.CallStaticVoidMethod(editorClass, editMethod, toBytes(env, fieldJson).get());
    clearException(env, "dev.haylen.HaylenEditText.edit");
}

void JavaBridge::finishText() {
    JNIEnv& env = getEnv();
    env.CallStaticVoidMethod(editorClass, finishMethod);
    clearException(env, "dev.haylen.HaylenEditText.finish");
}

void JavaBridge::lockOrientation(int value) {
    JNIEnv& env = getEnv();
    env.CallStaticVoidMethod(activityClass, lockOrientationMethod, static_cast<jint>(value));
    clearException(env, "dev.haylen.HaylenActivity.lockOrientation");
}

void JavaBridge::setStatusBar(int value) {
    JNIEnv& env = getEnv();
    env.CallStaticVoidMethod(activityClass, setStatusBarMethod, static_cast<jint>(value));
    clearException(env, "dev.haylen.HaylenActivity.setStatusBar");
}

void JavaBridge::setSystemGestures(int value) {
    JNIEnv& env = getEnv();
    env.CallStaticVoidMethod(activityClass, setSystemGesturesMethod, static_cast<jint>(value));
    clearException(env, "dev.haylen.HaylenActivity.setSystemGestures");
}

void JavaBridge::keepScreenOn(bool value) {
    JNIEnv& env = getEnv();
    env.CallStaticVoidMethod(activityClass, keepScreenOnMethod, static_cast<jboolean>(value ? JNI_TRUE : JNI_FALSE));
    clearException(env, "dev.haylen.HaylenActivity.keepScreenOn");
}

void JavaBridge::captureBack(bool value) {
    JNIEnv& env = getEnv();
    env.CallStaticVoidMethod(activityClass, captureBackMethod, static_cast<jboolean>(value ? JNI_TRUE : JNI_FALSE));
    clearException(env, "dev.haylen.HaylenActivity.captureBack");
}

// The device reports nothing when Java fails, so every value of the info stays unknown.
std::string JavaBridge::getSystemInfo() {
    JNIEnv& env = getEnv();
    return callText(env, activityClass, systemInfoMethod, "dev.haylen.HaylenActivity.systemInfo", "{}");
}

Permission::Answer JavaBridge::setClipboard(std::string_view text) {
    JNIEnv& env = getEnv();
    const JavaLocalRef answer(env, static_cast<jbyteArray>(env.CallStaticObjectMethod(activityClass, setClipboardMethod, toBytes(env, text).get())));
    if (clearException(env, "dev.haylen.HaylenActivity.setClipboard")) {
        return {.state = Permission::State::Unsupported, .message = "The clipboard could not be written, because Java failed."};
    }
    return Permission::Answer::fromJson(core::Json::parse(toString(env, answer.get()), nullptr, false));
}

std::string JavaBridge::getClipboard() {
    JNIEnv& env = getEnv();
    return callText(env, activityClass, clipboardMethod, "dev.haylen.HaylenActivity.clipboard", "");
}

std::string JavaBridge::getNetworkRequirement() {
    JNIEnv& env = getEnv();
    return callText(env, activityClass, networkRequirementMethod, "dev.haylen.HaylenActivity.networkRequirement", "");
}

std::string JavaBridge::getDevelopmentServer() {
    JNIEnv& env = getEnv();
    return callText(env, activityClass, developmentServerMethod, "dev.haylen.HaylenActivity.developmentServer", "");
}

// The ids of the displays of Android are the numbers that the display manager gives them.
void JavaBridge::openOutput(std::uint32_t number, std::string_view display, std::string_view title) {
    JNIEnv& env = getEnv();
    env.CallStaticVoidMethod(outputsClass, openOutputMethod, static_cast<jint>(number), static_cast<jint>(std::stoi(std::string(display))), toBytes(env, title).get());
    if (clearException(env, "dev.haylen.HaylenOutputs.open")) {
        AndroidOutputs::receiveEnding(number, Output::Ending::Unsupported, "The activity could not open the presentation of the output.");
    }
}

void JavaBridge::closeOutput(std::uint32_t number) {
    JNIEnv& env = getEnv();
    env.CallStaticVoidMethod(outputsClass, closeOutputMethod, static_cast<jint>(number));
    clearException(env, "dev.haylen.HaylenOutputs.close");
}

void JavaBridge::checkPermission(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback) {
    callPermission(permissionStateMethod, "dev.haylen.HaylenPermissions.state", kind, std::move(callback));
}

void JavaBridge::requestPermission(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback) {
    callPermission(requestPermissionMethod, "dev.haylen.HaylenPermissions.request", kind, std::move(callback));
}

// A call that Java failed counts as held, so a failure explains nothing it cannot tell.
bool JavaBridge::isPermissionHeld(Permission::Kind kind) {
    JNIEnv& env = getEnv();
    const jboolean held = env.CallStaticBooleanMethod(permissionsClass, permissionHeldMethod, toBytes(env, Permission::kindName(kind)).get());
    return clearException(env, "dev.haylen.HaylenPermissions.isHeld") || held == JNI_TRUE;
}

// The callback waits outside the lock, since Java answers on the main thread, which may be the thread of the call.
void JavaBridge::callPermission(jmethodID method, std::string_view name, Permission::Kind kind, std::function<void(Permission::Answer answer)> callback) {
    std::int64_t request = 0;
    {
        const std::scoped_lock lock(permissionMutex);
        request = nextPermission++;
        permissionCallbacks.emplace(request, std::move(callback));
    }
    JNIEnv& env = getEnv();
    env.CallStaticVoidMethod(permissionsClass, method, static_cast<jlong>(request), toBytes(env, Permission::kindName(kind)).get());
    if (clearException(env, name)) {
        answerPermission(request, core::Json{{"state", "unsupported"}, {"message", std::format("The permission \"{}\" could not be read, because Java failed.", Permission::kindName(kind))}}.dump());
    }
}

void JavaBridge::answerPermission(std::int64_t request, std::string_view answerJson) {
    std::function<void(Permission::Answer)> callback;
    {
        const std::scoped_lock lock(permissionMutex);
        const auto found = permissionCallbacks.find(request);
        if (found == permissionCallbacks.end()) {
            return;
        }
        callback = std::move(found->second);
        permissionCallbacks.erase(found);
    }
    callback(Permission::Answer::fromJson(core::Json::parse(answerJson, nullptr, false)));
}

void JavaBridge::rumbleGamepad(std::int32_t device, float low, float high, std::int64_t milliseconds) {
    JNIEnv& env = getEnv();
    env.CallStaticVoidMethod(activityClass, rumbleGamepadMethod, static_cast<jint>(device), static_cast<jfloat>(low), static_cast<jfloat>(high), static_cast<jlong>(milliseconds));
    clearException(env, "dev.haylen.HaylenActivity.rumbleGamepad");
}

void JavaBridge::updateAccessibility(std::string_view treeJson) {
    JNIEnv& env = getEnv();
    env.CallStaticVoidMethod(accessibilityClass, updateAccessibilityMethod, toBytes(env, treeJson).get());
    clearException(env, "dev.haylen.HaylenAccessibilityTree.update");
}

void JavaBridge::announce(std::string_view text, bool interrupt) {
    JNIEnv& env = getEnv();
    env.CallStaticVoidMethod(accessibilityClass, announceMethod, toBytes(env, text).get(), static_cast<jboolean>(interrupt ? JNI_TRUE : JNI_FALSE));
    clearException(env, "dev.haylen.HaylenAccessibilityTree.announce");
}

void JavaBridge::updateMediaSession(std::string_view infoJson, std::span<const std::byte> artwork) {
    JNIEnv& env = getEnv();
    {
        const JavaLocalRef info = infoJson.empty() ? JavaLocalRef<jbyteArray>(env, nullptr) : toBytes(env, infoJson);
        const JavaLocalRef bytes = artwork.empty() ? JavaLocalRef<jbyteArray>(env, nullptr) : toBytes(env, std::string_view(reinterpret_cast<const char*>(artwork.data()), artwork.size()));
        env.CallStaticVoidMethod(mediaSessionClass, updateMediaSessionMethod, info.get(), bytes.get());
    }
    clearException(env, "dev.haylen.HaylenMediaSession.update");
}

void JavaBridge::setBackgroundAudio(bool value) {
    JNIEnv& env = getEnv();
    env.CallStaticVoidMethod(mediaSessionClass, setBackgroundAudioMethod, value ? JNI_TRUE : JNI_FALSE);
    clearException(env, "dev.haylen.HaylenMediaSession.setBackground");
}

void JavaBridge::openScreen(const ScreenRequest& request) {
    JNIEnv& env = getEnv();
    {
        const JavaLocalRef plugin = toBytes(env, request.plugin);
        const JavaLocalRef screen = toBytes(env, request.screen);
        const JavaLocalRef params = toBytes(env, request.params.json.dump());
        const JavaLocalRef buffers = toByteArrays(env, request.params.buffers);
        const JavaLocalRef handles = toViewHandles(env, request.params.views);
        const JavaLocalRef state = toBytes(env, request.state.dump());
        env.CallStaticVoidMethod(screensClass, openScreenMethod, static_cast<jlong>(request.id), plugin.get(), screen.get(), params.get(), buffers.get(), handles.get(), state.get(), static_cast<jboolean>(request.opaque ? JNI_TRUE : JNI_FALSE));
    }
    if (clearException(env, "dev.haylen.HaylenScreens.open")) {
        ScreenRelay::finish(request.id, false, core::Json{{"message", "The screen failed, because the Java method \"dev.haylen.HaylenScreens.open\" threw an exception."}, {"code", "exception"}}.dump());
    }
}

void JavaBridge::cancelScreen(std::uint64_t id) {
    JNIEnv& env = getEnv();
    env.CallStaticVoidMethod(screensClass, cancelScreenMethod, static_cast<jlong>(id));
    clearException(env, "dev.haylen.HaylenScreens.cancel");
}

// A pending exception would abort the next JNI call of the thread, so it goes to logcat with its stack and leaves an engine error that names the method. Returns whether the method left one.
bool JavaBridge::clearException(JNIEnv& env, std::string_view method) {
    if (env.ExceptionCheck() == JNI_FALSE) {
        return false;
    }
    env.ExceptionDescribe();
    env.ExceptionClear();
    core::Log::error("The Java method \"{}\" threw an exception, whose stack logcat shows under the tag \"System.err\".", method);
    return true;
}

std::string JavaBridge::toString(JNIEnv& env, jbyteArray bytes) {
    std::string result(static_cast<std::size_t>(env.GetArrayLength(bytes)), '\0');
    env.GetByteArrayRegion(bytes, 0, static_cast<jsize>(result.size()), reinterpret_cast<jbyte*>(result.data()));
    return result;
}

std::vector<std::string> JavaBridge::toStrings(JNIEnv& env, jobjectArray texts) {
    const jsize count = env.GetArrayLength(texts);
    std::vector<std::string> strings;
    strings.reserve(static_cast<std::size_t>(count));
    for (jsize index = 0; index < count; ++index) {
        const JavaLocalRef text(env, static_cast<jbyteArray>(env.GetObjectArrayElement(texts, index)));
        strings.push_back(toString(env, text.get()));
    }
    return strings;
}

// A direct `ByteBuffer` that Java sliced to its remaining bytes starts at its address and ends at its capacity, and anything else is a byte array.
// A buffer is a `byte[]` or a direct `ByteBuffer`. Arrays are told apart first, since `GetDirectBufferAddress` takes only a `java.nio.Buffer`, and the checks of JNI abort the app when it gets an array, as Android 10 does in debuggable apps.
std::vector<std::vector<std::byte>> JavaBridge::toBuffers(JNIEnv& env, jobjectArray buffers) {
    const jsize count = env.GetArrayLength(buffers);
    std::vector<std::vector<std::byte>> copies(static_cast<std::size_t>(count));
    for (jsize index = 0; index < count; ++index) {
        const JavaLocalRef buffer(env, env.GetObjectArrayElement(buffers, index));
        std::vector<std::byte>& copy = copies[static_cast<std::size_t>(index)];
        if (env.IsInstanceOf(buffer.get(), byteArrayClass) == JNI_TRUE) {
            const auto array = static_cast<jbyteArray>(buffer.get());
            copy.resize(static_cast<std::size_t>(env.GetArrayLength(array)));
            env.GetByteArrayRegion(array, 0, static_cast<jsize>(copy.size()), reinterpret_cast<jbyte*>(copy.data()));
            continue;
        }
        const auto* bytes = static_cast<const std::byte*>(env.GetDirectBufferAddress(buffer.get()));
        copy.assign(bytes, bytes + env.GetDirectBufferCapacity(buffer.get()));
    }
    return copies;
}

// Java may run out of memory for an array, which leaves `OutOfMemoryError` pending, so the exception is cleared and the call fails in C++ before any other call into Java.
JavaLocalRef<jbyteArray> JavaBridge::toBytes(JNIEnv& env, std::string_view text) {
    JavaLocalRef bytes(env, env.NewByteArray(static_cast<jsize>(text.size())));
    if (bytes.get() == nullptr) {
        clearException(env, "NewByteArray");
        throw std::runtime_error("Java has no memory for an array of " + std::to_string(text.size()) + " bytes.");
    }
    env.SetByteArrayRegion(bytes.get(), 0, static_cast<jsize>(text.size()), reinterpret_cast<const jbyte*>(text.data()));
    return bytes;
}

JavaLocalRef<jobjectArray> JavaBridge::toByteArrays(JNIEnv& env, std::span<const std::vector<std::byte>> buffers) {
    JavaLocalRef arrays(env, env.NewObjectArray(static_cast<jsize>(buffers.size()), byteArrayClass, nullptr));
    if (arrays.get() == nullptr) {
        clearException(env, "NewObjectArray");
        throw std::runtime_error("Java has no memory for a list of " + std::to_string(buffers.size()) + " buffers.");
    }
    for (std::size_t index = 0; index < buffers.size(); ++index) {
        const JavaLocalRef array = toBytes(env, std::string_view(reinterpret_cast<const char*>(buffers[index].data()), buffers[index].size()));
        env.SetObjectArrayElement(arrays.get(), static_cast<jsize>(index), array.get());
    }
    return arrays;
}

JavaLocalRef<jlongArray> JavaBridge::toViewHandles(JNIEnv& env, std::span<const varn::dataview::DataView> views) {
    JavaLocalRef handles(env, env.NewLongArray(static_cast<jsize>(views.size())));
    if (handles.get() == nullptr) {
        clearException(env, "NewLongArray");
        throw std::runtime_error("Java has no memory for a list of " + std::to_string(views.size()) + " data views.");
    }
    for (std::size_t index = 0; index < views.size(); ++index) {
        const auto handle = static_cast<jlong>(reinterpret_cast<std::uintptr_t>(views[index].handle()));
        env.SetLongArrayRegion(handles.get(), static_cast<jsize>(index), 1, &handle);
    }
    return handles;
}

jlong JavaBridge::shareView(jlong handle) noexcept {
    auto* lent = reinterpret_cast<varn_dataview*>(static_cast<std::uintptr_t>(handle));
    return static_cast<jlong>(reinterpret_cast<std::uintptr_t>(varn::dataview::DataView::share(lent).detach()));
}

std::string JavaBridge::callText(JNIEnv& env, jclass owner, jmethodID method, std::string_view name, std::string_view fallback) {
    const JavaLocalRef bytes(env, static_cast<jbyteArray>(env.CallStaticObjectMethod(owner, method)));
    if (clearException(env, name)) {
        return std::string(fallback);
    }
    return toString(env, bytes.get());
}

// A thread that this class attached keeps its attachment, which the key ends when the thread exits, so no call pays for attaching and every attachment is released. A thread that Java refuses to attach cannot call into Java, which is an error of the call.
JNIEnv& JavaBridge::getEnv() {
    JNIEnv* env = nullptr;
    const jint state = javaVm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (state == JNI_OK) {
        return *env;
    }

    if (state != JNI_EDETACHED) {
        throw std::runtime_error(std::format("Java refused the environment of this thread with the code {}, so the thread cannot call into Java.", state));
    }

    const jint attached = javaVm->AttachCurrentThread(&env, nullptr);
    if (attached != JNI_OK) {
        throw std::runtime_error(std::format("Java could not attach this thread, which failed with the code {}, so the thread cannot call into Java.", attached));
    }
    pthread_setspecific(attachedThreads, env);
    return *env;
}

void JavaBridge::detachThread(void*) {
    javaVm->DetachCurrentThread();
}

} // namespace haylen::platform
