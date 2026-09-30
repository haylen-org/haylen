#include "platform/android/JavaBridge.hpp"

#include <utility>

#include "haylen/core/Json.hpp"
#include "varn/http/AndroidHttpBridge.h"

namespace haylen::platform {

JavaVM* JavaBridge::javaVm = nullptr;
pthread_key_t JavaBridge::attachedThreads{};
std::vector<std::string>& JavaBridge::plugins = *new std::vector<std::string>();
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
std::mutex& JavaBridge::urlMutex = *new std::mutex();
std::unordered_map<std::int64_t, std::function<void(bool)>>& JavaBridge::urlCallbacks = *new std::unordered_map<std::int64_t, std::function<void(bool)>>();
std::int64_t JavaBridge::nextUrl = 1;

jint JavaBridge::load(JavaVM* vm) {
    JNIEnv* env = nullptr;
    vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    javaVm = vm;
    pthread_key_create(&attachedThreads, &JavaBridge::detachThread);

    // A missing class leaves an exception pending, so the lookup stops at the first one.
    bridgeClass = findClass(*env, "dev/haylen/HaylenBridge");
    pluginsClass = bridgeClass != nullptr ? findClass(*env, "dev/haylen/HaylenPlugins") : nullptr;
    editorClass = pluginsClass != nullptr ? findClass(*env, "dev/haylen/HaylenEditText") : nullptr;
    activityClass = editorClass != nullptr ? findClass(*env, "dev/haylen/HaylenActivity") : nullptr;
    if (activityClass == nullptr) {
        return JNI_ERR;
    }
    dispatchMethod = env->GetStaticMethodID(bridgeClass, "dispatch", "(J[B[B)V");
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

    // The plugins load when the process starts, before any activity loads this library, so their list is final here.
    const auto ids = static_cast<jbyteArray>(env->CallStaticObjectMethod(pluginsClass, env->GetStaticMethodID(pluginsClass, "ids", "()[B")));
    plugins = core::Json::parse(toString(*env, ids)).get<std::vector<std::string>>();
    env->DeleteLocalRef(ids);

    varn::http::client::AndroidHttpBridge::publish(vm);
    return JNI_VERSION_1_6;
}

jclass JavaBridge::findClass(JNIEnv& env, const char* name) {
    const jclass found = env.FindClass(name);
    return found != nullptr ? static_cast<jclass>(env.NewGlobalRef(found)) : nullptr;
}

void JavaBridge::dispatch(std::uint64_t id, std::string_view method, std::string_view paramsJson) {
    JNIEnv& env = getEnv();
    const jbyteArray methodBytes = toBytes(env, method);
    const jbyteArray paramsBytes = toBytes(env, paramsJson);
    env.CallStaticVoidMethod(bridgeClass, dispatchMethod, static_cast<jlong>(id), methodBytes, paramsBytes);
    env.DeleteLocalRef(methodBytes);
    env.DeleteLocalRef(paramsBytes);
}

void JavaBridge::cancel(std::uint64_t id) {
    getEnv().CallStaticVoidMethod(bridgeClass, cancelMethod, static_cast<jlong>(id));
}

void JavaBridge::setAppRunning(bool value) {
    getEnv().CallStaticVoidMethod(bridgeClass, setAppRunningMethod, static_cast<jboolean>(value ? JNI_TRUE : JNI_FALSE));
}

void JavaBridge::reportError(std::string_view reportJson) {
    JNIEnv& env = getEnv();
    const jbyteArray bytes = toBytes(env, reportJson);
    env.CallStaticVoidMethod(pluginsClass, reportErrorMethod, bytes);
    env.DeleteLocalRef(bytes);
}

const std::vector<std::string>& JavaBridge::getPlugins() noexcept {
    return plugins;
}

void JavaBridge::editText(std::string_view fieldJson) {
    JNIEnv& env = getEnv();
    const jbyteArray bytes = toBytes(env, fieldJson);
    env.CallStaticVoidMethod(editorClass, editMethod, bytes);
    env.DeleteLocalRef(bytes);
}

void JavaBridge::finishText() {
    getEnv().CallStaticVoidMethod(editorClass, finishMethod);
}

void JavaBridge::lockOrientation(int value) {
    getEnv().CallStaticVoidMethod(activityClass, lockOrientationMethod, static_cast<jint>(value));
}

void JavaBridge::captureBack(bool value) {
    getEnv().CallStaticVoidMethod(activityClass, captureBackMethod, static_cast<jboolean>(value ? JNI_TRUE : JNI_FALSE));
}

std::string JavaBridge::getSystemInfo() {
    JNIEnv& env = getEnv();
    const auto bytes = static_cast<jbyteArray>(env.CallStaticObjectMethod(activityClass, systemInfoMethod));
    std::string json = toString(env, bytes);
    env.DeleteLocalRef(bytes);
    return json;
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
    getEnv().CallStaticVoidMethod(activityClass, vibrateMethod, static_cast<jlong>(milliseconds));
}

std::string JavaBridge::toString(JNIEnv& env, jbyteArray bytes) {
    std::string result(static_cast<std::size_t>(env.GetArrayLength(bytes)), '\0');
    env.GetByteArrayRegion(bytes, 0, static_cast<jsize>(result.size()), reinterpret_cast<jbyte*>(result.data()));
    return result;
}

jbyteArray JavaBridge::toBytes(JNIEnv& env, std::string_view text) {
    const jbyteArray bytes = env.NewByteArray(static_cast<jsize>(text.size()));
    env.SetByteArrayRegion(bytes, 0, static_cast<jsize>(text.size()), reinterpret_cast<const jbyte*>(text.data()));
    return bytes;
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
