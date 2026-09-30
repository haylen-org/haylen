// A plain C library that the native interop tests and the native sample load on every platform. It exercises values, structs, buffers, text, callbacks from the calling thread and from threads of its own, and the `HaylenNativeApi` of the engine with byte buffers, batched events, streams, a screen, the window of the app and covers.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "haylen/platform/native/HaylenNative.h"

#if defined(_WIN32)
#include <windows.h>
#define NATIVE_TEST_EXPORT __declspec(dllexport)
#else
#include <pthread.h>
#define NATIVE_TEST_EXPORT __attribute__((visibility("default")))
#endif

// The sample builds the library twice, as a dynamic library and as a static library linked into iOS apps, and each build names itself.
#if !defined(NATIVE_TEST_ORIGIN)
#define NATIVE_TEST_ORIGIN "dynamic"
#endif

typedef struct NativeTestPoint {
    int32_t x;
    int32_t y;
} NativeTestPoint;

typedef struct NativeTestRect {
    NativeTestPoint origin;
    float width;
    float height;
} NativeTestRect;

typedef void (*NativeTestVisitor)(int32_t index, const char* label);
typedef void (*NativeTestReporter)(int32_t value, const uint8_t* data, size_t size);

// A job on a thread of the library carries the interface of the engine it answers through, because a later init of the library replaces the one the library keeps.
typedef struct NativeTestJob {
    void (*body)(struct NativeTestJob* job);
    const HaylenNativeApi* api;
    NativeTestReporter reporter;
    int32_t value;
    int32_t width;
    int32_t height;
    uint64_t call;
    char* text;
    uint8_t* bytes;
    size_t size;
} NativeTestJob;

static const HaylenNativeApi* nativeTestApi = NULL;
static int32_t nativeTestErrors = 0;
static char* nativeTestLastError = NULL;
static uint64_t nativeTestScreen = 0;

#if defined(_WIN32)
static DWORD WINAPI native_test_thread(LPVOID context) {
    NativeTestJob* job = (NativeTestJob*)context;
    job->body(job);
    free(job->text);
    free(job->bytes);
    free(job);
    return 0;
}

static void native_test_spawn(NativeTestJob* job) {
    HANDLE thread = CreateThread(NULL, 0, native_test_thread, job, 0, NULL);
    CloseHandle(thread);
}
#else
static void* native_test_thread(void* context) {
    NativeTestJob* job = (NativeTestJob*)context;
    job->body(job);
    free(job->text);
    free(job->bytes);
    free(job);
    return NULL;
}

static void native_test_spawn(NativeTestJob* job) {
    pthread_t thread;
    pthread_create(&thread, NULL, native_test_thread, job);
    pthread_detach(thread);
}
#endif

static NativeTestJob* native_test_job(void (*body)(NativeTestJob* job)) {
    NativeTestJob* job = (NativeTestJob*)calloc(1, sizeof(NativeTestJob));
    job->body = body;
    return job;
}

NATIVE_TEST_EXPORT int32_t native_test_add(int32_t a, int32_t b) {
    return a + b;
}

NATIVE_TEST_EXPORT double native_test_scale(double value, double factor) {
    return value * factor;
}

NATIVE_TEST_EXPORT const char* native_test_origin(void) {
    return NATIVE_TEST_ORIGIN;
}

NATIVE_TEST_EXPORT NativeTestPoint native_test_point_add(NativeTestPoint a, NativeTestPoint b) {
    NativeTestPoint sum = {a.x + b.x, a.y + b.y};
    return sum;
}

NATIVE_TEST_EXPORT void native_test_rect_grow(NativeTestRect* rect, float amount) {
    rect->origin.x -= (int32_t)amount;
    rect->origin.y -= (int32_t)amount;
    rect->width += amount * 2.0F;
    rect->height += amount * 2.0F;
}

NATIVE_TEST_EXPORT void native_test_fill(uint8_t* buffer, size_t size, uint8_t seed) {
    for (size_t index = 0; index < size; ++index) {
        buffer[index] = (uint8_t)(seed + index);
    }
}

