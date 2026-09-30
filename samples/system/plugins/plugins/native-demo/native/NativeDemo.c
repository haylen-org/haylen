// Desktop part of the Native Demo plugin, a C library for macOS, Windows and Linux built on the C library and the threads of each system. native.load hands its init function the HaylenNativeApi of the engine, through which the library answers the methods of the plugin, sends its events, pushes its video and audio streams, opens its confirm screen over the window of the app and hears the errors that stop the app. Its handlers, their cancel functions, its screen and its error handler run on the frame thread, and its threads only emit events, answer calls, end screens and push frames and samples.
// The desktops place no views of native libraries over the app, so the banner and the covering native screen fail with the code unsupported, and so do the file picker, which the desktop systems offer through no shared C API, and config, since native libraries receive no plugin parameters. The confirm screen is a window of its own, which NativeDemoScreen.m and NativeDemoScreen.c open with the window of the app from getWindow.

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "NativeDemoScreen.h"
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

// The work that runs on threads of the library, each with a generation that a new start, or stopping, raises, which ends the older threads when they wake up.
enum NativeDemoWork { NATIVE_DEMO_TICKS, NATIVE_DEMO_VIDEO, NATIVE_DEMO_TONE, NATIVE_DEMO_BURST, NATIVE_DEMO_WORKS };

// The pattern of the video stream, 30 frames per second of BGRA pixels, and the tone of the audio stream, 16-bit mono samples a tenth of a second ahead of the clock.
enum { NATIVE_DEMO_VIDEO_WIDTH = 320, NATIVE_DEMO_VIDEO_HEIGHT = 180, NATIVE_DEMO_VIDEO_FPS = 30, NATIVE_DEMO_TONE_RATE = 44100, NATIVE_DEMO_TONE_BLOCK = 441 };

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
    int count;
    double frequency;
    HaylenNativeVideoStream* video;
    HaylenNativeAudioStream* audio;
} NativeDemoJob;

static const HaylenNativeApi* nativeDemoApi = NULL;
static NativeDemoWait nativeDemoWaits[NATIVE_DEMO_WAITS];
static char* nativeDemoLastError = NULL;
static unsigned nativeDemoGenerations[NATIVE_DEMO_WORKS];

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

static double native_demo_now(void) {
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)frequency.QuadPart;
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

