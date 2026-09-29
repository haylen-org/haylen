#pragma once

#include <jni.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace haylen::platform {

// Calls into the Java side of the haylen Android library: the platform bridge of dev.haylen.HaylenBridge, the hidden text field of dev.haylen.HaylenEditText and the orientation lock and back callback of dev.haylen.HaylenActivity.
class JavaBridge final {
  public:
    // Resolves the Java classes the native side calls, including the HTTP transport of Varn, which JNI_OnLoad does because the app class loader is still in reach there. Returns the JNI version, or JNI_ERR when the APK lacks the bridge class.
    static jint load(JavaVM* vm);

    // Hands a call to the Java handler registry, which answers through the bridge relay.
    static void dispatch(std::uint64_t id, std::string_view method, std::string_view paramsJson);

    // Hands a text field as JSON to the hidden text field, or lets it go, from the frame thread.
    static void editText(std::string_view fieldJson);
    static void finishText();

    // Takes 0 for landscape, 1 for portrait and 2 for any orientation.
    static void lockOrientation(int value);

    // Tells the activity whether the app takes the back button, which on Android 13 and later decides whether back reaches the app or leaves it with the back animation of the system.
    static void captureBack(bool value);

    // Text crosses JNI as UTF-8 bytes, because the JNI string functions use a modified UTF-8 that breaks characters outside the Basic Multilingual Plane, such as emoji.
    [[nodiscard]] static std::string toString(JNIEnv& env, jbyteArray bytes);

  private:
    // Attaches the calling thread to the Java VM for the duration of a call.
    class Thread final {
      public:
        Thread();
        ~Thread();

        Thread(const Thread&) = delete;
        Thread& operator=(const Thread&) = delete;

        [[nodiscard]] JNIEnv& getEnv() const noexcept {
            return *env;
        }

      private:
        JNIEnv* env = nullptr;
        bool attached = false;
    };

    [[nodiscard]] static jbyteArray toBytes(JNIEnv& env, std::string_view text);

    [[nodiscard]] static jclass findClass(JNIEnv& env, const char* name);

    static JavaVM* javaVm;
    static jclass bridgeClass;
    static jmethodID dispatchMethod;
    static jclass editorClass;
    static jmethodID editMethod;
    static jmethodID finishMethod;
    static jclass activityClass;
    static jmethodID lockOrientationMethod;
    static jmethodID captureBackMethod;
};

} // namespace haylen::platform
