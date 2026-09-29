#pragma once

#include <jni.h>
#include <pthread.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace haylen::platform {

// Calls into the Java side of the haylen Android library: the platform bridge of dev.haylen.HaylenBridge, the plugins of dev.haylen.HaylenPlugins, the hidden text field of dev.haylen.HaylenEditText and the orientation lock and back callback of dev.haylen.HaylenActivity. A native thread attaches to the Java VM the first time it calls Java and detaches when it ends.
class JavaBridge final {
  public:
    // Resolves the Java classes the native side calls, including the HTTP transport of Varn, which JNI_OnLoad does because the app class loader is still in reach there, and reads the plugins that the process loaded before. Returns the JNI version, or JNI_ERR when the APK lacks a class.
    static jint load(JavaVM* vm);

    // Hands a call to the Java handler registry, which answers through the bridge relay.
    static void dispatch(std::uint64_t id, std::string_view method, std::string_view paramsJson);

    // Tells the Java handler of a call that the app gave it up.
    static void cancel(std::uint64_t id);

    // Tells the Java bridge whether an app runs, which is when the engine takes native events. Java keeps the events meanwhile.
    static void setAppRunning(bool value);

    // Hands the JSON report of the error that stopped the app to the plugins.
    static void reportError(std::string_view reportJson);

    // The ids of the plugins whose native part the process loaded.
    [[nodiscard]] static const std::vector<std::string>& getPlugins() noexcept;

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
    [[nodiscard]] static JNIEnv& getEnv();
    static void detachThread(void* env);

    [[nodiscard]] static jbyteArray toBytes(JNIEnv& env, std::string_view text);

    [[nodiscard]] static jclass findClass(JNIEnv& env, const char* name);

    static JavaVM* javaVm;
    static pthread_key_t attachedThreads;
    static std::vector<std::string>& plugins;
    static jclass bridgeClass;
    static jmethodID dispatchMethod;
    static jmethodID cancelMethod;
    static jmethodID setAppRunningMethod;
    static jclass pluginsClass;
    static jmethodID reportErrorMethod;
    static jclass editorClass;
    static jmethodID editMethod;
    static jmethodID finishMethod;
    static jclass activityClass;
    static jmethodID lockOrientationMethod;
    static jmethodID captureBackMethod;
};

} // namespace haylen::platform
