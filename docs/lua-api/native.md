# 📖 `haylen.native`

The module `haylen.native` loads native libraries for the `ffi` module of Varn, finds their symbols and makes callbacks that native code may call from any thread. Varn's `ffi` declares the C functions and types of a library and calls them. This module finds the file of the library where the app ships it, hands the library the C interface of the engine and brings the calls of native code back to the frame thread. The [native code guide](../platform/native.md) explains when to use it, how to ship libraries with an app and how to call SDKs that have a C API.

```lua
local ffi = require('ffi')
local native = require('haylen.native')
```

The browser loads no native libraries. There `native.available()` returns `false`, and `native.findSymbol` returns `nil`. The function `native.load` raises `Native libraries are not available in the browser. Call JavaScript through "haylen.platform" instead.`, and `native.callback` raises `Native callbacks are not available in the browser. Call JavaScript through "haylen.platform" instead.`.

## Functions

### native.available()

Returns `true` where the platform loads native libraries, which is every platform but the browser.

```lua
local native = require('haylen.native')

if not native.available() then
    print('This build calls JavaScript through haylen.platform instead.')
end
```

### native.load(name, options)

Loads a library and returns the namespace of Varn's `ffi` whose fields are the functions that `ffi.cdef` declared. Call them with the C types of their parameters and results. The argument `name` is one of these:

- **A path**, which is any name with a folder in it, such as `/opt/sdk/libsdk.so` or `./libsdk.dylib`. It loads as given.
- **A file name** with an extension, such as `game_platform.dll` or `libgame_platform.so`. The function looks for it in the folders below.
- **A library name** without prefix and extension, such as `native-test`. It becomes the file names of the platform: `libnative-test.dylib` and `native-test.framework/native-test` on macOS and Mac Catalyst, `native-test.framework/native-test` on iOS and tvOS, `native-test.dll` on Windows, and `libnative-test.so` on Linux and Android.

The function looks for a name in this order, and the first file that loads wins:

1. The folders that the `--native` option of the player adds. The command `haylen.py run` passes them during development.
2. The folders of the platform. That is `Contents/Frameworks` of the app bundle and then the folder of the executable on macOS and Mac Catalyst, and `Frameworks` of the app bundle on iOS and tvOS. On Windows it is the folder of the executable, and on Linux the folder of the executable and its `lib` folder. On Android it is the libraries of the APK, which the dynamic linker finds by name.
3. The libraries linked into the app, which register their symbols, as iOS and tvOS apps do with static libraries. For them `native.load` returns `ffi.C`, because their symbols are part of the app. The namespace `ffi.C` finds the registered symbols by name without the app exporting them.

Windows loads a library with its wide path. It looks for the libraries that a library depends on in its own folder and in the system folders. A library stays loaded for the rest of the process, even after the app restarts, because native code may still run from it.

