// Desktop part of the Native Demo plugin, a C library for macOS, Windows and Linux built on the C library and the threads of each system. native.load hands its init function the HaylenNativeApi of the engine, through which the library answers the methods of the plugin, sends its events and hears the errors that stop the app. Its handlers, their cancel functions and its error handler run on the frame thread, and its threads only emit events and answer calls.
// The desktops give native libraries no view API, so the banner and the native screen fail with the code unsupported, and so do the file picker, which the desktop systems offer through no shared C API, and config, since native libraries receive no plugin parameters.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "haylen/platform/native/HaylenNative.h"

#if defined(_WIN32)
#include <windows.h>
#define NATIVE_DEMO_EXPORT __declspec(dllexport)
#else
#include <pthread.h>
#include <time.h>
#define NATIVE_DEMO_EXPORT __attribute__((visibility("default")))
#endif

#if defined(_WIN32)
static const char* const nativeDemoPlatform = "windows";
#elif defined(__APPLE__)
static const char* const nativeDemoPlatform = "macos";
#else
static const char* const nativeDemoPlatform = "linux";
#endif

enum { NATIVE_DEMO_WAITS = 32, NATIVE_DEMO_TEXT = 512 };

typedef struct NativeDemoWait {
    uint64_t call;
    char token[32];
} NativeDemoWait;

typedef struct NativeDemoJob {
    void (*body)(struct NativeDemoJob* job);
    uint64_t call;
    long limit;
    unsigned generation;
    unsigned milliseconds;
} NativeDemoJob;

static const HaylenNativeApi* nativeDemoApi = NULL;
static NativeDemoWait nativeDemoWaits[NATIVE_DEMO_WAITS];
static char* nativeDemoLastError = NULL;
// The ticker thread of the current generation sends ticks, and a new generation, or stopping, ends the older threads when they wake up.
static unsigned nativeDemoTickerGeneration = 0;

#if defined(_WIN32)
static SRWLOCK nativeDemoLock = SRWLOCK_INIT;

static void native_demo_lock(void) {
    AcquireSRWLockExclusive(&nativeDemoLock);
}

static void native_demo_unlock(void) {
    ReleaseSRWLockExclusive(&nativeDemoLock);
}

static void native_demo_sleep(unsigned milliseconds) {
    Sleep(milliseconds);
}

static DWORD WINAPI native_demo_thread(LPVOID context) {
    NativeDemoJob* job = (NativeDemoJob*)context;
    job->body(job);
    free(job);
    return 0;
}

static void native_demo_spawn(NativeDemoJob* job) {
    HANDLE thread = CreateThread(NULL, 0, native_demo_thread, job, 0, NULL);
    CloseHandle(thread);
}
#else
static pthread_mutex_t nativeDemoLock = PTHREAD_MUTEX_INITIALIZER;

static void native_demo_lock(void) {
    pthread_mutex_lock(&nativeDemoLock);
}

static void native_demo_unlock(void) {
    pthread_mutex_unlock(&nativeDemoLock);
}

static void native_demo_sleep(unsigned milliseconds) {
    struct timespec duration = {(time_t)(milliseconds / 1000), (long)(milliseconds % 1000) * 1000000L};
    nanosleep(&duration, NULL);
}

static void* native_demo_thread(void* context) {
    NativeDemoJob* job = (NativeDemoJob*)context;
    job->body(job);
    free(job);
    return NULL;
}

static void native_demo_spawn(NativeDemoJob* job) {
    pthread_t thread;
    pthread_create(&thread, NULL, native_demo_thread, job);
    pthread_detach(thread);
}
#endif

static NativeDemoJob* native_demo_job(void (*body)(NativeDemoJob* job)) {
    NativeDemoJob* job = (NativeDemoJob*)calloc(1, sizeof(NativeDemoJob));
    job->body = body;
    return job;
}

