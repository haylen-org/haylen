#include "gpu/android/GpuTestActivity.hpp"

#include <android/asset_manager.h>
#include <android/log.h>
#include <android/window.h>
#include <gtest/gtest.h>
#include <jni.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "gpu/support/GpuTestFiles.hpp"

namespace haylen::test {

void GpuTestActivity::create(ANativeActivity* activity) {
    shown = activity;
    activity->callbacks->onDestroy = &GpuTestActivity::destroy;

    // A change of configuration creates the activity again in the same process, while the tests already run.
    if (started) {
        return;
    }
    started = true;
    filesDirectory = activity->internalDataPath;
    ANativeActivity_setWindowFlags(activity, AWINDOW_FLAG_KEEP_SCREEN_ON, 0);

    readEnvironment(activity);
    const std::filesystem::path temporary = filesDirectory / "tmp";
    std::filesystem::create_directories(temporary);
    setenv("TMPDIR", temporary.c_str(), 1);
    copyReferences(activity->assetManager);

    forwardOutput();
    std::thread(&GpuTestActivity::runTests).detach();
}

std::filesystem::path GpuTestActivity::getFilesDirectory() {
    return filesDirectory;
}

void GpuTestActivity::check(JNIEnv* env, const char* method) {
    if (env->ExceptionCheck() == JNI_FALSE) {
        return;
    }
    env->ExceptionDescribe();
    env->ExceptionClear();
    throw std::runtime_error(std::string("The Java method \"") + method + "\" of the GPU test app threw an exception.");
}

// Each item of the extra is a variable as `NAME=value`, which the tests read once they start.
void GpuTestActivity::readEnvironment(ANativeActivity* activity) {
    JNIEnv* env = activity->env;
    const jmethodID getIntent = env->GetMethodID(env->GetObjectClass(activity->clazz), "getIntent", "()Landroid/content/Intent;");
    check(env, "getIntent");
    const jobject intent = env->CallObjectMethod(activity->clazz, getIntent);
    check(env, "getIntent");
    const jmethodID getExtra = env->GetMethodID(env->GetObjectClass(intent), "getStringArrayExtra", "(Ljava/lang/String;)[Ljava/lang/String;");
    check(env, "getStringArrayExtra");
    const auto values = static_cast<jobjectArray>(env->CallObjectMethod(intent, getExtra, env->NewStringUTF("environment")));
    check(env, "getStringArrayExtra");
    if (values == nullptr) {
        return;
    }

    for (jsize index = 0; index < env->GetArrayLength(values); ++index) {
        const auto value = static_cast<jstring>(env->GetObjectArrayElement(values, index));
        const char* characters = env->GetStringUTFChars(value, nullptr);
        const std::string_view variable(characters);
        if (const std::size_t equals = variable.find('='); equals != std::string_view::npos) {
            take(std::string(variable.substr(0, equals)), std::string(variable.substr(equals + 1)));
        }
        env->ReleaseStringUTFChars(value, characters);
        env->DeleteLocalRef(value);
    }
}

// GoogleTest reads its variables when the library loads, before the activity starts, so they reach it as the arguments they stand for.
void GpuTestActivity::take(const std::string& name, const std::string& value) {
    constexpr std::string_view kGoogleTest = "GTEST_";
    if (!name.starts_with(kGoogleTest)) {
        setenv(name.c_str(), value.c_str(), 1);
        return;
    }
    std::string flag = "--gtest_" + name.substr(kGoogleTest.size()) + "=" + value;
    std::transform(flag.begin(), flag.begin() + static_cast<std::ptrdiff_t>(flag.find('=')), flag.begin(), [](char character) { return static_cast<char>(std::tolower(static_cast<unsigned char>(character))); });
    flags.push_back(std::move(flag));
}

void GpuTestActivity::copyReferences(AAssetManager* assets) {
    const std::filesystem::path references = GpuTestFiles::getReferencesDirectory();
    std::filesystem::remove_all(references);
    std::filesystem::create_directories(references);

    const std::unique_ptr<AAssetDir, decltype(&AAssetDir_close)> folder(AAssetManager_openDir(assets, "gpu"), AAssetDir_close);
    std::array<char, 65536> buffer{};
    while (const char* name = AAssetDir_getNextFileName(folder.get())) {
        const std::unique_ptr<AAsset, decltype(&AAsset_close)> asset(AAssetManager_open(assets, (std::string("gpu/") + name).c_str(), AASSET_MODE_STREAMING), AAsset_close);
        std::ofstream file(references / name, std::ios::binary);
        for (int read = AAsset_read(asset.get(), buffer.data(), buffer.size()); read > 0; read = AAsset_read(asset.get(), buffer.data(), buffer.size())) {
            file.write(buffer.data(), read);
        }
    }
}

// The output of the tests goes through a pipe into the log line by line, since the standard output of an app reaches nothing.
void GpuTestActivity::forwardOutput() {
    std::array<int, 2> ends{};
    if (pipe(ends.data()) != 0) {
        throw std::runtime_error("The GPU test app could not make the pipe of its output.");
    }
    dup2(ends[1], STDOUT_FILENO);
    dup2(ends[1], STDERR_FILENO);
    close(ends[1]);
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);

    // clang-format off
    output = std::thread([reading = ends[0]] {
        FILE* stream = fdopen(reading, "r");
        std::array<char, 4096> line{};
        while (std::fgets(line.data(), static_cast<int>(line.size()), stream) != nullptr) {
            std::string text(line.data());
            if (!text.empty() && text.back() == '\n') {
                text.pop_back();
            }
            __android_log_write(ANDROID_LOG_INFO, kLogTag, text.c_str());
        }
        std::fclose(stream);
    });
    // clang-format on
}

void GpuTestActivity::runTests() {
    // The files of the app outlive its runs until it is uninstalled, so the results of an earlier run go first.
    const std::filesystem::path results = GpuTestFiles::getOutputDirectory();
    std::filesystem::remove_all(results);
    GTEST_FLAG_SET(output, "xml:" + (results / "results.xml").string());
    std::string name = "haylen-gpu-tests";
    std::vector<char*> arguments{name.data()};
    for (std::string& flag : flags) {
        arguments.push_back(flag.data());
    }
    int count = static_cast<int>(arguments.size());
    arguments.push_back(nullptr);
    testing::InitGoogleTest(&count, arguments.data());
    status = RUN_ALL_TESTS();

    // The log takes the last lines once both ends of the pipe close.
    std::fflush(stdout);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    output.join();

    // A process that quits while its activity shows makes the system start the activity again with the intent it had, so the activity finishes first and the process quits once it is gone.
    finished = true;
    ANativeActivity_finish(shown.load());
}

void GpuTestActivity::destroy(ANativeActivity*) {
    if (finished) {
        std::_Exit(status);
    }
}

} // namespace haylen::test
