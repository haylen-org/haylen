#pragma once

#include <jni.h>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>

#include "haylen/platform/AudioStream.hpp"
#include "haylen/platform/VideoStream.hpp"

namespace haylen::platform {

// The video and audio streams that Android plugins open through `HaylenVideoStream` and `HaylenAudioStream` of the Java side. A handle is the address of a stream, which the registry of plugin streams keeps for the life of the process. Arguments that a stream rejects throw an `IllegalArgumentException` in Java, and samples of the other format of a stream an `IllegalStateException`.
class AndroidStreams final {
  public:
    // Takes the format of `HaylenVideoStream.Format`, 0 for RGBA8 and 1 for BGRA8.
    [[nodiscard]] static jlong openVideo(JNIEnv& env, jbyteArray plugin, jbyteArray name, jint format, jint width, jint height);

    // Pushes `length` bytes of pixels from `offset` on, from the address of a direct buffer or from a byte array, without a copy in Java.
    static void pushVideoBuffer(JNIEnv& env, jlong stream, jobject pixels, jint offset, jint length, jint width, jint height, jint stride, jdouble timestamp);
    static void pushVideoArray(JNIEnv& env, jlong stream, jbyteArray pixels, jint offset, jint length, jint width, jint height, jint stride, jdouble timestamp);

    // Pushes the pixels of an `ARGB_8888` bitmap, which Android keeps as RGBA bytes, while they stay locked.
    static void pushBitmap(JNIEnv& env, jlong stream, jobject bitmap, jdouble timestamp);

    // Takes the format of `HaylenAudioStream.Format`, 0 for 32-bit floats and 1 for 16-bit integers.
    [[nodiscard]] static jlong openAudio(JNIEnv& env, jbyteArray plugin, jbyteArray name, jint sampleRate, jint channels, jint format, jint capacity);

    // Pushes the first `frames` interleaved frames of the array and returns how many fit into the ring.
    [[nodiscard]] static jint pushFloats(JNIEnv& env, jlong stream, jfloatArray samples, jint frames);
    [[nodiscard]] static jint pushShorts(JNIEnv& env, jlong stream, jshortArray samples, jint frames);

  private:
    // A C++ exception that becomes a Java exception once the JNI resources of the call are released.
    struct Failure {
        const char* javaClass = nullptr;
        std::string message;
    };

    template <typename Function> [[nodiscard]] static std::optional<Failure> attempt(Function&& function) {
        try {
            function();
            return std::nullopt;
        } catch (const std::invalid_argument& error) {
            return Failure{.javaClass = kIllegalArgument, .message = error.what()};
        } catch (const std::exception& error) {
            return Failure{.javaClass = kIllegalState, .message = error.what()};
        }
    }

    static void raise(JNIEnv& env, const std::optional<Failure>& failure);

    static void push(VideoStream& target, const std::byte* pixels, std::int64_t length, int width, int height, int stride, double timestamp);
    template <typename Sample> [[nodiscard]] static jint pushSamples(JNIEnv& env, jlong stream, jarray samples, jint frames);

    [[nodiscard]] static VideoStream& getVideo(jlong stream);
    [[nodiscard]] static AudioStream& getAudio(jlong stream);

    static constexpr const char* kIllegalArgument = "java/lang/IllegalArgumentException";
    static constexpr const char* kIllegalState = "java/lang/IllegalStateException";
};

} // namespace haylen::platform