// Returns where the JSON value after a JSON value starting at text begins, skipping strings with their escapes and nested objects and arrays whole.
static const char* native_demo_skip(const char* text) {
    if (*text == '"') {
        for (++text; *text != '\0' && *text != '"'; ++text) {
            if (*text == '\\' && text[1] != '\0') {
                ++text;
            }
        }
        return *text == '"' ? text + 1 : text;
    }
    if (*text == '{' || *text == '[') {
        int depth = 0;
        do {
            if (*text == '"') {
                text = native_demo_skip(text);
                continue;
            }
            depth += (*text == '{' || *text == '[') ? 1 : (*text == '}' || *text == ']') ? -1 : 0;
            ++text;
        } while (depth > 0 && *text != '\0');
        return text;
    }
    while (*text != '\0' && *text != ',' && *text != '}' && *text != ']' && *text != ' ' && *text != '\n' && *text != '\r' && *text != '\t') {
        ++text;
    }
    return text;
}

static const char* native_demo_space(const char* text) {
    while (*text == ' ' || *text == '\n' || *text == '\r' || *text == '\t') {
        ++text;
    }
    return text;
}

// Copies the JSON text of the value of a key of the top-level object of json into value, and returns 0 when the key is missing or its value does not fit.
static int native_demo_field(const char* json, const char* key, char* value, size_t size) {
    const size_t keyLength = strlen(key);
    const char* cursor = native_demo_space(json);
    if (*cursor != '{') {
        return 0;
    }
    cursor = native_demo_space(cursor + 1);
    while (*cursor == '"') {
        const char* name = cursor + 1;
        const char* nameEnd = native_demo_skip(cursor);
        const int matches = (size_t)(nameEnd - name - 1) == keyLength && strncmp(name, key, keyLength) == 0;
        cursor = native_demo_space(nameEnd);
        if (*cursor != ':') {
            return 0;
        }
        const char* start = native_demo_space(cursor + 1);
        const char* end = native_demo_skip(start);
        if (matches) {
            const size_t length = (size_t)(end - start);
            if (length == 0 || length >= size) {
                return 0;
            }
            memcpy(value, start, length);
            value[length] = '\0';
            return 1;
        }
        cursor = native_demo_space(end);
        if (*cursor != ',') {
            return 0;
        }
        cursor = native_demo_space(cursor + 1);
    }
    return 0;
}

static void native_demo_unsupported(uint64_t call, const char* message) {
    char failure[NATIVE_DEMO_TEXT];
    snprintf(failure, sizeof(failure), "{\"message\":\"%s\",\"code\":\"unsupported\",\"data\":{\"language\":\"C\"}}", message);
    nativeDemoApi->resolve(call, 0, failure);
}

static void native_demo_echo(uint64_t call, const char* paramsJson) {
    const size_t size = strlen(paramsJson) + 64;
    char* value = (char*)malloc(size);
    char* answer = (char*)malloc(size + 64);
    if (!native_demo_field(paramsJson, "value", value, size)) {
        snprintf(value, size, "null");
    }
    snprintf(answer, size + 64, "{\"echo\":%s,\"thread\":\"frame\",\"language\":\"C\"}", value);
    nativeDemoApi->resolve(call, 1, answer);
    free(answer);
    free(value);
}

static void native_demo_count_primes(NativeDemoJob* job) {
    char* composite = (char*)calloc(job->limit > 0 ? (size_t)job->limit : 1, 1);
    long count = 0;
    for (long number = 2; number < job->limit; ++number) {
        if (composite[number]) {
            continue;
        }
        ++count;
        for (long long multiple = (long long)number * number; multiple < job->limit; multiple += number) {
            composite[multiple] = 1;
        }
    }
    free(composite);

    char answer[NATIVE_DEMO_TEXT];
    snprintf(answer, sizeof(answer), "{\"primes\":%ld,\"thread\":\"background\",\"detail\":\"a thread of the library\",\"language\":\"C\"}", count);
    nativeDemoApi->resolve(job->call, 1, answer);
}

