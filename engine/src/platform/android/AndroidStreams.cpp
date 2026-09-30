#include "platform/android/AndroidStreams.hpp"

#include <android/bitmap.h>

#include <span>

#include "haylen/platform/PluginStreams.hpp"
#include "platform/android/JavaBridge.hpp"

namespace haylen::platform {

jlong AndroidStreams::openVideo(JNIEnv& env, jbyteArray plugin, jbyteArray name, jint format, jint width, jint height) {
    const std::string id = JavaBridge::toString(env, plugin);
    const std::string key = JavaBridge::toString(env, name);
    VideoStream* stream = nullptr;
    raise(env, attempt([&] { stream = PluginStreams::openVideo(id, key, format == 0 ? VideoStream::Format::Rgba8 : VideoStream::Format::Bgra8, width, height).get(); }));
    return reinterpret_cast<jlong>(stream);
}

void AndroidStreams::pushVideoBuffer(JNIEnv& env, jlong stream, jobject pixels, jint offset, jint length, jint width, jint height, jint stride, jdouble timestamp) {
    const auto* address = static_cast<const std::byte*>(env.GetDirectBufferAddress(pixels));
    raise(env, attempt([&] { push(getVideo(stream), address + offset, length, width, height, stride, timestamp); }));
}

// The critical region keeps the garbage collector from moving the array while the stream copies the frame out of it.
void AndroidStreams::pushVideoArray(JNIEnv& env, jlong stream, jbyteArray pixels, jint offset, jint length, jint width, jint height, jint stride, jdouble timestamp) {
    void* bytes = env.GetPrimitiveArrayCritical(pixels, nullptr);
    const std::optional<Failure> failure = attempt([&] { push(getVideo(stream), static_cast<const std::byte*>(bytes) + offset, length, width, height, stride, timestamp); });
    env.ReleasePrimitiveArrayCritical(pixels, bytes, JNI_ABORT);
    raise(env, failure);
}

void AndroidStreams::pushBitmap(JNIEnv& env, jlong stream, jobject bitmap, jdouble timestamp) {
    VideoStream& target = getVideo(stream);
    AndroidBitmapInfo info{};
    if (AndroidBitmap_getInfo(&env, bitmap, &info) != ANDROID_BITMAP_RESULT_SUCCESS || info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        raise(env, Failure{.javaClass = kIllegalArgument, .message = "A video frame takes a bitmap of the config \"ARGB_8888\"."});
        return;
    }
    if (target.getFormat() != VideoStream::Format::Rgba8) {
        raise(env, Failure{.javaClass = kIllegalState, .message = "A bitmap holds RGBA pixels, which a stream of the format \"BGRA8\" does not take. Open the stream with the format \"RGBA8\"."});
        return;
    }

    void* pixels = nullptr;
    if (AndroidBitmap_lockPixels(&env, bitmap, &pixels) != ANDROID_BITMAP_RESULT_SUCCESS) {
        raise(env, Failure{.javaClass = kIllegalState, .message = "The pixels of the bitmap could not be locked, as happens to a recycled bitmap."});
        return;
    }
    const std::optional<Failure> failure = attempt([&] { target.push(static_cast<const std::byte*>(pixels), static_cast<int>(info.width), static_cast<int>(info.height), info.stride, timestamp); });
    AndroidBitmap_unlockPixels(&env, bitmap);
    raise(env, failure);
}

jlong AndroidStreams::openAudio(JNIEnv& env, jbyteArray plugin, jbyteArray name, jint sampleRate, jint channels, jint format, jint capacity) {
    const std::string id = JavaBridge::toString(env, plugin);
    const std::string key = JavaBridge::toString(env, name);
    AudioStream* stream = nullptr;
    // clang-format off
    raise(env, attempt([&] {
        if (sampleRate <= 0 || channels <= 0 || capacity <= 0) {
            throw std::invalid_argument("An audio stream needs a sample rate, channels and room for at least one frame.");
        }
        stream = PluginStreams::openAudio(id, key, static_cast<std::uint32_t>(sampleRate), static_cast<std::uint32_t>(channels), format == 0 ? AudioStream::Format::Float32 : AudioStream::Format::Int16, static_cast<std::size_t>(capacity)).get();
    }));
    // clang-format on
    return reinterpret_cast<jlong>(stream);
}

jint AndroidStreams::pushFloats(JNIEnv& env, jlong stream, jfloatArray samples, jint frames) {
    return pushSamples<float>(env, stream, samples, frames);
}

jint AndroidStreams::pushShorts(JNIEnv& env, jlong stream, jshortArray samples, jint frames) {
    return pushSamples<std::int16_t>(env, stream, samples, frames);
}

void AndroidStreams::raise(JNIEnv& env, const std::optional<Failure>& failure) {
    if (failure) {
        env.ThrowNew(env.FindClass(failure->javaClass), failure->message.c_str());
    }
}

// The stream checks the size and the stride itself, so the length only has to hold the rows they span.
void AndroidStreams::push(VideoStream& target, const std::byte* pixels, std::int64_t length, int width, int height, int stride, double timestamp) {
    const std::int64_t needed = height > 0 ? std::int64_t{stride} * (height - 1) + std::int64_t{width} * 4 : 0;
    if (stride < 0 || length < needed) {
        throw std::invalid_argument("The pixels of a video frame hold fewer bytes than its size and its stride need.");
    }
    target.push(pixels, width, height, static_cast<std::size_t>(stride), timestamp);
}

template <typename Sample> jint AndroidStreams::pushSamples(JNIEnv& env, jlong stream, jarray samples, jint frames) {
    AudioStream& target = getAudio(stream);
    if (frames < 0 || static_cast<std::size_t>(frames) * target.getChannels() > static_cast<std::size_t>(env.GetArrayLength(samples))) {
        raise(env, Failure{.javaClass = kIllegalArgument, .message = "The samples hold fewer frames than the push asks for."});
        return 0;
    }

    const std::size_t count = static_cast<std::size_t>(frames) * target.getChannels();
    std::size_t written = 0;
    void* data = env.GetPrimitiveArrayCritical(samples, nullptr);
    const std::optional<Failure> failure = attempt([&] { written = target.push(std::span(static_cast<const Sample*>(data), count)); });
    env.ReleasePrimitiveArrayCritical(samples, data, JNI_ABORT);
    raise(env, failure);
    return static_cast<jint>(written);
}

VideoStream& AndroidStreams::getVideo(jlong stream) {
    return *reinterpret_cast<VideoStream*>(stream);
}

AudioStream& AndroidStreams::getAudio(jlong stream) {
    return *reinterpret_cast<AudioStream*>(stream);
}

} // namespace haylen::platform

