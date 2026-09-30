#include "platform/android/JavaBridge.hpp"

#include <utility>

#include "haylen/core/Json.hpp"
#include "haylen/core/Log.hpp"
#include "platform/BridgeRelay.hpp"
#include "platform/DialogRelay.hpp"
#include "platform/ScreenRelay.hpp"
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
jmethodID JavaBridge::captureBackMethod = nullptr;
jmethodID JavaBridge::systemInfoMethod = nullptr;
jmethodID JavaBridge::openUrlMethod = nullptr;
jmethodID JavaBridge::vibrateMethod = nullptr;
jmethodID JavaBridge::networkRequirementMethod = nullptr;
jclass JavaBridge::dialogsClass = nullptr;
jmethodID JavaBridge::showDialogMethod = nullptr;
jmethodID JavaBridge::cancelDialogMethod = nullptr;
jclass JavaBridge::screensClass = nullptr;
jmethodID JavaBridge::openScreenMethod = nullptr;
jmethodID JavaBridge::cancelScreenMethod = nullptr;
std::mutex& JavaBridge::urlMutex = *new std::mutex();
std::unordered_map<std::int64_t, std::function<void(bool)>>& JavaBridge::urlCallbacks = *new std::unordered_map<std::int64_t, std::function<void(bool)>>();
std::int64_t JavaBridge::nextUrl = 1;

jint JavaBridge::load(JavaVM* vm) {
    JNIEnv* env = nullptr;
    vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    javaVm = vm;
    pthread_key_create(&attachedThreads, &JavaBridge::detachThread);

    // A missing class leaves an exception pending, so the lookup stops at the first one.
    byteArrayClass = findClass(*env, "[B");
    bridgeClass = findClass(*env, "dev/haylen/HaylenBridge");
    pluginsClass = bridgeClass != nullptr ? findClass(*env, "dev/haylen/HaylenPlugins") : nullptr;
    editorClass = pluginsClass != nullptr ? findClass(*env, "dev/haylen/HaylenEditText") : nullptr;
    activityClass = editorClass != nullptr ? findClass(*env, "dev/haylen/HaylenActivity") : nullptr;
    dialogsClass = activityClass != nullptr ? findClass(*env, "dev/haylen/HaylenDialogs") : nullptr;
    screensClass = dialogsClass != nullptr ? findClass(*env, "dev/haylen/HaylenScreens") : nullptr;
    if (screensClass == nullptr) {
        return JNI_ERR;
    }
    dispatchMethod = env->GetStaticMethodID(bridgeClass, "dispatch", "(J[B[B[[B)V");
    cancelMethod = env->GetStaticMethodID(bridgeClass, "cancel", "(J)V");
    setAppRunningMethod = env->GetStaticMethodID(bridgeClass, "setAppRunning", "(Z)V");
    reportErrorMethod = env->GetStaticMethodID(pluginsClass, "reportError", "([B)V");
    editMethod = env->GetStaticMethodID(editorClass, "edit", "([B)V");
    finishMethod = env->GetStaticMethodID(editorClass, "finish", "()V");
    lockOrientationMethod = env->GetStaticMethodID(activityClass, "lockOrientation", "(I)V");
    captureBackMethod = env->GetStaticMethodID(activityClass, "captureBack", "(Z)V");
    systemInfoMethod = env->GetStaticMethodID(activityClass, "systemInfo", "()[B");
    openUrlMethod = env->GetStaticMethodID(activityClass, "openUrl", "(J[B)V");
    vibrateMethod = env->GetStaticMethodID(activityClass, "vibrate", "(J)V");
    networkRequirementMethod = env->GetStaticMethodID(activityClass, "networkRequirement", "()[B");
    showDialogMethod = env->GetStaticMethodID(dialogsClass, "show", "(J[B[B)V");
    cancelDialogMethod = env->GetStaticMethodID(dialogsClass, "cancel", "(J)V");
    openScreenMethod = env->GetStaticMethodID(screensClass, "open", "(J[B[B[B[[B[BZ)V");
    cancelScreenMethod = env->GetStaticMethodID(screensClass, "cancel", "(J)V");

    // The plugins load when the process starts, before any activity loads this library, so their list is final here.
    const auto ids = static_cast<jbyteArray>(env->CallStaticObjectMethod(pluginsClass, env->GetStaticMethodID(pluginsClass, "ids", "()[B")));
    if (!clearException(*env, "dev.haylen.HaylenPlugins.ids")) {
        plugins = core::Json::parse(toString(*env, ids)).get<std::vector<std::string>>();
        env->DeleteLocalRef(ids);
    }

    varn::http::client::AndroidHttpBridge::publish(vm);
    return JNI_VERSION_1_6;
}