static double native_demo_now(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec / 1e9;
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

// Copies the text of a string value of the top-level object of json into value without its quotes and with its simple escapes undone, or the fallback when the key is missing.
static void native_demo_text(const char* json, const char* key, char* value, size_t size, const char* fallback) {
    char* quoted = (char*)malloc(size + 2);
    if (!native_demo_field(json, key, quoted, size + 2) || quoted[0] != '"') {
        snprintf(value, size, "%s", fallback);
        free(quoted);
        return;
    }
    size_t length = 0;
    for (const char* cursor = quoted + 1; *cursor != '\0' && *cursor != '"' && length + 1 < size; ++cursor) {
        if (*cursor == '\\' && cursor[1] != '\0') {
            ++cursor;
            value[length++] = *cursor == 'n' ? ' ' : *cursor;
            continue;
        }
        value[length++] = *cursor;
    }
    value[length] = '\0';
    free(quoted);
}

static void native_demo_unsupported(uint64_t call, const char* message) {
    char failure[NATIVE_DEMO_TEXT];
    snprintf(failure, sizeof(failure), "{\"message\":\"%s\",\"code\":\"unsupported\",\"data\":{\"language\":\"C\"}}", message);
    nativeDemoApi->resolve(call, 0, failure, NULL, 0);
}

static int native_demo_number(const char* json, const char* key, int fallback) {
    char value[32];
    return native_demo_field(json, key, value, sizeof(value)) ? (int)strtol(value, NULL, 10) : fallback;
}

// Raises the generation of the work, which ends the threads of the earlier generation, and returns the new one.
static unsigned native_demo_restart(enum NativeDemoWork work) {
    native_demo_lock();
    const unsigned generation = ++nativeDemoGenerations[work];
    native_demo_unlock();
    return generation;
}

static int native_demo_current(enum NativeDemoWork work, unsigned generation) {
    native_demo_lock();
    const int current = nativeDemoGenerations[work] == generation;
    native_demo_unlock();
    return current;
}

static void native_demo_echo(uint64_t call, const char* paramsJson) {
    const size_t size = strlen(paramsJson) + 64;
    char* value = (char*)malloc(size);
    char* answer = (char*)malloc(size + 64);
    if (!native_demo_field(paramsJson, "value", value, size)) {
        snprintf(value, size, "null");
    }
    snprintf(answer, size + 64, "{\"echo\":%s,\"thread\":\"frame\",\"language\":\"C\"}", value);
    nativeDemoApi->resolve(call, 1, answer, NULL, 0);
    free(answer);
    free(value);
}

// Answers with the bytes it received, which stay valid while the handler runs, since resolve copies them.
static void native_demo_echo_bytes(uint64_t call, const HaylenNativeBuffer* buffers, size_t bufferCount) {
    if (bufferCount == 0) {
        nativeDemoApi->resolve(call, 0, "{\"message\":\"echoBytes needs bytes.\",\"code\":\"invalidParams\"}", NULL, 0);
        return;
    }
    char answer[NATIVE_DEMO_TEXT];
    snprintf(answer, sizeof(answer), "{\"data\":{\"$bytes\":0},\"size\":%zu,\"thread\":\"frame\",\"language\":\"C\"}", buffers[0].size);
    nativeDemoApi->resolve(call, 1, answer, buffers, 1);
}

// Draws the pattern of the demo: a gradient from red to green with blue stripes that move with the frame, as RGBA or BGRA pixels.
static void native_demo_draw(uint8_t* pixels, int width, int height, int frame, int bgra) {
    for (int row = 0; row < height; ++row) {
        for (int column = 0; column < width; ++column) {
            uint8_t* pixel = pixels + ((size_t)row * (size_t)width + (size_t)column) * 4;
            const uint8_t red = (uint8_t)(column * 255 / (width > 1 ? width - 1 : 1));
            const uint8_t blue = ((column + row + frame * 4) / 16) % 2 == 0 ? 230 : 40;
            pixel[bgra ? 2 : 0] = red;
            pixel[1] = (uint8_t)(row * 255 / (height > 1 ? height - 1 : 1));
            pixel[bgra ? 0 : 2] = blue;
            pixel[3] = 255;
        }
    }
}

static uint32_t native_demo_crc(const uint8_t* bytes, size_t size, uint32_t crc) {
    crc = ~crc;
    for (size_t index = 0; index < size; ++index) {
        crc ^= bytes[index];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
        }
    }
    return ~crc;
}

static uint8_t* native_demo_put32(uint8_t* cursor, uint32_t value) {
    cursor[0] = (uint8_t)(value >> 24);
    cursor[1] = (uint8_t)(value >> 16);
    cursor[2] = (uint8_t)(value >> 8);
    cursor[3] = (uint8_t)value;
    return cursor + 4;
}

// Writes the chunk of the type with the data that already sits after its header, and returns where the next chunk starts.
static uint8_t* native_demo_chunk(uint8_t* chunk, const char* type, size_t size) {
    native_demo_put32(chunk, (uint32_t)size);
    memcpy(chunk + 4, type, 4);
    return native_demo_put32(chunk + 8 + size, native_demo_crc(chunk + 4, size + 4, 0));
}