extern "C" {

JNIEXPORT jlong JNICALL Java_dev_haylen_HaylenVideoStream_nativeOpen(JNIEnv* env, jclass, jbyteArray plugin, jbyteArray name, jint format, jint width, jint height) {
    return haylen::platform::AndroidStreams::openVideo(*env, plugin, name, format, width, height);
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenVideoStream_nativePushBuffer(JNIEnv* env, jclass, jlong stream, jobject pixels, jint offset, jint length, jint width, jint height, jint stride, jdouble timestamp) {
    haylen::platform::AndroidStreams::pushVideoBuffer(*env, stream, pixels, offset, length, width, height, stride, timestamp);
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenVideoStream_nativePushArray(JNIEnv* env, jclass, jlong stream, jbyteArray pixels, jint offset, jint length, jint width, jint height, jint stride, jdouble timestamp) {
    haylen::platform::AndroidStreams::pushVideoArray(*env, stream, pixels, offset, length, width, height, stride, timestamp);
}

JNIEXPORT void JNICALL Java_dev_haylen_HaylenVideoStream_nativePushBitmap(JNIEnv* env, jclass, jlong stream, jobject bitmap, jdouble timestamp) {
    haylen::platform::AndroidStreams::pushBitmap(*env, stream, bitmap, timestamp);
}

JNIEXPORT jlong JNICALL Java_dev_haylen_HaylenAudioStream_nativeOpen(JNIEnv* env, jclass, jbyteArray plugin, jbyteArray name, jint sampleRate, jint channels, jint format, jint capacity) {
    return haylen::platform::AndroidStreams::openAudio(*env, plugin, name, sampleRate, channels, format, capacity);
}

JNIEXPORT jint JNICALL Java_dev_haylen_HaylenAudioStream_nativePushFloats(JNIEnv* env, jclass, jlong stream, jfloatArray samples, jint frames) {
    return haylen::platform::AndroidStreams::pushFloats(*env, stream, samples, frames);
}

JNIEXPORT jint JNICALL Java_dev_haylen_HaylenAudioStream_nativePushShorts(JNIEnv* env, jclass, jlong stream, jshortArray samples, jint frames) {
    return haylen::platform::AndroidStreams::pushShorts(*env, stream, samples, frames);
}
}