jclass JavaBridge::findClass(JNIEnv& env, const char* name) {
    const jclass found = env.FindClass(name);
    return found != nullptr ? static_cast<jclass>(env.NewGlobalRef(found)) : nullptr;
}

void JavaBridge::dispatch(std::uint64_t id, std::string_view method, std::string_view paramsJson, std::span<const std::vector<std::byte>> buffers) {
    JNIEnv& env = getEnv();
    const jbyteArray methodBytes = toBytes(env, method);
    const jbyteArray paramsBytes = toBytes(env, paramsJson);
    const jobjectArray arrays = toByteArrays(env, buffers);
    env.CallStaticVoidMethod(bridgeClass, dispatchMethod, static_cast<jlong>(id), methodBytes, paramsBytes, arrays);
    env.DeleteLocalRef(methodBytes);
    env.DeleteLocalRef(paramsBytes);
    env.DeleteLocalRef(arrays);

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
    const jbyteArray bytes = toBytes(env, reportJson);
    env.CallStaticVoidMethod(pluginsClass, reportErrorMethod, bytes);
    env.DeleteLocalRef(bytes);
    clearException(env, "dev.haylen.HaylenPlugins.reportError");
}

const std::vector<std::string>& JavaBridge::getPlugins() noexcept {
    return plugins;
}

void JavaBridge::editText(std::string_view fieldJson) {
    JNIEnv& env = getEnv();
    const jbyteArray bytes = toBytes(env, fieldJson);
    env.CallStaticVoidMethod(editorClass, editMethod, bytes);
    env.DeleteLocalRef(bytes);
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

void JavaBridge::captureBack(bool value) {
    JNIEnv& env = getEnv();
    env.CallStaticVoidMethod(activityClass, captureBackMethod, static_cast<jboolean>(value ? JNI_TRUE : JNI_FALSE));
    clearException(env, "dev.haylen.HaylenActivity.captureBack");
}

// The device reports nothing when Java fails, so every value of the info stays unknown.
std::string JavaBridge::getSystemInfo() {
    JNIEnv& env = getEnv();
    const auto bytes = static_cast<jbyteArray>(env.CallStaticObjectMethod(activityClass, systemInfoMethod));
    if (clearException(env, "dev.haylen.HaylenActivity.systemInfo")) {
        return "{}";
    }
    std::string json = toString(env, bytes);
    env.DeleteLocalRef(bytes);
    return json;
}

std::string JavaBridge::getNetworkRequirement() {
    JNIEnv& env = getEnv();
    const auto bytes = static_cast<jbyteArray>(env.CallStaticObjectMethod(activityClass, networkRequirementMethod));
    if (clearException(env, "dev.haylen.HaylenActivity.networkRequirement")) {
        return {};
    }
    std::string sentence = toString(env, bytes);
    env.DeleteLocalRef(bytes);
    return sentence;
}

// The callback waits outside the lock, because Java answers at once, from inside the call, when no activity runs.
void JavaBridge::openUrl(std::string_view url, std::function<void(bool opened)> callback) {
    std::int64_t request = 0;
    {
        const std::scoped_lock lock(urlMutex);
        request = nextUrl++;
        urlCallbacks.emplace(request, std::move(callback));
    }
    JNIEnv& env = getEnv();
    const jbyteArray bytes = toBytes(env, url);
    env.CallStaticVoidMethod(activityClass, openUrlMethod, static_cast<jlong>(request), bytes);
    env.DeleteLocalRef(bytes);
    if (clearException(env, "dev.haylen.HaylenActivity.openUrl")) {
        answerUrl(request, false);
    }
}

void JavaBridge::answerUrl(std::int64_t request, bool opened) {
    std::function<void(bool)> callback;
    {
        const std::scoped_lock lock(urlMutex);
        const auto found = urlCallbacks.find(request);
        if (found == urlCallbacks.end()) {
            return;
        }
        callback = std::move(found->second);
        urlCallbacks.erase(found);
    }
    callback(opened);
}

void JavaBridge::vibrate(std::int64_t milliseconds) {
    JNIEnv& env = getEnv();
    env.CallStaticVoidMethod(activityClass, vibrateMethod, static_cast<jlong>(milliseconds));
    clearException(env, "dev.haylen.HaylenActivity.vibrate");
}

void JavaBridge::showDialog(std::uint64_t id, std::string_view requestJson, std::span<const std::uint8_t> data) {
    JNIEnv& env = getEnv();
    const jbyteArray request = toBytes(env, requestJson);
    const jbyteArray bytes = env.NewByteArray(static_cast<jsize>(data.size()));
    env.SetByteArrayRegion(bytes, 0, static_cast<jsize>(data.size()), reinterpret_cast<const jbyte*>(data.data()));
    env.CallStaticVoidMethod(dialogsClass, showDialogMethod, static_cast<jlong>(id), request, bytes);
    env.DeleteLocalRef(request);
    env.DeleteLocalRef(bytes);
    if (clearException(env, "dev.haylen.HaylenDialogs.show")) {
        DialogRelay::resolve(id, {.failure = DialogResult::Failure{.code = DialogResult::Code::Failed, .message = "The dialog failed, because the Java method \"dev.haylen.HaylenDialogs.show\" threw an exception."}});
    }
}

void JavaBridge::cancelDialog(std::uint64_t id) {
    JNIEnv& env = getEnv();
    env.CallStaticVoidMethod(dialogsClass, cancelDialogMethod, static_cast<jlong>(id));
    clearException(env, "dev.haylen.HaylenDialogs.cancel");
}

void JavaBridge::openScreen(const ScreenRequest& request) {
    JNIEnv& env = getEnv();
    const jbyteArray plugin = toBytes(env, request.plugin);
    const jbyteArray screen = toBytes(env, request.screen);
    const jbyteArray params = toBytes(env, request.params.json.dump());
    const jobjectArray buffers = toByteArrays(env, request.params.buffers);
    const jbyteArray state = toBytes(env, request.state.dump());
    env.CallStaticVoidMethod(screensClass, openScreenMethod, static_cast<jlong>(request.id), plugin, screen, params, buffers, state, static_cast<jboolean>(request.opaque ? JNI_TRUE : JNI_FALSE));
    env.DeleteLocalRef(plugin);
    env.DeleteLocalRef(screen);
    env.DeleteLocalRef(params);
    env.DeleteLocalRef(buffers);
    env.DeleteLocalRef(state);
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
        const auto text = static_cast<jbyteArray>(env.GetObjectArrayElement(texts, index));
        strings.push_back(toString(env, text));
        env.DeleteLocalRef(text);
    }
    return strings;
}

