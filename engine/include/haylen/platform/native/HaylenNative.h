#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The version of `HaylenNativeApi` that this header describes. A new version may change any entry, so a library checks that the engine hands it the table of exactly this version.
enum { HAYLEN_NATIVE_API_VERSION = 5 };

enum HaylenNativeLogLevel { HAYLEN_NATIVE_LOG_DEBUG = 0, HAYLEN_NATIVE_LOG_INFO = 1, HAYLEN_NATIVE_LOG_WARNING = 2, HAYLEN_NATIVE_LOG_ERROR = 3 };

// How `emit` delivers an event, as flags that combine. A retained event that nothing listens to yet waits for the first listener of its name. The batched events of a name that arrive in one frame reach the app once, as one list in order.
enum HaylenNativeEmitFlags { HAYLEN_NATIVE_EMIT_RETAIN = 1, HAYLEN_NATIVE_EMIT_BATCHED = 2 };

// The pixel formats of video streams, four bytes per pixel.
enum HaylenNativePixelFormat { HAYLEN_NATIVE_PIXELS_RGBA8 = 0, HAYLEN_NATIVE_PIXELS_BGRA8 = 1 };

// The sample formats of audio streams, interleaved by channel.
enum HaylenNativeSampleFormat { HAYLEN_NATIVE_SAMPLES_FLOAT32 = 0, HAYLEN_NATIVE_SAMPLES_INT16 = 1 };

// A byte buffer that JSON refers to as `{"$bytes": N}`, where `N` is its place in the array of buffers that travels with the JSON.
typedef struct HaylenNativeBuffer {
    const void* data;
    size_t size;
} HaylenNativeBuffer;

// The streams of the app, which live as long as the process, so a library keeps their handles for good.
typedef struct HaylenNativeVideoStream HaylenNativeVideoStream;
typedef struct HaylenNativeAudioStream HaylenNativeAudioStream;

// The window of the app on the desktops, for libraries that open windows of their own over it. On macOS `handle` is the `NSWindow*` of the app, which Objective-C reads back with a `__bridge` cast, and `display` is null. On Windows `handle` is the `HWND` of the app and `display` is null. On Linux `handle` is the X11 `Window` of the app, an id that `(Window)(uintptr_t)handle` reads back, and `display` is the `Display*` of the connection of the engine, which belongs to the frame thread: a library that runs a window of its own opens its own connection, where the id names the same window, since X11 ids belong to the server.
typedef struct HaylenNativeWindow {
    void* handle;
    void* display;
} HaylenNativeWindow;

// Answers the platform call named `method`, whose parameters arrive as JSON text with the buffers it refers to, which stay valid until the handler returns. It runs on the frame thread, and the library answers once through `resolve`, at once or later from any thread.
typedef void (*HaylenNativeHandler)(void* user, uint64_t call, const char* method, const char* paramsJson, const HaylenNativeBuffer* buffers, size_t bufferCount);

// Tells a handler that the app cancelled the call or that its timeout passed, on the frame thread. An answer that still comes is dropped.
typedef void (*HaylenNativeCancel)(void* user, uint64_t call);

// Receives the report of every error that stops the app, the one its error screen shows, as JSON text of `{message, file, line, traceback, frames}`. It runs on the frame thread.
typedef void (*HaylenNativeErrorHandler)(void* user, const char* reportJson);

// Opens the screen of a plugin that the app asked for, after the engine covered the app. It runs on the frame thread with the parameters as JSON text and the buffers it refers to, valid until it returns, and the library ends the screen once through `finishScreen`, at once or later from any thread.
typedef void (*HaylenNativeScreenOpener)(void* user, uint64_t screen, const char* paramsJson, const HaylenNativeBuffer* buffers, size_t bufferCount);

// Tells a library that the app gave its screen up, with a cancel or a timeout, on the frame thread. The library closes the screen and still ends it through `finishScreen`, which uncovers the app.
typedef void (*HaylenNativeScreenCancel)(void* user, uint64_t screen);