// FNV-1a, so the checksum depends on the order of the bytes.
NATIVE_TEST_EXPORT uint32_t native_test_checksum(const uint8_t* buffer, size_t size) {
    uint32_t hash = 2166136261U;
    for (size_t index = 0; index < size; ++index) {
        hash = (hash ^ buffer[index]) * 16777619U;
    }
    return hash;
}

NATIVE_TEST_EXPORT int32_t native_test_visit(int32_t count, NativeTestVisitor visitor) {
    static const char* const labels[] = {"zero", "one", "two", "three"};
    for (int32_t index = 0; index < count; ++index) {
        visitor(index, labels[index % 4]);
    }
    return count;
}

static void native_test_report(NativeTestJob* job) {
    uint8_t data[4] = {1, 2, 3, 4};
    job->reporter(job->value, data, sizeof(data));
}

// Calls the reporter from a thread of the library with the value and four bytes, after this function returned.
NATIVE_TEST_EXPORT void native_test_report_later(NativeTestReporter reporter, int32_t value) {
    NativeTestJob* job = native_test_job(native_test_report);
    job->reporter = reporter;
    job->value = value;
    native_test_spawn(job);
}

static void native_test_answer_echo(NativeTestJob* job) {
    const size_t size = strlen(job->text) + 32;
    char* answer = (char*)malloc(size);
    snprintf(answer, size, "{\"echo\":%s,\"thread\":true}", job->text);
    job->api->resolve(job->call, 1, answer, NULL, 0);
    free(answer);
}

// Answers with the bytes it received and the same bytes reversed, which it refers to in the opposite order of the buffers.
static void native_test_answer_bytes(NativeTestJob* job) {
    uint8_t* reversed = (uint8_t*)malloc(job->size + 1);
    for (size_t index = 0; index < job->size; ++index) {
        reversed[index] = job->bytes[job->size - 1 - index];
    }
    const HaylenNativeBuffer buffers[2] = {{reversed, job->size}, {job->bytes, job->size}};
    char answer[64];
    snprintf(answer, sizeof(answer), "{\"same\":{\"$bytes\":1},\"reversed\":{\"$bytes\":0},\"size\":%zu}", job->size);
    job->api->resolve(job->call, 1, answer, buffers, 2);
    free(reversed);
}

// Sends `count` batched events at once, each with one byte that counts them, which reach the app in one list.
static void native_test_send_burst(int32_t count) {
    for (int32_t index = 0; index < count; ++index) {
        const uint8_t value = (uint8_t)index;
        const HaylenNativeBuffer buffer = {&value, 1};
        char payload[64];
        snprintf(payload, sizeof(payload), "{\"index\":%d,\"byte\":{\"$bytes\":0}}", index);
        nativeTestApi->emit("native_test.burst", payload, &buffer, 1, HAYLEN_NATIVE_EMIT_BATCHED);
    }
}

static void native_test_announce(NativeTestJob* job) {
    char payload[64];
    snprintf(payload, sizeof(payload), "{\"version\":%d,\"origin\":\"%s\"}", job->value, NATIVE_TEST_ORIGIN);
    job->api->emit("native_test.ready", payload, NULL, 0, HAYLEN_NATIVE_EMIT_RETAIN);
}

// Copies the first buffer of a call for a thread of the library.
static void native_test_keep_bytes(NativeTestJob* job, const HaylenNativeBuffer* buffers, size_t bufferCount) {
    job->size = bufferCount > 0 ? buffers[0].size : 0;
    job->bytes = (uint8_t*)malloc(job->size + 1);
    if (job->size > 0) {
        memcpy(job->bytes, buffers[0].data, job->size);
    }
}

