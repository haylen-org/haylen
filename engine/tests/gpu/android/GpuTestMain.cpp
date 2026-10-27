#include <android/native_activity.h>
#include <jni.h>

#include <cstddef>

#include "gpu/android/GpuTestActivity.hpp"

// Entry point of the GPU test app of Android, which NativeActivity calls when it creates the activity.
extern "C" JNIEXPORT void ANativeActivity_onCreate(ANativeActivity* activity, void*, std::size_t) {
    haylen::test::GpuTestActivity::create(activity);
}