// Encodes RGBA pixels as a PNG file with stored deflate blocks, which needs no compressor, and returns it with its size.
static uint8_t* native_demo_png(const uint8_t* pixels, int width, int height, size_t* size) {
    const size_t row = (size_t)width * 4 + 1;
    const size_t raw = row * (size_t)height;
    const size_t blocks = (raw + 65534) / 65535;
    const size_t deflated = 2 + raw + blocks * 5 + 4;
    *size = 8 + 25 + 12 + deflated + 12;
    uint8_t* png = (uint8_t*)malloc(*size);
    static const uint8_t signature[8] = {137, 'P', 'N', 'G', '\r', '\n', 26, '\n'};
    memcpy(png, signature, 8);

    uint8_t* header = png + 8;
    uint8_t* fields = native_demo_put32(native_demo_put32(header + 8, (uint32_t)width), (uint32_t)height);
    const uint8_t format[5] = {8, 6, 0, 0, 0};
    memcpy(fields, format, 5);
    uint8_t* data = native_demo_chunk(header, "IHDR", 13);

    // The zlib stream holds the rows, each after its filter byte of 0, in blocks of at most 65535 bytes, and ends with the Adler-32 of the rows.
    uint8_t* cursor = data + 8;
    *cursor++ = 0x78;
    *cursor++ = 0x01;
    uint32_t low = 1;
    uint32_t high = 0;
    size_t left = 0;
    for (size_t offset = 0; offset < raw; ++offset, --left) {
        if (left == 0) {
            left = raw - offset < 65535 ? raw - offset : 65535;
            *cursor++ = offset + left == raw ? 1 : 0;
            *cursor++ = (uint8_t)left;
            *cursor++ = (uint8_t)(left >> 8);
            *cursor++ = (uint8_t)~left;
            *cursor++ = (uint8_t)(~left >> 8);
        }
        const size_t column = offset % row;
        const uint8_t byte = column == 0 ? 0 : pixels[(offset / row) * (row - 1) + column - 1];
        *cursor++ = byte;
        low = (low + byte) % 65521;
        high = (high + low) % 65521;
    }
    native_demo_put32(cursor, (high << 16) | low);
    uint8_t* end = native_demo_chunk(data, "IDAT", deflated);
    native_demo_chunk(end, "IEND", 0);
    return png;
}

// Draws the pattern natively and answers with it as the bytes of a PNG file.
static void native_demo_generated_image(uint64_t call, const char* paramsJson) {
    const int width = native_demo_number(paramsJson, "width", 0);
    const int height = native_demo_number(paramsJson, "height", 0);
    if (width < 1 || height < 1 || width > 2048 || height > 2048) {
        nativeDemoApi->resolve(call, 0, "{\"message\":\"generatedImage needs a width and a height from 1 to 2048.\",\"code\":\"invalidParams\"}", NULL, 0);
        return;
    }
    uint8_t* pixels = (uint8_t*)malloc((size_t)width * (size_t)height * 4);
    native_demo_draw(pixels, width, height, 0, 0);
    size_t size = 0;
    uint8_t* png = native_demo_png(pixels, width, height, &size);
    const HaylenNativeBuffer buffer = {png, size};
    char answer[NATIVE_DEMO_TEXT];
    snprintf(answer, sizeof(answer), "{\"png\":{\"$bytes\":0},\"width\":%d,\"height\":%d,\"drawnWith\":\"a PNG encoder in C\",\"language\":\"C\"}", width, height);
    nativeDemoApi->resolve(call, 1, answer, &buffer, 1);
    free(png);
    free(pixels);
}

// Pushes a frame of the pattern into the video stream 30 times per second, with the seconds since the start as its timestamp.
static void native_demo_play_video(NativeDemoJob* job) {
    uint8_t* pixels = (uint8_t*)malloc((size_t)NATIVE_DEMO_VIDEO_WIDTH * NATIVE_DEMO_VIDEO_HEIGHT * 4);
    const double start = native_demo_now();
    for (int frame = 0; native_demo_current(NATIVE_DEMO_VIDEO, job->generation); ++frame) {
        native_demo_draw(pixels, NATIVE_DEMO_VIDEO_WIDTH, NATIVE_DEMO_VIDEO_HEIGHT, frame, 1);
        nativeDemoApi->pushVideoFrame(job->video, pixels, NATIVE_DEMO_VIDEO_WIDTH, NATIVE_DEMO_VIDEO_HEIGHT, NATIVE_DEMO_VIDEO_WIDTH * 4, native_demo_now() - start);
        const double wait = start + (double)(frame + 1) / (double)NATIVE_DEMO_VIDEO_FPS - native_demo_now();
        if (wait > 0.0) {
            native_demo_sleep((unsigned)(wait * 1000.0));
        }
    }
    free(pixels);
}