// Answers `native_test.echo` and `native_test.echoBytes` from a thread of the library, sends the batched events of `native_test.burst`, fails `native_test.fail` with a code and data, and leaves `native_test.wait` pending until the app gives it up.
static void native_test_handle(void* user, uint64_t call, const char* method, const char* paramsJson, const HaylenNativeBuffer* buffers, size_t bufferCount) {
    (void)user;
    if (strcmp(method, "native_test.echo") == 0) {
        NativeTestJob* job = native_test_job(native_test_answer_echo);
        job->api = nativeTestApi;
        job->call = call;
        const size_t length = strlen(paramsJson) + 1;
        job->text = (char*)malloc(length);
        memcpy(job->text, paramsJson, length);
        native_test_spawn(job);
    } else if (strcmp(method, "native_test.echoBytes") == 0) {
        NativeTestJob* job = native_test_job(native_test_answer_bytes);
        job->api = nativeTestApi;
        job->call = call;
        native_test_keep_bytes(job, buffers, bufferCount);
        native_test_spawn(job);
    } else if (strcmp(method, "native_test.burst") == 0) {
        const char* count = strchr(paramsJson, ':');
        native_test_send_burst(count != NULL ? (int32_t)strtol(count + 1, NULL, 10) : 0);
        nativeTestApi->resolve(call, 1, "null", NULL, 0);
    } else if (strcmp(method, "native_test.fail") == 0) {
        nativeTestApi->resolve(call, 0, "{\"message\":\"The native test failed on purpose.\",\"code\":\"native_test_failure\",\"data\":{\"reason\":\"requested\"}}", NULL, 0);
    }
}

static void native_test_cancel(void* user, uint64_t call) {
    (void)user;
    char payload[64];
    snprintf(payload, sizeof(payload), "{\"call\":%llu}", (unsigned long long)call);
    nativeTestApi->emit("native_test.cancelled", payload, NULL, 0, 0);
}

// Pushes a frame of `width` by `height` pixels into the video stream `pattern` of `native-test` from a thread of the library. The stream is BGRA, every pixel holds blue, green and red from the seed and an opaque alpha, and every row carries four bytes of padding.
static void native_test_push_frame(NativeTestJob* job) {
    HaylenNativeVideoStream* stream = job->api->openVideoStream("native-test", "pattern", HAYLEN_NATIVE_PIXELS_BGRA8, 0, 0);
    const size_t stride = (size_t)job->width * 4 + 4;
    uint8_t* pixels = (uint8_t*)calloc(stride * (size_t)job->height, 1);
    for (int32_t row = 0; row < job->height; ++row) {
        for (int32_t column = 0; column < job->width; ++column) {
            uint8_t* pixel = pixels + (size_t)row * stride + (size_t)column * 4;
            pixel[0] = (uint8_t)job->value;
            pixel[1] = (uint8_t)(job->value + 1);
            pixel[2] = (uint8_t)(job->value + 2);
            pixel[3] = 255;
        }
    }
    job->api->pushVideoFrame(stream, pixels, job->width, job->height, (int)stride, job->value / 10.0);
    free(pixels);
}

NATIVE_TEST_EXPORT void native_test_push_frame_later(int32_t width, int32_t height, int32_t seed) {
    NativeTestJob* job = native_test_job(native_test_push_frame);
    job->api = nativeTestApi;
    job->width = width;
    job->height = height;
    job->value = seed;
    native_test_spawn(job);
}

// Pushes `count` 16-bit samples of the value into the audio stream `tone` of `native-test`, mono at 8000 Hz, from a thread of the library.
static void native_test_push_tone(NativeTestJob* job) {
    HaylenNativeAudioStream* stream = job->api->openAudioStream("native-test", "tone", 8000, 1, HAYLEN_NATIVE_SAMPLES_INT16, 4000);
    int16_t* samples = (int16_t*)malloc(sizeof(int16_t) * (size_t)job->width);
    for (int32_t index = 0; index < job->width; ++index) {
        samples[index] = (int16_t)job->value;
    }
    job->api->pushAudioFrames(stream, samples, (size_t)job->width);
    free(samples);
}

NATIVE_TEST_EXPORT void native_test_push_tone_later(int32_t count, int32_t value) {
    NativeTestJob* job = native_test_job(native_test_push_tone);
    job->api = nativeTestApi;
    job->width = count;
    job->value = value;
    native_test_spawn(job);
}

// Returns 1 when the engine refuses to open the streams of `native-test` again with another format.
NATIVE_TEST_EXPORT int32_t native_test_reopen_refused(void) {
    const int video = nativeTestApi->openVideoStream("native-test", "pattern", HAYLEN_NATIVE_PIXELS_RGBA8, 0, 0) == NULL;
    const int audio = nativeTestApi->openAudioStream("native-test", "tone", 8000, 2, HAYLEN_NATIVE_SAMPLES_INT16, 4000) == NULL;
    return video && audio ? 1 : 0;
}

