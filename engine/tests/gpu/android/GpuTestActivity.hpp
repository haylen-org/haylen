#pragma once

#include <android/native_activity.h>
#include <jni.h>

#include <atomic>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace haylen::test {

// Runs the GPU tests as an app of Android, since a program that adb starts reaches the Vulkan driver of some devices only from an app. The activity takes the variables of the tests from the string array extra `environment` of its intent, and hands those of GoogleTest to it as their arguments, copies the reference images from its assets into its files and runs the tests on a thread of their own, whose output reaches the log under the tag `haylen-gpu-tests`. The tests write their results as `results.xml` next to the images of the scenes that differ, and the app finishes its activity and quits with their status.
class GpuTestActivity final {
  public:
    GpuTestActivity() = delete;

    static void create(ANativeActivity* activity);
    [[nodiscard]] static std::filesystem::path getFilesDirectory();

  private:
    static constexpr const char* kLogTag = "haylen-gpu-tests";

    static inline bool started = false;
    static inline std::atomic<ANativeActivity*> shown = nullptr;
    static inline std::atomic<bool> finished = false;
    static inline int status = 1;
    static inline std::filesystem::path filesDirectory;
    static inline std::thread output;
    static inline std::vector<std::string> flags;

    static void check(JNIEnv* env, const char* method);
    static void readEnvironment(ANativeActivity* activity);
    static void take(const std::string& name, const std::string& value);
    static void copyReferences(AAssetManager* assets);
    static void forwardOutput();
    static void runTests();
    static void destroy(ANativeActivity* activity);
};

} // namespace haylen::test