// The entry points of the engine for native libraries. Every entry may be called from any thread, and what it sends reaches the app on the frame thread. Entries that take buffers copy them before they return, and buffers may be null when `bufferCount` is 0.
typedef struct HaylenNativeApi {
    int version;
    // Sends an event with a JSON payload, and the buffers it refers to, to `haylen.platform.on` listeners, delivered by the `HaylenNativeEmitFlags` in `flags`. A null payload sends `null`. An event that nothing listens to is dropped unless it is retained.
    void (*emit)(const char* event, const char* payloadJson, const HaylenNativeBuffer* buffers, size_t bufferCount, int flags);
    // Answers a call once. A success carries any JSON value with the buffers it refers to, and a failure carries a message string or an object with `message`, `code` and `data`.
    void (*resolve)(uint64_t call, int ok, const char* resultJson, const HaylenNativeBuffer* buffers, size_t bufferCount);
    // Answers `platform.call(method)` for every app the process runs, replacing an earlier handler of the method. A null handler removes it, and `cancel` may be null.
    void (*registerHandler)(const char* method, HaylenNativeHandler handler, HaylenNativeCancel cancel, void* user);
    // Writes a line to the engine log at a `HaylenNativeLogLevel`.
    void (*log)(int level, const char* text);
    // Declares the library the native part of the plugin `id`, so the app sees the native part of that plugin as present on this platform from then on.
    void (*registerPlugin)(const char* id);
    // Hands `handler` the report of every error that stops an app of the process from then on. Registering the same handler with the same `user` again changes nothing.
    void (*registerErrorHandler)(HaylenNativeErrorHandler handler, void* user);
    // Returns the video stream `name` of the plugin `id`, which the app draws through `platform.plugin(id):videoStream(name)`, opening it the first time with a `HaylenNativePixelFormat` and a size, 0 by 0 until the first frame. Returns null and logs why for an empty id or name, an unknown format, a negative size or a stream that is open with another format.
    HaylenNativeVideoStream* (*openVideoStream)(const char* plugin, const char* name, int format, int width, int height);
    // Copies a frame of `width` by `height` pixels whose rows start `stride` bytes apart. The stream keeps only the newest frame, which the app shows from its next frame, and a frame of another size resizes the stream.
    void (*pushVideoFrame)(HaylenNativeVideoStream* stream, const void* pixels, int width, int height, int stride, double timestamp);
    // Returns the audio stream `name` of the plugin `id`, which the app plays through `platform.plugin(id):audioStream(name)`, opening it the first time with a sample rate, channels, a `HaylenNativeSampleFormat` and room for `capacityFrames` frames. Returns null and logs why for an empty id or name, a format, rate, channel count or capacity it cannot take, or a stream that is open with another format.
    HaylenNativeAudioStream* (*openAudioStream)(const char* plugin, const char* name, int sampleRate, int channels, int format, int capacityFrames);
    // Writes interleaved frames in the format of the stream and returns how many fit, dropping the rest while the stream is full. One thread at a time pushes into a stream.
    size_t (*pushAudioFrames)(HaylenNativeAudioStream* stream, const void* samples, size_t frames);
    // Opens the screen `name` of the plugin `id` with `open` whenever an app of the process asks for it, replacing an earlier registration of the screen. A null `open` removes it, and `cancel` may be null.
    void (*registerScreen)(const char* plugin, const char* name, HaylenNativeScreenOpener open, HaylenNativeScreenCancel cancel, void* user);
    // Ends a screen once it closed, with a result like `resolve`, or with a failure such as `{"message": "...", "code": "cancelled"}` when the user closed it. The engine uncovers the app and answers the call of the screen, or hands the end to the next app as the retained event `<plugin>.screenRestored` when the app restarted meanwhile.
    void (*finishScreen)(uint64_t screen, int ok, const char* resultJson, const HaylenNativeBuffer* buffers, size_t bufferCount);
    // Fills `window` with the window of the app and returns 1, or returns 0 before the window opens and where apps have no desktop window.
    int (*getWindow)(HaylenNativeWindow* window);
    // Covers the app while native UI of the library covers it, which makes the app inactive, halted and muted until the matching `uncoverApp`. Covers nest and belong to the process, so they outlive the apps that restart under them.
    void (*coverApp)(void);
    void (*uncoverApp)(void);
} HaylenNativeApi;

// The function that `native.load(name, {init = 'symbol'})` calls after loading the library. It returns 0 on success, and any other value fails the load with that code.
typedef int (*HaylenNativeInit)(const HaylenNativeApi* api);

#ifdef __cplusplus
}
#endif