// Keeps the report of the last error that stopped an app and counts the reports, which the tests read back.
static void native_test_app_failed(void* user, const char* reportJson) {
    (void)user;
    const size_t length = strlen(reportJson) + 1;
    free(nativeTestLastError);
    nativeTestLastError = (char*)malloc(length);
    memcpy(nativeTestLastError, reportJson, length);
    ++nativeTestErrors;
}

NATIVE_TEST_EXPORT int32_t native_test_error_count(void) {
    return nativeTestErrors;
}

NATIVE_TEST_EXPORT const char* native_test_last_error(void) {
    return nativeTestLastError != NULL ? nativeTestLastError : "";
}

// Opens the screen `panel` of `native-test`, which sends `screenOpened` with its parameters and their bytes, and stays open until the test ends it or the app gives it up.
static void native_test_open_screen(void* user, uint64_t screen, const char* paramsJson, const HaylenNativeBuffer* buffers, size_t bufferCount) {
    (void)user;
    nativeTestScreen = screen;
    const size_t size = strlen(paramsJson) + 64;
    char* payload = (char*)malloc(size);
    snprintf(payload, size, "{\"params\":%s,\"buffers\":%zu}", paramsJson, bufferCount);
    nativeTestApi->emit("native_test.screenOpened", payload, buffers, bufferCount, 0);
    free(payload);
}

// Closes the panel that the app gave up, which ends it as `cancelled`.
static void native_test_cancel_screen(void* user, uint64_t screen) {
    (void)user;
    nativeTestApi->finishScreen(screen, 0, "{\"message\":\"The panel closed.\",\"code\":\"cancelled\"}", NULL, 0);
}

static void native_test_close_screen(NativeTestJob* job) {
    job->api->finishScreen(job->call, 1, "{\"closed\":true,\"thread\":true}", NULL, 0);
}

// Ends the open panel with a result from a thread of the library, the way a native window ends when its user closes it.
NATIVE_TEST_EXPORT void native_test_close_screen_later(void) {
    NativeTestJob* job = native_test_job(native_test_close_screen);
    job->api = nativeTestApi;
    job->call = nativeTestScreen;
    native_test_spawn(job);
}

// Returns the handle of the window of the app as a pointer-wide integer, or -1 while the engine has none.
NATIVE_TEST_EXPORT intptr_t native_test_window(void) {
    HaylenNativeWindow window;
    return nativeTestApi->getWindow(&window) ? (intptr_t)window.handle : -1;
}

NATIVE_TEST_EXPORT void native_test_cover(int32_t covered) {
    if (covered) {
        nativeTestApi->coverApp();
    } else {
        nativeTestApi->uncoverApp();
    }
}

// Registers the handlers of the library, its screen and its error handler, declares it the native part of the `native-test` plugin and announces it from a thread of its own with a retained event, which waits for a listener that connects later.
NATIVE_TEST_EXPORT int native_test_haylen_init(const HaylenNativeApi* api) {
    if (api->version != HAYLEN_NATIVE_API_VERSION) {
        return 1;
    }
    nativeTestApi = api;
    api->registerHandler("native_test.echo", native_test_handle, NULL, NULL);
    api->registerHandler("native_test.echoBytes", native_test_handle, NULL, NULL);
    api->registerHandler("native_test.burst", native_test_handle, NULL, NULL);
    api->registerHandler("native_test.fail", native_test_handle, NULL, NULL);
    api->registerHandler("native_test.wait", native_test_handle, native_test_cancel, NULL);
    api->registerScreen("native-test", "panel", native_test_open_screen, native_test_cancel_screen, NULL);
    api->registerErrorHandler(native_test_app_failed, NULL);
    api->registerPlugin("native-test");
    api->log(HAYLEN_NATIVE_LOG_INFO, "The native test library is ready.");

    NativeTestJob* job = native_test_job(native_test_announce);
    job->api = api;
    job->value = api->version;
    native_test_spawn(job);
    return 0;
}

// Fails on purpose, so the tests see how a failing init reaches Lua.
NATIVE_TEST_EXPORT int native_test_failing_init(const HaylenNativeApi* api) {
    (void)api;
    return 7;
}
