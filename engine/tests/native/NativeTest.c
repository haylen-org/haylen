// A plain C library that the native interop tests and the native sample load on every platform. It exercises values, structs, buffers, text, callbacks from the calling thread and from threads of its own, and the HaylenNativeApi of the engine.

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

typedef struct NativeTestJob {
    void (*body)(struct NativeTestJob* job);
    NativeTestReporter reporter;
    int32_t value;
    uint64_t call;
    char* text;
} NativeTestJob;

static const HaylenNativeApi* nativeTestApi = NULL;

#if defined(_WIN32)
static DWORD WINAPI native_test_thread(LPVOID context) {
    NativeTestJob* job = (NativeTestJob*)context;
    job->body(job);
    free(job->text);
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
    nativeTestApi->resolve(job->call, 1, answer);
    free(answer);
}

static void native_test_announce(NativeTestJob* job) {
    char payload[64];
    snprintf(payload, sizeof(payload), "{\"version\":%d,\"origin\":\"%s\"}", job->value, NATIVE_TEST_ORIGIN);
    nativeTestApi->emit("native_test.ready", payload, 1);
}

// Answers native_test.echo from a thread of the library, fails native_test.fail with a code and data, and leaves native_test.wait pending until the app gives it up.
static void native_test_handle(void* user, uint64_t call, const char* method, const char* paramsJson) {
    (void)user;
    if (strcmp(method, "native_test.echo") == 0) {
        NativeTestJob* job = native_test_job(native_test_answer_echo);
        job->call = call;
        const size_t length = strlen(paramsJson) + 1;
        job->text = (char*)malloc(length);
        memcpy(job->text, paramsJson, length);
        native_test_spawn(job);
    } else if (strcmp(method, "native_test.fail") == 0) {
        nativeTestApi->resolve(call, 0, "{\"message\":\"The native test failed on purpose.\",\"code\":\"native_test_failure\",\"data\":{\"reason\":\"requested\"}}");
    }
}

static void native_test_cancel(void* user, uint64_t call) {
    (void)user;
    char payload[64];
    snprintf(payload, sizeof(payload), "{\"call\":%llu}", (unsigned long long)call);
    nativeTestApi->emit("native_test.cancelled", payload, 0);
}

// Registers the handlers of the library, declares it the native part of the native-test plugin and announces it from a thread of its own with a retained event, which waits for a listener that connects later.
NATIVE_TEST_EXPORT int native_test_haylen_init(const HaylenNativeApi* api) {
    if (api->version < HAYLEN_NATIVE_API_VERSION) {
        return 1;
    }
    nativeTestApi = api;
    api->registerHandler("native_test.echo", native_test_handle, NULL, NULL);
    api->registerHandler("native_test.fail", native_test_handle, NULL, NULL);
    api->registerHandler("native_test.wait", native_test_handle, native_test_cancel, NULL);
    api->registerPlugin("native-test");
    api->log(HAYLEN_NATIVE_LOG_INFO, "The native test library is ready.");

    NativeTestJob* job = native_test_job(native_test_announce);
    job->value = api->version;
    native_test_spawn(job);
    return 0;
}

// Fails on purpose, so the tests see how a failing init reaches Lua.
NATIVE_TEST_EXPORT int native_test_failing_init(const HaylenNativeApi* api) {
    (void)api;
    return 7;
}
