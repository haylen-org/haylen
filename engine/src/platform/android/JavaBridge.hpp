#pragma once

#include <jni.h>
#include <pthread.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "haylen/platform/ScreenRequest.hpp"

namespace haylen::platform {

// Calls into the Java side of the `haylen` Android library: the platform bridge of `dev.haylen.HaylenBridge`, the plugins of `dev.haylen.HaylenPlugins`, the hidden text field of `dev.haylen.HaylenEditText`, the orientation lock, the back callback and the system services of `dev.haylen.HaylenActivity`, the native dialogs of `dev.haylen.HaylenDialogs` and the screens of plugins of `dev.haylen.HaylenScreens`. A native thread attaches to the Java VM the first time it calls Java and detaches when it ends. An exception that a Java method throws never stays pending: it is logged with its stack and cleared, and a call that expects an answer gets a failed one.
class JavaBridge final {
  public:
    // Resolves the Java classes the native side calls, including the HTTP transport of Varn, which `JNI_OnLoad` does because the app class loader is still in reach there, and reads the plugins that the process loaded before. Returns the JNI version, or `JNI_ERR` when the APK lacks a class.
    static jint load(JavaVM* vm);

    // Hands a call to the Java handler registry with the byte buffers of its parameters as byte arrays, and the registry answers through the bridge relay.
    static void dispatch(std::uint64_t id, std::string_view method, std::string_view paramsJson, std::span<const std::vector<std::byte>> buffers);

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

    // What Android tells about the device, as the JSON that `AndroidDeviceInfo` reads.
    [[nodiscard]] static std::string getSystemInfo();

    // The sentence that engine-owned network errors end with while the app does not declare the permission `INTERNET`, or an empty text.
    [[nodiscard]] static std::string getNetworkRequirement();

    // Opens the url with the app that handles it on the main thread, and `answerUrl` hands the callback whether one took it.
    static void openUrl(std::string_view url, std::function<void(bool opened)> callback);
    static void answerUrl(std::int64_t request, bool opened);

    static void vibrate(std::int64_t milliseconds);

    // Shows the dialog that the JSON of `AndroidDialogJson` describes, with the data of a save as bytes, and the Java side answers through the dialog relay. A dialog that Java never took fails at once.
    static void showDialog(std::uint64_t id, std::string_view requestJson, std::span<const std::uint8_t> data);
    static void cancelDialog(std::uint64_t id);

    // Opens the screen of a plugin, and the Java side ends it through the screen relay. A screen that Java never took fails at once.
    static void openScreen(const ScreenRequest& request);
    static void cancelScreen(std::uint64_t id);

    // Text crosses JNI as UTF-8 bytes, because the JNI string functions use a modified UTF-8 that breaks characters outside the Basic Multilingual Plane, such as emoji.
    [[nodiscard]] static std::string toString(JNIEnv& env, jbyteArray bytes);

    // Reads an array of texts that Java hands over as UTF-8 byte arrays.
    [[nodiscard]] static std::vector<std::string> toStrings(JNIEnv& env, jobjectArray texts);

    // Copies the buffers that Java hands over, each a byte array or a direct `ByteBuffer`, once.
    [[nodiscard]] static std::vector<std::vector<std::byte>> toBuffers(JNIEnv& env, jobjectArray buffers);

  private:
    [[nodiscard]] static JNIEnv& getEnv();
    static void detachThread(void* env);

    [[nodiscard]] static jbyteArray toBytes(JNIEnv& env, std::string_view text);
    [[nodiscard]] static jobjectArray toByteArrays(JNIEnv& env, std::span<const std::vector<std::byte>> buffers);

    [[nodiscard]] static jclass findClass(JNIEnv& env, const char* name);

    static bool clearException(JNIEnv& env, std::string_view method);

    static JavaVM* javaVm;
    static pthread_key_t attachedThreads;
    static std::vector<std::string>& plugins;
    static jclass byteArrayClass;
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
    static jmethodID systemInfoMethod;
    static jmethodID openUrlMethod;
    static jmethodID vibrateMethod;
    static jmethodID networkRequirementMethod;
    static jclass dialogsClass;
    static jmethodID showDialogMethod;
    static jmethodID cancelDialogMethod;
    static jclass screensClass;
    static jmethodID openScreenMethod;
    static jmethodID cancelScreenMethod;

    // The callbacks of the urls that wait for the answer of Java, by request.
    static std::mutex& urlMutex;
    static std::unordered_map<std::int64_t, std::function<void(bool)>>& urlCallbacks;
    static std::int64_t nextUrl;
};

} // namespace haylen::platform
