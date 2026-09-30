#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The version of HaylenNativeApi that this header describes. A new version may change any entry, so a library checks that the engine hands it the table of exactly this version.
enum { HAYLEN_NATIVE_API_VERSION = 4 };

enum HaylenNativeLogLevel { HAYLEN_NATIVE_LOG_DEBUG = 0, HAYLEN_NATIVE_LOG_INFO = 1, HAYLEN_NATIVE_LOG_WARNING = 2, HAYLEN_NATIVE_LOG_ERROR = 3 };

// How emit delivers an event, as flags that combine. A retained event that nothing listens to yet waits for the first listener of its name. The batched events of a name that arrive in one frame reach the app once, as one list in order.
enum HaylenNativeEmitFlags { HAYLEN_NATIVE_EMIT_RETAIN = 1, HAYLEN_NATIVE_EMIT_BATCHED = 2 };

// The pixel formats of video streams, four bytes per pixel.
enum HaylenNativePixelFormat { HAYLEN_NATIVE_PIXELS_RGBA8 = 0, HAYLEN_NATIVE_PIXELS_BGRA8 = 1 };

// The sample formats of audio streams, interleaved by channel.
enum HaylenNativeSampleFormat { HAYLEN_NATIVE_SAMPLES_FLOAT32 = 0, HAYLEN_NATIVE_SAMPLES_INT16 = 1 };

// A byte buffer that JSON refers to as {"$bytes": N}, where N is its place in the array of buffers that travels with the JSON.
typedef struct HaylenNativeBuffer {
    const void* data;
    size_t size;
} HaylenNativeBuffer;

// The streams of the app, which live as long as the process, so a library keeps their handles for good.
typedef struct HaylenNativeVideoStream HaylenNativeVideoStream;
typedef struct HaylenNativeAudioStream HaylenNativeAudioStream;

// Answers the platform call named method, whose parameters arrive as JSON text with the buffers it refers to, which stay valid until the handler returns. It runs on the frame thread, and the library answers once through resolve, at once or later from any thread.
typedef void (*HaylenNativeHandler)(void* user, uint64_t call, const char* method, const char* paramsJson, const HaylenNativeBuffer* buffers, size_t bufferCount);

// Tells a handler that the app cancelled the call or that its timeout passed, on the frame thread. An answer that still comes is dropped.
typedef void (*HaylenNativeCancel)(void* user, uint64_t call);

// Receives the report of every error that stops the app, the one its error screen shows, as JSON text of {message, file, line, traceback, frames}. It runs on the frame thread.
typedef void (*HaylenNativeErrorHandler)(void* user, const char* reportJson);

// The entry points of the engine for native libraries. Every entry may be called from any thread, and what it sends reaches the app on the frame thread. Entries that take buffers copy them before they return, and buffers may be null when bufferCount is 0.
typedef struct HaylenNativeApi {
    int version;
    // Sends an event with a JSON payload, and the buffers it refers to, to haylen.platform.on listeners, delivered by the HaylenNativeEmitFlags in flags. A null payload sends null. An event that nothing listens to is dropped unless it is retained.
    void (*emit)(const char* event, const char* payloadJson, const HaylenNativeBuffer* buffers, size_t bufferCount, int flags);
    // Answers a call once. A success carries any JSON value with the buffers it refers to, and a failure carries a message string or an object with message, code and data.
    void (*resolve)(uint64_t call, int ok, const char* resultJson, const HaylenNativeBuffer* buffers, size_t bufferCount);
    // Answers platform.call(method) for every app the process runs, replacing an earlier handler of the method. A null handler removes it, and cancel may be null.
    void (*registerHandler)(const char* method, HaylenNativeHandler handler, HaylenNativeCancel cancel, void* user);
    // Writes a line to the engine log at a HaylenNativeLogLevel.
    void (*log)(int level, const char* text);
    // Declares the library the native part of the plugin id, so the app sees the native part of that plugin as present on this platform from then on.
    void (*registerPlugin)(const char* id);
    // Hands handler the report of every error that stops an app of the process from then on. Registering the same handler with the same user again changes nothing.
    void (*registerErrorHandler)(HaylenNativeErrorHandler handler, void* user);
    // Returns the video stream name of the plugin id, which the app draws through platform.plugin(id):videoStream(name), opening it the first time with a HaylenNativePixelFormat and a size, 0 by 0 until the first frame. Returns null and logs why for an empty id or name, an unknown format, a negative size or a stream that is open with another format.
    HaylenNativeVideoStream* (*openVideoStream)(const char* plugin, const char* name, int format, int width, int height);
    // Copies a frame of width by height pixels whose rows start stride bytes apart. The stream keeps only the newest frame, which the app shows from its next frame, and a frame of another size resizes the stream.
    void (*pushVideoFrame)(HaylenNativeVideoStream* stream, const void* pixels, int width, int height, int stride, double timestamp);
    // Returns the audio stream name of the plugin id, which the app plays through platform.plugin(id):audioStream(name), opening it the first time with a sample rate, channels, a HaylenNativeSampleFormat and room for capacityFrames frames. Returns null and logs why for an empty id or name, a format, rate, channel count or capacity it cannot take, or a stream that is open with another format.
    HaylenNativeAudioStream* (*openAudioStream)(const char* plugin, const char* name, int sampleRate, int channels, int format, int capacityFrames);
    // Writes interleaved frames in the format of the stream and returns how many fit, dropping the rest while the stream is full. One thread at a time pushes into a stream.
    size_t (*pushAudioFrames)(HaylenNativeAudioStream* stream, const void* samples, size_t frames);
} HaylenNativeApi;

// The function that native.load(name, {init = 'symbol'}) calls after loading the library. It returns 0 on success, and any other value fails the load with that code.
typedef int (*HaylenNativeInit)(const HaylenNativeApi* api);

#ifdef __cplusplus
}
#endif