// A direct `ByteBuffer` that Java sliced to its remaining bytes starts at its address and ends at its capacity, and anything else is a byte array.
std::vector<std::vector<std::byte>> JavaBridge::toBuffers(JNIEnv& env, jobjectArray buffers) {
    const jsize count = env.GetArrayLength(buffers);
    std::vector<std::vector<std::byte>> copies(static_cast<std::size_t>(count));
    for (jsize index = 0; index < count; ++index) {
        const jobject buffer = env.GetObjectArrayElement(buffers, index);
        std::vector<std::byte>& copy = copies[static_cast<std::size_t>(index)];
        if (const void* address = env.GetDirectBufferAddress(buffer)) {
            const auto* bytes = static_cast<const std::byte*>(address);
            copy.assign(bytes, bytes + env.GetDirectBufferCapacity(buffer));
        } else {
            const auto array = static_cast<jbyteArray>(buffer);
            copy.resize(static_cast<std::size_t>(env.GetArrayLength(array)));
            env.GetByteArrayRegion(array, 0, static_cast<jsize>(copy.size()), reinterpret_cast<jbyte*>(copy.data()));
        }
        env.DeleteLocalRef(buffer);
    }
    return copies;
}

jbyteArray JavaBridge::toBytes(JNIEnv& env, std::string_view text) {
    const jbyteArray bytes = env.NewByteArray(static_cast<jsize>(text.size()));
    env.SetByteArrayRegion(bytes, 0, static_cast<jsize>(text.size()), reinterpret_cast<const jbyte*>(text.data()));
    return bytes;
}

jobjectArray JavaBridge::toByteArrays(JNIEnv& env, std::span<const std::vector<std::byte>> buffers) {
    const jobjectArray arrays = env.NewObjectArray(static_cast<jsize>(buffers.size()), byteArrayClass, nullptr);
    for (std::size_t index = 0; index < buffers.size(); ++index) {
        const jbyteArray array = env.NewByteArray(static_cast<jsize>(buffers[index].size()));
        env.SetByteArrayRegion(array, 0, static_cast<jsize>(buffers[index].size()), reinterpret_cast<const jbyte*>(buffers[index].data()));
        env.SetObjectArrayElement(arrays, static_cast<jsize>(index), array);
        env.DeleteLocalRef(array);
    }
    return arrays;
}

// A thread that this class attached keeps its attachment, which the key ends when the thread exits, so no call pays for attaching and every attachment is released.
JNIEnv& JavaBridge::getEnv() {
    JNIEnv* env = nullptr;
    if (javaVm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_EDETACHED) {
        javaVm->AttachCurrentThread(&env, nullptr);
        pthread_setspecific(attachedThreads, env);
    }
    return *env;
}

void JavaBridge::detachThread(void*) {
    javaVm->DetachCurrentThread();
}

} // namespace haylen::platform