The table `options` is optional:

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `init` | string | none | A function of the library that `native.load` calls with the [C interface of the engine](#the-c-interface-of-the-engine). It is declared in C as `int init(const HaylenNativeApi* api)`, and it returns `0` when it succeeds. |
| `global` | boolean | `false` | Makes the symbols of the library visible to the libraries loaded after it, which some SDKs expect from their plugins. |

These are the errors of the function:

- An empty name raises `A native library needs a name or a path.`.
- A library that does not load raises `The native library "<name>" could not be loaded. These are the places it searched:`, followed by every place the function tried and why each one failed, such as `not found` or the message of the dynamic linker. The message ends with `Put the library in one of them or load it by its path.`.
- A missing `init` function raises `The native library "<name>" has no function "<init>".`.
- An `init` function that returns anything but `0` raises `The function "<init>" of the native library "<name>" failed with code <code>.`.
- An unknown option raises `Unknown option "<key>".`.

```lua
local ffi = require('ffi')
local native = require('haylen.native')

ffi.cdef[[
    typedef struct NativeTestPoint { int32_t x; int32_t y; } NativeTestPoint;
    int32_t native_test_add(int32_t a, int32_t b);
    const char* native_test_origin(void);
    NativeTestPoint native_test_point_add(NativeTestPoint a, NativeTestPoint b);
    void native_test_fill(uint8_t* buffer, size_t size, uint8_t seed);
    uint32_t native_test_checksum(const uint8_t* buffer, size_t size);
]]

local lib = native.load('native-test')
print(lib.native_test_add(20, 22))
print(ffi.string(lib.native_test_origin()))

local sum = lib.native_test_point_add(ffi.new('NativeTestPoint', {1, 2}), ffi.new('NativeTestPoint', {x = 10, y = 20}))
print(sum.x, sum.y)

local buffer = ffi.new('uint8_t[?]', 8)
lib.native_test_fill(buffer, 8, 1)
print(lib.native_test_checksum(buffer, 8))
```

### native.findSymbol(name)

Returns the address of the symbol `name` as a light userdata, or `nil` when no library has it. The search covers the symbols of the libraries linked into the app, the libraries loaded so far in the order they loaded, and the app itself. The address goes where C takes a pointer, such as a parameter declared `void*` or a function pointer. The function `ffi.cast` turns it into any typed pointer, and a function pointer made this way is called like a function.

```lua
local ffi = require('ffi')
local native = require('haylen.native')

native.load('native-test')
local address = native.findSymbol('native_test_add')
local add = ffi.cast('int32_t (*)(int32_t, int32_t)', address)
print(address ~= nil, add(20, 22))
```

### native.callback(declaration, fn, options)

Makes a C function pointer that runs `fn` on the frame thread, and returns a [`NativeCallback`](#nativecallback) whose `pointer` goes to native code. The argument `declaration` is a C function type that returns `void`, with optional parameter names, such as `'void (int32_t code, const char* text)'`. Native code may call the pointer from any thread. The callback copies the arguments at once and returns, and `fn` receives them on the frame thread.

The table `options` is optional:

| Option | Value | Meaning |
| --- | --- | --- |
| `thread` | `'any'` | The default. Every call waits for the start of the next frame, whichever thread made it. The bridge delivers the queue of calls there. |
| `thread` | `'frame'` | A call on the frame thread runs `fn` at once, inside the native call. A call from another thread waits for the next frame. Use it for SDKs that call back while the app pumps them from Lua. |

Each parameter arrives in Lua as follows:

| Declared as | Arrives as |
| --- | --- |
| `bool`, `_Bool` | A boolean. |
| An integer type, such as `int`, `unsigned`, `long long`, `int32_t`, `uint64_t`, `size_t`, `intptr_t` or an `enum` | An integer. Unsigned 64-bit values above the largest Lua integer wrap around. |
| `float`, `double` | A number. |
| `const char*` | A copy of the text, or `nil` for a null pointer. |
| `T name[count]` | A string with the bytes of `count` elements of `T`. The `count` is a number or the name of an integer parameter of the declaration, before or after it, such as `const uint8_t data[size]`. A null pointer arrives as `nil`. |
| Any other pointer, such as `void*`, `char*` or `struct Session*` | A light userdata, or `nil` for a null pointer. |

A copied pointer outlives the native call, but the memory it points to may not. So a call that waits for the next frame reads only memory that native code keeps alive, or declares the bytes it needs as a range. A callback with `thread = 'frame'` that runs inside the native call may read what its pointers point to during `fn`.

An error raised in `fn` stops the app with the error screen, and native code goes on as if `fn` had returned. A range whose count arrives negative does not call `fn`, and it stops the app with `The native callback received the negative length <count> for "<name>".`.

A callback lives until `callback:free()`, even when Lua no longer holds it, because native code may keep its pointer. When the app stops, every callback of the app stops calling Lua. A call that arrives afterwards, even after the app restarted, does nothing. A callback that must return a value to native code at once is a Varn `ffi.cast` callback instead, as the [native code guide](../platform/native.md#callbacks) explains.

A declaration that is not a C function type returning `void`, or that has a parameter of an unknown type, a structure passed by value or a range without a count, raises an error that names the problem. The [errors](#errors) section lists them.

```lua
local ffi = require('ffi')
local native = require('haylen.native')

ffi.cdef[[
    typedef void (*NativeTestReporter)(int32_t value, const uint8_t* data, size_t size);
    typedef void (*NativeTestVisitor)(int32_t index, const char* label);
    void native_test_report_later(NativeTestReporter reporter, int32_t value);
    int32_t native_test_visit(int32_t count, NativeTestVisitor visitor);
]]
local lib = native.load('native-test')

-- The library calls it from a thread of its own, and the function runs at the start of the next frame.
local reporter = native.callback('void (int32_t value, const uint8_t data[size], size_t size)', function(value, data, size)
    print('Reported', value, #data, size)
end)
lib.native_test_report_later(reporter.pointer, 42)

-- The library calls it during native_test_visit on the frame thread, so it runs at once.
local visitor = native.callback('void (int32_t index, const char* label)', function(index, label)
    print('Visited', index, label)
end, {thread = 'frame'})
lib.native_test_visit(3, visitor.pointer)
visitor:free()
```

## NativeCallback

A `NativeCallback`, named `haylen.NativeCallback` in messages, is the C function pointer that [`native.callback`](#nativecallbackdeclaration-fn-options) returns.

### callback:free()

Releases the function pointer. Native code must no longer call it. Freeing twice does nothing.

### Properties

| Property | Type | Meaning |
| --- | --- | --- |
| `pointer` | light userdata | The C function pointer, which an `ffi` function takes for any pointer parameter, a function pointer included. Reading it after `free` raises `This "haylen.NativeCallback" was already released.`. |
| `freed` | boolean | Whether `free` was called. |

The properties are read-only.

```lua
local native = require('haylen.native')

local done = native.callback('void (int status)', function(status)
    print('Finished with', status)
end)
print(done.pointer, done.freed) -- false
done:free()
print(done.freed) -- true
```

## The C interface of the engine

The header `haylen/platform/native/HaylenNative.h` declares the C interface of the engine for native libraries, `HaylenNativeApi`. The call `native.load(name, {init = 'symbol'})` hands it to the `init` function of the library. Every entry may be called from any thread, and what it sends reaches the app on the frame thread through the bridge. Every entry copies what it takes before it returns, so a library may free its buffers, pixels and samples as soon as the call is over. The [native code guide](../platform/native.md#libraries-that-talk-to-the-app) shows when a library uses it.

| Entry | Meaning |
| --- | --- |
| `version` | The constant `HAYLEN_NATIVE_API_VERSION` of the engine, which is 5 for the interface of this page. A new version may change any entry, so a library checks that the version equals the version of the header it was built with. |
| `emit(event, payloadJson, buffers, bufferCount, flags)` | Sends an event that `platform.on` receives. The array of `bufferCount` byte buffers holds the bytes that the JSON refers to as `{"$bytes": N}`, or it is null with 0 when there are no bytes. The argument `flags` combines `HAYLEN_NATIVE_EMIT_RETAIN` and `HAYLEN_NATIVE_EMIT_BATCHED`, which send the event [retained or batched](platform.md#platformonevent-listener-options). |
| `resolve(call, ok, resultJson, buffers, bufferCount)` | Answers a call once, with a JSON result and the byte buffers it refers to. A failure is a message string, or an object with `message`, `code` and `data`. |
| `registerHandler(method, handler, cancel, user)` | Answers `platform.call(method)` with the C function `handler(user, call, method, paramsJson, buffers, bufferCount)`. The handler runs on the frame thread with the byte buffers of the parameters, valid until it returns, and answers through `resolve`, at once or later. The function `cancel(user, call)` runs on the frame thread when the app cancels a call or its timeout passes, and it may be null. A null `handler` removes the method. Handlers belong to the process, like the library, so they keep answering after the app restarts. |
| `log(level, text)` | Writes a line to the engine log at a `HaylenNativeLogLevel`: `HAYLEN_NATIVE_LOG_DEBUG`, `HAYLEN_NATIVE_LOG_INFO`, `HAYLEN_NATIVE_LOG_WARNING` or `HAYLEN_NATIVE_LOG_ERROR`. |
| `registerPlugin(id)` | Declares the library the native part of the plugin `id`. From then on `platform.plugin(id).native` and the `native` field of `platform.plugins()` are `true`, for every app the process runs. A null or empty id is logged as an error. |
| `registerErrorHandler(handler, user)` | Calls `handler(user, reportJson)` on the frame thread with the report of every error that stops an app of the process from then on, the one that its error screen shows. The report is the JSON text of `{message, file, line, traceback, frames}`, the same report that the native parts of [plugins](../platform/plugins.md#errors-of-the-app) receive. Registering the same handler with the same `user` again changes nothing, so an `init` function that runs again after a restart registers it once. A null `handler` is logged as an error. |
| `openVideoStream(plugin, name, format, width, height)` | Returns the [video stream](platform.md#videostream) `name` of the plugin `plugin`, which `platform.plugin(plugin):videoStream(name)` returns in Lua. The first call opens it with a `HaylenNativePixelFormat`, `HAYLEN_NATIVE_PIXELS_RGBA8` or `HAYLEN_NATIVE_PIXELS_BGRA8`, and a size, which may be 0 by 0 until the first frame. A later call returns the same stream. It returns null and logs why for an empty id or name, an unknown format, a negative size or a stream that is open with another format. |
| `pushVideoFrame(stream, pixels, width, height, stride, timestamp)` | Copies a frame of `width` by `height` pixels whose rows start `stride` bytes apart, with its timestamp in seconds. The stream keeps only the newest frame, which the app shows from its next frame. A frame of another size resizes the stream. |
| `openAudioStream(plugin, name, sampleRate, channels, format, capacityFrames)` | Returns the [audio stream](platform.md#audiostream) `name` of the plugin `plugin`, which `platform.plugin(plugin):audioStream(name)` returns in Lua. The first call opens it with its sample rate, its channels, a `HaylenNativeSampleFormat`, `HAYLEN_NATIVE_SAMPLES_FLOAT32` or `HAYLEN_NATIVE_SAMPLES_INT16`, and a ring of `capacityFrames` frames. It returns null and logs why for an empty id or name, for a format, rate, channel count or capacity it cannot take, or for a stream that is open with another rate, channel count or format. |
| `pushAudioFrames(stream, samples, frames)` | Writes `frames` interleaved frames in the format of the stream and returns how many fit into the ring. It drops the rest while the ring is full. One thread at a time pushes into a stream. |
| `registerScreen(plugin, name, open, cancel, user)` | Opens the [screen](platform.md#screens) `name` of the plugin `plugin` with the C function `open(user, screen, paramsJson, buffers, bufferCount)`, whenever an app of the process asks for it with `platform.plugin(plugin):openScreen(name)`. The function `open` runs on the frame thread once the engine covered the app, with the byte buffers of the parameters, valid until it returns. The library ends the screen through `finishScreen`. The function `cancel(user, screen)` runs on the frame thread when the app cancels the screen or its timeout passes, and it may be null. A null `open` removes the screen, and a null or empty plugin or name is logged as an error. Screens of libraries open before the screens of the platform. |
| `finishScreen(screen, ok, resultJson, buffers, bufferCount)` | Ends a screen once, from any thread, with a JSON result and the byte buffers it refers to, or with a failure. A failure is a message string, or an object with `message`, `code` and `data`, such as the code `cancelled` when the person closed the screen. The engine uncovers the app and answers the call of the screen. When the app restarted meanwhile, it hands the end to the next app as `screenRestored`. A screen that the app gave up still ends here once its window is gone. |
| `getWindow(window)` | Fills a `HaylenNativeWindow` with the window of the app and returns 1. It returns 0 before the window opens and where apps have no desktop window. On macOS `handle` is the `NSWindow*`, read back with `(__bridge NSWindow*)handle`. On Windows `handle` is the `HWND`. On Linux `handle` is the X11 `Window`, read back with `(Window)(uintptr_t)handle`, and `display` is the `Display*` of the engine. That display belongs to the frame thread, so a library that runs a window on a thread of its own opens a connection of its own, where the same id names the same window. The field `display` is null elsewhere. |
| `coverApp()`, `uncoverApp()` | Cover the app while native UI of the library covers it, which makes the app inactive, halted and muted, and end the cover. Covers nest and belong to the process, so they outlive the apps that restart under them. An `uncoverApp` without a `coverApp` is logged as an error. |

Library handlers answer after the handlers registered in the engine and before the handlers of the platform. Streams and screens belong to the process. So the handles of streams stay valid for good, a library keeps them across the apps that the process runs, and a screen that shows while the app restarts ends in the next app. The `init` function runs every time the app loads the library with `init`, so it registers its handlers again after a restart.

```c
#include "haylen/platform/native/HaylenNative.h"

static const HaylenNativeApi* engine = 0;

/* Answers with the parameters it received and the bytes they refer to, which `resolve` copies. */
static void answer(void* user, uint64_t call, const char* method, const char* paramsJson, const HaylenNativeBuffer* buffers, size_t bufferCount) {
    engine->resolve(call, 1, paramsJson, buffers, bufferCount);
}

static void appFailed(void* user, const char* reportJson) {
    engine->log(HAYLEN_NATIVE_LOG_INFO, reportJson);
}

int my_library_haylen_init(const HaylenNativeApi* api) {
    if (api->version != HAYLEN_NATIVE_API_VERSION) {
        return 1;
    }
    engine = api;
    api->registerHandler("my-library.echo", answer, 0, 0);
    api->registerErrorHandler(appFailed, 0);
    api->emit("my-library.ready", "{\"version\": 1}", 0, 0, HAYLEN_NATIVE_EMIT_RETAIN);
    return 0;
}
```

```lua
local async = require('async')
local native = require('haylen.native')
local platform = require('haylen.platform')

native.load('my-library', {init = 'my_library_haylen_init'})

-- The library sent ready retained, so this listener receives it although it connects after the load.
platform.on('my-library.ready', function(payload)
    print('Ready', payload.version)
end)

async.spawn(function()
    local echoed = platform.call('my-library.echo', {word = 'hello', data = platform.bytes('\0\1\2')}):await()
    print(echoed.word, #echoed.data)
end)
```

A library that feeds a stream opens it once and pushes from any thread, such as the thread of a capture device:

```c
#include "haylen/platform/native/HaylenNative.h"

static const HaylenNativeApi* engine = 0;
static HaylenNativeVideoStream* preview = 0;
static HaylenNativeAudioStream* microphone = 0;

/* Called by the capture device of the library for every frame, on its own thread. */
static void frameCaptured(const uint8_t* bgra, int width, int height, int stride, double seconds) {
    engine->pushVideoFrame(preview, bgra, width, height, stride, seconds);
}

/* Called by the audio device of the library for every block of 16-bit mono samples, on its own thread. */
static void samplesCaptured(const int16_t* samples, size_t frames) {
    engine->pushAudioFrames(microphone, samples, frames);
}

int capture_haylen_init(const HaylenNativeApi* api) {
    if (api->version != HAYLEN_NATIVE_API_VERSION) {
        return 1;
    }
    engine = api;
    preview = api->openVideoStream("capture", "preview", HAYLEN_NATIVE_PIXELS_BGRA8, 1280, 720);
    microphone = api->openAudioStream("capture", "microphone", 48000, 1, HAYLEN_NATIVE_SAMPLES_INT16, 24000);
    api->registerPlugin("capture");
    return preview != 0 && microphone != 0 ? 0 : 2;
}
```

## Errors

These messages are raised by the functions and methods of the module.

| Message | Cause |
| --- | --- |
| `Native libraries are not available in the browser. Call JavaScript through "haylen.platform" instead.` | The function `native.load` ran in the browser build. |
| `Native callbacks are not available in the browser. Call JavaScript through "haylen.platform" instead.` | The function `native.callback` ran in the browser build. |
| `A native library needs a name or a path.` | The name of `native.load` is empty. |
| `The native library "<name>" could not be loaded. These are the places it searched: <places> Put the library in one of them or load it by its path.` | No place of the search held a library that loads. Each place comes on its own line with the reason it failed. |
| `The native library "<name>" has no function "<init>".` | The library lacks the `init` function that the options name. |
| `The function "<init>" of the native library "<name>" failed with code <code>.` | The `init` function returned a code other than `0`. |
| `The thread of a native callback is "any" or "frame", not "<thread>".` | The option `thread` of `native.callback` has another value. |
| `A native callback returns nothing, so its declaration is "void" and the parameters, such as "void (int code, const char* text)".` | The declaration returns another type. |
| `The native callback declaration has the unknown type "<type>".` | A parameter has a type that the declaration does not know. |
| `<parameter> of the native callback declaration is not a C number type, and a callback reads other values only through pointers.` | A parameter is a structure or another value passed by value. |
| `<parameter> of the native callback declaration needs a count or the name of a length parameter between "[" and "]".` | A range has no count. |
| `<parameter> is a range of elements whose size the native callback declaration does not know. Use "uint8_t" or another C number type.` | A range has elements of `void` or of an opaque type. |
| `The length "<length>" of the native callback parameter "<name>" must name an integer parameter.` | A range names a length that is not an integer parameter. |
| `The native callback received the negative length <count> for "<name>".` | Native code passed a negative count for a range. This stops the app with the error screen instead of calling the function. |
| `This "haylen.NativeCallback" was already released.` | The code read `pointer` of a callback after `free`. |
| `Unknown option "<key>".` | An options table has a key that the function does not take. |

Other mistakes in the text of a declaration, such as a missing `)` or an unexpected character, raise a message that starts with `The native callback declaration` and names what is wrong.