static void native_demo_compute(uint64_t call, const char* paramsJson) {
    char limit[32];
    if (!native_demo_field(paramsJson, "limit", limit, sizeof(limit))) {
        nativeDemoApi->resolve(call, 0, "{\"message\":\"compute needs a limit.\",\"code\":\"invalidParams\"}");
        return;
    }
    NativeDemoJob* job = native_demo_job(native_demo_count_primes);
    job->call = call;
    job->limit = strtol(limit, NULL, 10);
    native_demo_spawn(job);
}

// The call never answers by itself, so only a cancel or a timeout of the app ends it, which the cancel function reports.
static void native_demo_wait(uint64_t call, const char* paramsJson) {
    for (int index = 0; index < NATIVE_DEMO_WAITS; ++index) {
        if (nativeDemoWaits[index].call == 0) {
            nativeDemoWaits[index].call = call;
            if (!native_demo_field(paramsJson, "token", nativeDemoWaits[index].token, sizeof(nativeDemoWaits[index].token))) {
                snprintf(nativeDemoWaits[index].token, sizeof(nativeDemoWaits[index].token), "null");
            }
            return;
        }
    }
    nativeDemoApi->resolve(call, 0, "{\"message\":\"Too many waits are pending.\",\"code\":\"busy\"}");
}

static void native_demo_cancel(void* user, uint64_t call) {
    (void)user;
    for (int index = 0; index < NATIVE_DEMO_WAITS; ++index) {
        if (nativeDemoWaits[index].call == call) {
            char payload[128];
            snprintf(payload, sizeof(payload), "{\"token\":%s,\"language\":\"C\"}", nativeDemoWaits[index].token);
            nativeDemoApi->emit("native-demo.waitCancelled", payload, 0);
            nativeDemoWaits[index].call = 0;
            return;
        }
    }
}

static void native_demo_tick(NativeDemoJob* job) {
    for (int count = 1;; ++count) {
        native_demo_sleep(job->milliseconds);
        native_demo_lock();
        const int current = job->generation == nativeDemoTickerGeneration;
        native_demo_unlock();
        if (!current) {
            return;
        }
        char payload[128];
        snprintf(payload, sizeof(payload), "{\"count\":%d,\"thread\":\"background\",\"language\":\"C\"}", count);
        nativeDemoApi->emit("native-demo.tick", payload, 0);
    }
}

static unsigned native_demo_stop_ticking(void) {
    native_demo_lock();
    const unsigned generation = ++nativeDemoTickerGeneration;
    native_demo_unlock();
    return generation;
}

static void native_demo_ticks(uint64_t call, const char* paramsJson) {
    char enabled[16];
    char interval[32];
    if (!native_demo_field(paramsJson, "enabled", enabled, sizeof(enabled)) || !native_demo_field(paramsJson, "interval", interval, sizeof(interval))) {
        nativeDemoApi->resolve(call, 0, "{\"message\":\"ticks needs enabled and interval.\",\"code\":\"invalidParams\"}");
        return;
    }

    const unsigned generation = native_demo_stop_ticking();
    const int start = strcmp(enabled, "true") == 0;
    const double seconds = strtod(interval, NULL);
    if (start) {
        NativeDemoJob* job = native_demo_job(native_demo_tick);
        job->generation = generation;
        job->milliseconds = (unsigned)(seconds * 1000.0);
        native_demo_spawn(job);
    }

    char answer[128];
    snprintf(answer, sizeof(answer), "{\"enabled\":%s,\"interval\":%s}", start ? "true" : "false", interval);
    nativeDemoApi->resolve(call, 1, answer);
}

// Every app that loads the Lua API sends start. The library stops the ticks that an earlier app of the process left running and hands the new app the error that stopped the earlier one.
static void native_demo_start(uint64_t call) {
    native_demo_stop_ticking();
    if (nativeDemoLastError != NULL) {
        nativeDemoApi->emit("native-demo.lastError", nativeDemoLastError, 1);
        free(nativeDemoLastError);
        nativeDemoLastError = NULL;
    }
    nativeDemoApi->resolve(call, 1, "null");
}

