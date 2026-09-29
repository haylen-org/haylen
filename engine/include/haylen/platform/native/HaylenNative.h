#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The version of HaylenNativeApi that this header describes. The engine hands libraries a table whose version is at least this one.
enum { HAYLEN_NATIVE_API_VERSION = 2 };

enum HaylenNativeLogLevel { HAYLEN_NATIVE_LOG_DEBUG = 0, HAYLEN_NATIVE_LOG_INFO = 1, HAYLEN_NATIVE_LOG_WARNING = 2, HAYLEN_NATIVE_LOG_ERROR = 3 };

// Answers the platform call named method, whose parameters arrive as JSON text. It runs on the frame thread, and the library answers once through resolve, at once or later from any thread.
typedef void (*HaylenNativeHandler)(void* user, uint64_t call, const char* method, const char* paramsJson);

// Tells a handler that the app cancelled the call or that its timeout passed, on the frame thread. An answer that still comes is dropped.
typedef void (*HaylenNativeCancel)(void* user, uint64_t call);

// The entry points of the engine for native libraries. Every entry may be called from any thread, and what it sends reaches the app on the frame thread.
typedef struct HaylenNativeApi {
    int version;
    // Sends an event with a JSON payload to haylen.platform.on listeners. A null payload sends null. An event that nothing listens to yet is dropped, unless retain is not 0: then it waits for the first listener of its name.
    void (*emit)(const char* event, const char* payloadJson, int retain);
    // Answers a call once. A success carries any JSON value, and a failure carries a message string or an object with message, code and data.
    void (*resolve)(uint64_t call, int ok, const char* resultJson);
    // Answers platform.call(method) for every app the process runs, replacing an earlier handler of the method. A null handler removes it, and cancel may be null.
    void (*registerHandler)(const char* method, HaylenNativeHandler handler, HaylenNativeCancel cancel, void* user);
    // Writes a line to the engine log at a HaylenNativeLogLevel.
    void (*log)(int level, const char* text);
    // Declares the library the native part of the plugin id, so the app sees the native part of that plugin as present on this platform from then on.
    void (*registerPlugin)(const char* id);
} HaylenNativeApi;

// The function that native.load(name, {init = 'symbol'}) calls after loading the library. It returns 0 on success, and any other value fails the load with that code.
typedef int (*HaylenNativeInit)(const HaylenNativeApi* api);

#ifdef __cplusplus
}
#endif