// Synthesizes a sine wave a tenth of a second ahead of the clock, so the voice that plays it never runs dry, while the ring of the stream drops what does not fit when no voice plays it.
static void native_demo_play_tone(NativeDemoJob* job) {
    int16_t samples[NATIVE_DEMO_TONE_BLOCK];
    const double step = 2.0 * 3.14159265358979323846 * job->frequency / (double)NATIVE_DEMO_TONE_RATE;
    const double start = native_demo_now();
    double phase = 0.0;
    double written = 0.0;
    while (native_demo_current(NATIVE_DEMO_TONE, job->generation)) {
        while (written < (native_demo_now() - start + 0.1) * (double)NATIVE_DEMO_TONE_RATE) {
            for (int index = 0; index < NATIVE_DEMO_TONE_BLOCK; ++index) {
                samples[index] = (int16_t)(sin(phase) * 0.3 * 32767.0);
                phase = fmod(phase + step, 2.0 * 3.14159265358979323846);
            }
            nativeDemoApi->pushAudioFrames(job->audio, samples, NATIVE_DEMO_TONE_BLOCK);
            written += (double)NATIVE_DEMO_TONE_BLOCK;
        }
        native_demo_sleep(5);
    }
}

// Sends count batched events 30 times per second for ticks ticks, which reach the app as one list per frame, and then burstDone.
static void native_demo_send_bursts(NativeDemoJob* job) {
    const double start = native_demo_now();
    for (int tick = 0; tick < job->limit; ++tick) {
        if (!native_demo_current(NATIVE_DEMO_BURST, job->generation)) {
            return;
        }
        for (int index = 0; index < job->count; ++index) {
            char payload[128];
            snprintf(payload, sizeof(payload), "{\"tick\":%d,\"index\":%d,\"language\":\"C\"}", tick, index);
            nativeDemoApi->emit("native-demo.burst", payload, NULL, 0, HAYLEN_NATIVE_EMIT_BATCHED);
        }
        const double wait = start + (double)(tick + 1) / 30.0 - native_demo_now();
        if (wait > 0.0) {
            native_demo_sleep((unsigned)(wait * 1000.0));
        }
    }
    char payload[128];
    snprintf(payload, sizeof(payload), "{\"events\":%d,\"ticks\":%ld,\"language\":\"C\"}", job->count * (int)job->limit, job->limit);
    nativeDemoApi->emit("native-demo.burstDone", payload, NULL, 0, 0);
}

static void native_demo_start_video(uint64_t call) {
    NativeDemoJob* job = native_demo_job(native_demo_play_video);
    job->generation = native_demo_restart(NATIVE_DEMO_VIDEO);
    job->video = nativeDemoApi->openVideoStream("native-demo", "pattern", HAYLEN_NATIVE_PIXELS_BGRA8, NATIVE_DEMO_VIDEO_WIDTH, NATIVE_DEMO_VIDEO_HEIGHT);
    native_demo_spawn(job);
    char answer[NATIVE_DEMO_TEXT];
    snprintf(answer, sizeof(answer), "{\"width\":%d,\"height\":%d,\"fps\":%d,\"format\":\"BGRA\",\"thread\":\"a thread of the library\",\"language\":\"C\"}", NATIVE_DEMO_VIDEO_WIDTH, NATIVE_DEMO_VIDEO_HEIGHT, NATIVE_DEMO_VIDEO_FPS);
    nativeDemoApi->resolve(call, 1, answer, NULL, 0);
}

static void native_demo_start_tone(uint64_t call, const char* paramsJson) {
    NativeDemoJob* job = native_demo_job(native_demo_play_tone);
    job->generation = native_demo_restart(NATIVE_DEMO_TONE);
    job->frequency = native_demo_number(paramsJson, "frequency", 440);
    job->audio = nativeDemoApi->openAudioStream("native-demo", "tone", NATIVE_DEMO_TONE_RATE, 1, HAYLEN_NATIVE_SAMPLES_INT16, NATIVE_DEMO_TONE_RATE);
    native_demo_spawn(job);
    char answer[NATIVE_DEMO_TEXT];
    snprintf(answer, sizeof(answer), "{\"frequency\":%g,\"sampleRate\":%d,\"channels\":1,\"format\":\"int16\",\"language\":\"C\"}", job->frequency, NATIVE_DEMO_TONE_RATE);
    nativeDemoApi->resolve(call, 1, answer, NULL, 0);
}

