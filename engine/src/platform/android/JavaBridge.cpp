#include "platform/android/JavaBridge.hpp"

#include "varn/http/AndroidHttpBridge.h"

namespace haylen::platform {

JavaVM* JavaBridge::javaVm = nullptr;
jclass JavaBridge::bridgeClass = nullptr;
jmethodID JavaBridge::dispatchMethod = nullptr;
jmethodID JavaBridge::cancelMethod = nullptr;
jclass JavaBridge::editorClass = nullptr;
jmethodID JavaBridge::editMethod = nullptr;
jmethodID JavaBridge::finishMethod = nullptr;
jclass JavaBridge::activityClass = nullptr;
jmethodID JavaBridge::lockOrientationMethod = nullptr;
jmethodID JavaBridge::captureBackMethod = nullptr;

jint JavaBridge::load(JavaVM* vm) {
    JNIEnv* env = nullptr;
    vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    javaVm = vm;
    // A missing class leaves an exception pending, so the lookup stops at the first one.
    bridgeClass = findClass(*env, "dev/haylen/HaylenBridge");
    editorClass = bridgeClass != nullptr ? findClass(*env, "dev/haylen/HaylenEditText") : nullptr;
    activityClass = editorClass != nullptr ? findClass(*env, "dev/haylen/HaylenActivity") : nullptr;
    if (activityClass == nullptr) {
        return JNI_ERR;
    }
    dispatchMethod = env->GetStaticMethodID(bridgeClass, "dispatch", "(J[B[B)V");
    cancelMethod = env->GetStaticMethodID(bridgeClass, "cancel", "(J)V");
    editMethod = env->GetStaticMethodID(editorClass, "edit", "([B)V");
    finishMethod = env->GetStaticMethodID(editorClass, "finish", "()V");
    lockOrientationMethod = env->GetStaticMethodID(activityClass, "lockOrientation", "(I)V");
    captureBackMethod = env->GetStaticMethodID(activityClass, "captureBack", "(Z)V");
    varn::http::client::AndroidHttpBridge::publish(vm);
    return JNI_VERSION_1_6;
}

jclass JavaBridge::findClass(JNIEnv& env, const char* name) {
    const jclass found = env.FindClass(name);
    return found != nullptr ? static_cast<jclass>(env.NewGlobalRef(found)) : nullptr;
}

void JavaBridge::dispatch(std::uint64_t id, std::string_view method, std::string_view paramsJson) {
    const Thread thread;
    JNIEnv& env = thread.getEnv();
    const jbyteArray methodBytes = toBytes(env, method);
    const jbyteArray paramsBytes = toBytes(env, paramsJson);
    env.CallStaticVoidMethod(bridgeClass, dispatchMethod, static_cast<jlong>(id), methodBytes, paramsBytes);
    env.DeleteLocalRef(methodBytes);
    env.DeleteLocalRef(paramsBytes);
}

void JavaBridge::cancel(std::uint64_t id) {
    const Thread thread;
    thread.getEnv().CallStaticVoidMethod(bridgeClass, cancelMethod, static_cast<jlong>(id));
}

void JavaBridge::editText(std::string_view fieldJson) {
    const Thread thread;
    JNIEnv& env = thread.getEnv();
    const jbyteArray bytes = toBytes(env, fieldJson);
    env.CallStaticVoidMethod(editorClass, editMethod, bytes);
    env.DeleteLocalRef(bytes);
}

void JavaBridge::finishText() {
    const Thread thread;
    thread.getEnv().CallStaticVoidMethod(editorClass, finishMethod);
}

void JavaBridge::lockOrientation(int value) {
    const Thread thread;
    thread.getEnv().CallStaticVoidMethod(activityClass, lockOrientationMethod, static_cast<jint>(value));
}

void JavaBridge::captureBack(bool value) {
    const Thread thread;
    thread.getEnv().CallStaticVoidMethod(activityClass, captureBackMethod, static_cast<jboolean>(value ? JNI_TRUE : JNI_FALSE));
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

JavaBridge::Thread::Thread() {
    if (javaVm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_EDETACHED) {
        javaVm->AttachCurrentThread(&env, nullptr);
        attached = true;
    }
}

JavaBridge::Thread::~Thread() {
    if (attached) {
        javaVm->DetachCurrentThread();
    }
}

} // namespace haylen::platform
