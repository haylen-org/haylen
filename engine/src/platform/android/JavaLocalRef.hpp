#pragma once

#include <jni.h>

#include <utility>

namespace haylen::platform {

// Deletes a local reference of Java when it leaves its scope. The frame thread is a native thread that never returns to Java, so a local reference lives until the thread ends unless it is deleted, also when a call fails half way.
template <typename T> class JavaLocalRef final {
  public:
    JavaLocalRef(JNIEnv& owner, T value) noexcept : env(&owner), reference(value) {}

    ~JavaLocalRef() {
        if (reference != nullptr) {
            env->DeleteLocalRef(reference);
        }
    }

    JavaLocalRef(JavaLocalRef&& other) noexcept : env(other.env), reference(std::exchange(other.reference, nullptr)) {}
    JavaLocalRef(const JavaLocalRef&) = delete;
    JavaLocalRef& operator=(const JavaLocalRef&) = delete;
    JavaLocalRef& operator=(JavaLocalRef&&) = delete;

    [[nodiscard]] T get() const noexcept {
        return reference;
    }

  private:
    JNIEnv* env;
    T reference;
};

} // namespace haylen::platform