static void native_demo_start_bursts(uint64_t call, const char* paramsJson) {
    NativeDemoJob* job = native_demo_job(native_demo_send_bursts);
    job->generation = native_demo_restart(NATIVE_DEMO_BURST);
    job->count = native_demo_number(paramsJson, "count", 100);
    job->limit = native_demo_number(paramsJson, "ticks", 30);
    native_demo_spawn(job);
    char answer[NATIVE_DEMO_TEXT];
    snprintf(answer, sizeof(answer), "{\"count\":%d,\"ticks\":%ld,\"language\":\"C\"}", job->count, job->limit);
    nativeDemoApi->resolve(call, 1, answer, NULL, 0);
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
    nativeDemoApi->resolve(job->call, 1, answer, NULL, 0);
}

static void native_demo_compute(uint64_t call, const char* paramsJson) {
    char limit[32];
    if (!native_demo_field(paramsJson, "limit", limit, sizeof(limit))) {
        nativeDemoApi->resolve(call, 0, "{\"message\":\"compute needs a limit.\",\"code\":\"invalidParams\"}", NULL, 0);
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
    nativeDemoApi->resolve(call, 0, "{\"message\":\"Too many waits are pending.\",\"code\":\"busy\"}", NULL, 0);
}

static void native_demo_cancel(void* user, uint64_t call) {
    (void)user;
    for (int index = 0; index < NATIVE_DEMO_WAITS; ++index) {
        if (nativeDemoWaits[index].call == call) {
            char payload[128];
            snprintf(payload, sizeof(payload), "{\"token\":%s,\"language\":\"C\"}", nativeDemoWaits[index].token);
            nativeDemoApi->emit("native-demo.waitCancelled", payload, NULL, 0, 0);
            nativeDemoWaits[index].call = 0;
            return;
        }
    }
}

static void native_demo_tick(NativeDemoJob* job) {
    for (int count = 1;; ++count) {
        native_demo_sleep(job->milliseconds);
        if (!native_demo_current(NATIVE_DEMO_TICKS, job->generation)) {
            return;
        }
        char payload[128];
        snprintf(payload, sizeof(payload), "{\"count\":%d,\"thread\":\"background\",\"language\":\"C\"}", count);
        nativeDemoApi->emit("native-demo.tick", payload, NULL, 0, 0);
    }
}

static void native_demo_ticks(uint64_t call, const char* paramsJson) {
    char enabled[16];
    char interval[32];
    if (!native_demo_field(paramsJson, "enabled", enabled, sizeof(enabled)) || !native_demo_field(paramsJson, "interval", interval, sizeof(interval))) {
        nativeDemoApi->resolve(call, 0, "{\"message\":\"ticks needs enabled and interval.\",\"code\":\"invalidParams\"}", NULL, 0);
        return;
    }

    const unsigned generation = native_demo_restart(NATIVE_DEMO_TICKS);
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
    nativeDemoApi->resolve(call, 1, answer, NULL, 0);
}

// Every app that loads the Lua API sends start. The library stops the ticks, the streams and the bursts that an earlier app of the process left running and hands the new app the error that stopped the earlier one.
static void native_demo_start(uint64_t call) {
    for (int work = 0; work < NATIVE_DEMO_WORKS; ++work) {
        native_demo_restart((enum NativeDemoWork)work);
    }
    if (nativeDemoLastError != NULL) {
        nativeDemoApi->emit("native-demo.lastError", nativeDemoLastError, NULL, 0, HAYLEN_NATIVE_EMIT_RETAIN);
        free(nativeDemoLastError);
        nativeDemoLastError = NULL;
    }
    nativeDemoApi->resolve(call, 1, "null", NULL, 0);
}

static void native_demo_stop(uint64_t call, enum NativeDemoWork work) {
    native_demo_restart(work);
    nativeDemoApi->resolve(call, 1, "null", NULL, 0);
}

static void native_demo_handle(void* user, uint64_t call, const char* method, const char* paramsJson, const HaylenNativeBuffer* buffers, size_t bufferCount) {
    (void)user;
    const char* name = method + strlen("native-demo.");
    if (strcmp(name, "echo") == 0) {
        native_demo_echo(call, paramsJson);
    } else if (strcmp(name, "echoBytes") == 0) {
        native_demo_echo_bytes(call, buffers, bufferCount);
    } else if (strcmp(name, "generatedImage") == 0) {
        native_demo_generated_image(call, paramsJson);
    } else if (strcmp(name, "startVideo") == 0) {
        native_demo_start_video(call);
    } else if (strcmp(name, "stopVideo") == 0) {
        native_demo_stop(call, NATIVE_DEMO_VIDEO);
    } else if (strcmp(name, "startTone") == 0) {
        native_demo_start_tone(call, paramsJson);
    } else if (strcmp(name, "stopTone") == 0) {
        native_demo_stop(call, NATIVE_DEMO_TONE);
    } else if (strcmp(name, "burst") == 0) {
        native_demo_start_bursts(call, paramsJson);
    } else if (strcmp(name, "compute") == 0) {
        native_demo_compute(call, paramsJson);
    } else if (strcmp(name, "fail") == 0) {
        nativeDemoApi->resolve(call, 0, "{\"message\":\"The native demo failed on purpose.\",\"code\":\"demoFailure\",\"data\":{\"reason\":\"requested\",\"language\":\"C\"}}", NULL, 0);
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

// Opens the confirm screen, a native window over the window of the app, with the title and the question of the parameters.
static void native_demo_open_screen(void* user, uint64_t screen, const char* paramsJson, const HaylenNativeBuffer* buffers, size_t bufferCount) {
    (void)user;
    (void)buffers;
    (void)bufferCount;
    char title[NATIVE_DEMO_TEXT];
    char question[NATIVE_DEMO_TEXT];
    native_demo_text(paramsJson, "title", title, sizeof(title), "Native Demo");
    native_demo_text(paramsJson, "question", question, sizeof(question), "");
    native_demo_screen_open(nativeDemoApi, screen, title, question);
}

static void native_demo_cancel_screen(void* user, uint64_t screen) {
    (void)user;
    native_demo_screen_cancel(screen);
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

// Registers the methods of the plugin, its confirm screen and the error handler, declares the library the native part of native-demo and sends loaded retained, so the first listener of the app receives it however late it connects.
NATIVE_DEMO_EXPORT int native_demo_haylen_init(const HaylenNativeApi* api) {
    static const char* const methods[] = {"echo", "echoBytes", "generatedImage", "compute", "fail", "ticks", "startVideo", "stopVideo", "startTone", "stopTone", "burst", "config", "start", "showBanner", "setBannerVisible", "removeBanner", "showScreen", "pickFile"};
    if (api->version != HAYLEN_NATIVE_API_VERSION) {
        return 1;
    }
    nativeDemoApi = api;
    for (size_t index = 0; index < sizeof(methods) / sizeof(methods[0]); ++index) {
        char name[64];
        snprintf(name, sizeof(name), "native-demo.%s", methods[index]);
        api->registerHandler(name, native_demo_handle, NULL, NULL);
    }
    api->registerHandler("native-demo.wait", native_demo_handle, native_demo_cancel, NULL);
    api->registerScreen("native-demo", "confirm", native_demo_open_screen, native_demo_cancel_screen, NULL);
    api->registerErrorHandler(native_demo_app_failed, NULL);
    api->registerPlugin("native-demo");

    char payload[128];
    snprintf(payload, sizeof(payload), "{\"language\":\"C\",\"platform\":\"%s\"}", nativeDemoPlatform);
    api->emit("native-demo.loaded", payload, NULL, 0, HAYLEN_NATIVE_EMIT_RETAIN);
    return 0;
}