static void native_demo_handle(void* user, uint64_t call, const char* method, const char* paramsJson) {
    (void)user;
    const char* name = method + strlen("native-demo.");
    if (strcmp(name, "echo") == 0) {
        native_demo_echo(call, paramsJson);
    } else if (strcmp(name, "compute") == 0) {
        native_demo_compute(call, paramsJson);
    } else if (strcmp(name, "fail") == 0) {
        nativeDemoApi->resolve(call, 0, "{\"message\":\"The native demo failed on purpose.\",\"code\":\"demoFailure\",\"data\":{\"reason\":\"requested\",\"language\":\"C\"}}");
    } else if (strcmp(name, "wait") == 0) {
        native_demo_wait(call, paramsJson);
    } else if (strcmp(name, "ticks") == 0) {
        native_demo_ticks(call, paramsJson);
    } else if (strcmp(name, "start") == 0) {
        native_demo_start(call);
    } else if (strcmp(name, "config") == 0) {
        native_demo_unsupported(call, "Native libraries receive no plugin parameters, so the desktop part leaves them to the Lua API of the plugin.");
    } else if (strcmp(name, "showScreen") == 0) {
        native_demo_unsupported(call, "The desktops give native libraries no view API, so the plugin cannot show a native screen over the app.");
    } else if (strcmp(name, "pickFile") == 0) {
        native_demo_unsupported(call, "The desktop systems share no C API for a file picker, so the desktop part of the plugin has none.");
    } else {
        // The methods left are showBanner, setBannerVisible and removeBanner.
        native_demo_unsupported(call, "The desktops give native libraries no view API, so the plugin cannot place a banner over the app.");
    }
}

// Keeps the message, the file and the line of the error that stopped the app, which the next app receives when it sends start.
static void native_demo_app_failed(void* user, const char* reportJson) {
    (void)user;
    const size_t size = strlen(reportJson) + 1;
    char* message = (char*)malloc(size);
    char* file = (char*)malloc(size);
    char line[32];
    if (!native_demo_field(reportJson, "message", message, size)) {
        snprintf(message, size, "\"\"");
    }
    if (!native_demo_field(reportJson, "file", file, size)) {
        snprintf(file, size, "\"\"");
    }
    if (!native_demo_field(reportJson, "line", line, sizeof(line))) {
        snprintf(line, sizeof(line), "0");
    }

    const size_t length = strlen(message) + strlen(file) + strlen(line) + 64;
    free(nativeDemoLastError);
    nativeDemoLastError = (char*)malloc(length);
    snprintf(nativeDemoLastError, length, "{\"message\":%s,\"file\":%s,\"line\":%s,\"language\":\"C\"}", message, file, line);
    free(file);
    free(message);
}

// Registers the methods of the plugin and the error handler, declares the library the native part of native-demo and sends loaded retained, so the first listener of the app receives it however late it connects.
NATIVE_DEMO_EXPORT int native_demo_haylen_init(const HaylenNativeApi* api) {
    static const char* const methods[] = {"echo", "compute", "fail", "ticks", "config", "start", "showBanner", "setBannerVisible", "removeBanner", "showScreen", "pickFile"};
    if (api->version < HAYLEN_NATIVE_API_VERSION) {
        return 1;
    }
    nativeDemoApi = api;
    for (size_t index = 0; index < sizeof(methods) / sizeof(methods[0]); ++index) {
        char name[64];
        snprintf(name, sizeof(name), "native-demo.%s", methods[index]);
        api->registerHandler(name, native_demo_handle, NULL, NULL);
    }
    api->registerHandler("native-demo.wait", native_demo_handle, native_demo_cancel, NULL);
    api->registerErrorHandler(native_demo_app_failed, NULL);
    api->registerPlugin("native-demo");

    char payload[128];
    snprintf(payload, sizeof(payload), "{\"language\":\"C\",\"platform\":\"%s\"}", nativeDemoPlatform);
    api->emit("native-demo.loaded", payload, 1);
    return 0;
}
