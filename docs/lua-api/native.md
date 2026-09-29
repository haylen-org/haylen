# haylen.native

`haylen.native` loads native libraries for the `ffi` module of Varn, finds their symbols and creates callbacks that native code may call from any thread. Varn's `ffi` declares the C functions and types and calls them, and this module finds the file of a library where the app ships it, hands the library the interface of the engine, and brings the calls of native code back to the frame thread. The [native code guide](../native.md) explains when to use it, how to package libraries with an app and how to integrate SDKs with a C API.

```lua
local ffi = require('ffi')
local native = require('haylen.native')
```

The browser loads no native libraries. There `native.available()` returns `false`, `native.load` and `native.callback` raise `Native libraries are not available in the browser. Call JavaScript through haylen.platform instead.`, and `native.symbol` returns `nil`.

## Functions

### native.available()

Returns `true` where the platform loads native libraries, which is every platform but the browser.

```lua
local native = require('haylen.native')

if not native.available() then
    print('this build calls JavaScript through haylen.platform instead')
end
```

### native.load(name, options)

Loads a library and returns the namespace of Varn's `ffi` whose fields are the functions that `ffi.cdef` declared, called with the C types of their parameters and results. `name` is one of:

- A path, which is any name with a folder in it, such as `/opt/sdk/libsdk.so` or `./libsdk.dylib`. It loads as given.
- A file name with an extension, such as `steam_api64.dll` or `libsteam_api.so`, which is looked up in the folders below.
- A library name without prefix and extension, such as `native_test`, which becomes the file names of the platform: `libnative_test.dylib` and `native_test.framework/native_test` on macOS and Mac Catalyst, `native_test.framework/native_test` on iOS and tvOS, `native_test.dll` on Windows, and `libnative_test.so` on Linux and Android.

A name is looked up in this order, and the first file that loads wins:

1. The folders that the `--native` option of the player adds, which `make.py run` passes during development.
2. The folders of the platform: `Contents/Frameworks` of the app bundle and then the folder of the executable on macOS and Mac Catalyst, `Frameworks` of the app bundle on iOS and tvOS, the folder of the executable on Windows, the folder of the executable and its `lib` folder on Linux, and the libraries of the APK, which the dynamic linker finds by name, on Android.
3. The libraries linked into the app, which register their symbols, as iOS and tvOS apps do with static libraries. `native.load` returns `ffi.C` for them, because their symbols are part of the app.

Windows loads a library with its wide path and looks for the libraries it depends on in its own folder and the system folders. A library stays loaded for the rest of the process, even after the app restarts, because native code may still run from it. A library that does not load raises an error that lists every place the lookup tried and why each one failed, such as `not found` or the message of the dynamic linker.

`options` is an optional table:

| Option | Type | Meaning |
| --- | --- | --- |
| `init` | string | A function of the library that `native.load` calls with the `HaylenNativeApi` of the engine, declared in C as `int init(const HaylenNativeApi* api)`. A result other than 0 raises `The function <init> of the native library <name> failed with code <code>.`, and a missing function raises `The native library <name> has no function <init>.`. See [library handlers](#library-handlers). |
| `global` | boolean | Makes the symbols of the library visible to libraries loaded after it, which some SDKs expect from their plugins. It is `false` by default. |

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

local lib = native.load('native_test')
print(lib.native_test_add(20, 22))
print(ffi.string(lib.native_test_origin()))

local sum = lib.native_test_point_add(ffi.new('NativeTestPoint', {1, 2}), ffi.new('NativeTestPoint', {x = 10, y = 20}))
print(sum.x, sum.y)

local buffer = ffi.new('uint8_t[?]', 8)
lib.native_test_fill(buffer, 8, 1)
print(lib.native_test_checksum(ffi.cast('const uint8_t*', buffer), 8))
```

### native.symbol(name)

Returns the address of the symbol `name` as a light userdata, or `nil`. The lookup covers the symbols of the libraries linked into the app, the libraries loaded so far in load order, and the app itself. The address goes where C takes a pointer, such as a parameter declared `void*` or a function pointer, and `ffi.cast` turns it into a typed pointer. Varn's `ffi` calls only the functions of a namespace, so a function is called through `native.load` rather than through its address.

```lua
local ffi = require('ffi')
local native = require('haylen.native')

native.load('native_test')
local address = native.symbol('native_test_add')
print(address ~= nil, ffi.cast('void*', address))
```

### native.callback(declaration, fn, options)

Creates a C function pointer that runs `fn` on the frame thread, and returns a [callback](#callbacks) whose `pointer` goes to native code. `declaration` is a C function type that returns `void`, with optional parameter names, such as `'void (int32_t code, const char* text)'`. Native code may call the pointer from any thread: the callback copies the arguments at once and returns, and `fn` receives them on the frame thread. `options` is an optional table:

| Option | Value | Meaning |
| --- | --- | --- |
| `thread` | `'any'` | The default. Every call waits for the start of the next frame, where the bridge delivers its queue, whichever thread made it. |
| `thread` | `'frame'` | A call on the frame thread runs `fn` at once, inside the native call, and a call from another thread waits for the next frame. It suits SDKs that call back while the app pumps them from Lua. |

Each parameter arrives in Lua as follows:

| Declared as | Arrives as |
| --- | --- |
| `bool`, `_Bool` | A boolean. |
| An integer type, such as `int`, `unsigned`, `long long`, `int32_t`, `uint64_t`, `size_t`, `intptr_t` or an `enum` | An integer. Unsigned 64-bit values above the largest Lua integer wrap around. |
| `float`, `double` | A number. |
| `const char*` | A copy of the text, or `nil` for a null pointer. |
| `T name[count]` | A string with the bytes of `count` elements of `T`, where `count` is a number or the name of an integer parameter of the declaration, before or after it, such as `const uint8_t data[size]`. A null pointer arrives as `nil`, and a negative count reports an error instead of calling `fn`. |
| Any other pointer, such as `void*`, `char*` or `struct Session*` | A light userdata, or `nil` for a null pointer. |

A copied pointer outlives the native call, while the memory it points to may not, so a call that waits for the next frame reads memory that native code keeps alive, or declares the bytes it needs as a range. A callback with `thread = 'frame'` that runs inside the native call may read what the pointers point to during `fn`. An error raised in `fn` stops the app with the error screen, and native code continues as if `fn` had returned. A declaration that is not a C function type returning `void`, or that has a parameter of an unknown type, a structure passed by value or a range without a count, raises an error that names the problem.

A callback lives until `callback:free()`, even when Lua no longer holds it, because native code may keep its pointer. When the app stops, every callback of the app stops calling Lua: a call that arrives afterwards, even after the app restarted, does nothing. Callbacks that must return a value to native code at once are Varn `ffi.cast` callbacks, which run during the call on the thread that makes it, as the [native code guide](../native.md#callbacks) explains.

```lua
local ffi = require('ffi')
local native = require('haylen.native')

ffi.cdef[[
    typedef void (*NativeTestReporter)(int32_t value, const uint8_t* data, size_t size);
    typedef void (*NativeTestVisitor)(int32_t index, const char* label);
    void native_test_report_later(NativeTestReporter reporter, int32_t value);
    int32_t native_test_visit(int32_t count, NativeTestVisitor visitor);
]]
local lib = native.load('native_test')

-- The library calls it from a thread of its own, and the function runs at the start of the next frame.
local reporter = native.callback('void (int32_t value, const uint8_t data[size], size_t size)', function(value, data, size)
    print('reported', value, #data, size)
end)
lib.native_test_report_later(reporter.pointer, 42)

-- The library calls it during native_test_visit on the frame thread, so it runs at once.
local visitor = native.callback('void (int32_t index, const char* label)', function(index, label)
    print('visited', index, label)
end, {thread = 'frame'})
lib.native_test_visit(3, visitor.pointer)
visitor:free()
```

## Callbacks

`native.callback` returns a `haylen.NativeCallback`.

| Member | Meaning |
| --- | --- |
| `callback.pointer` | The C function pointer as a light userdata, which an ffi function takes for any pointer parameter, including a function pointer. Reading it after `free` raises `haylen.NativeCallback was already released.`. |
| `callback.freed` | Whether `free` was called. |
| `callback:free()` | Releases the function pointer, which native code must no longer call. Freeing twice does nothing. |

```lua
local native = require('haylen.native')

local done = native.callback('void (int status)', function(status)
    print('finished with', status)
end)
print(done.pointer, done.freed)
done:free()
print(done.freed)
```

## Library handlers

`haylen/platform/native/HaylenNative.h` declares the C interface of the engine for native libraries, `HaylenNativeApi`, which `native.load(name, {init = 'symbol'})` hands to the init function of the library. Every entry may be called from any thread, and what it sends reaches the app on the frame thread through the bridge.

| Entry | Meaning |
| --- | --- |
| `version` | `HAYLEN_NATIVE_API_VERSION` of the engine, which grows when entries are added at the end. |
| `emit(event, payloadJson)` | Sends an event that `platform.on` receives. |
| `resolve(call, ok, resultJson)` | Answers a call once, with a JSON result or a failure that is a message string or an object with `message`, `code` and `data`. |
| `registerHandler(method, handler, cancel, user)` | Answers `platform.call(method)` with the C function `handler(user, call, method, paramsJson)`, which runs on the frame thread and answers through `resolve`, at once or later. `cancel(user, call)` runs on the frame thread when the app cancels a call or its timeout passes, and may be null. A null `handler` removes the method. Handlers belong to the process, like the library, so they keep answering after the app restarts. |
| `log(level, text)` | Writes a line to the engine log at a `HaylenNativeLogLevel`. |

Library handlers answer after the handlers registered in the engine and before the handlers of the platform. The init function runs every time the app loads the library with `init`, so it registers its handlers again after a restart.

```c
#include "haylen/platform/native/HaylenNative.h"

static const HaylenNativeApi* engine = 0;

static void answer(void* user, uint64_t call, const char* method, const char* paramsJson) {
    engine->resolve(call, 1, paramsJson);
}

int my_library_haylen_init(const HaylenNativeApi* api) {
    engine = api;
    api->registerHandler("my_library.echo", answer, 0, 0);
    api->emit("my_library.ready", "{\"version\": 1}");
    return 0;
}
```

```lua
local async = require('async')
local native = require('haylen.native')
local platform = require('haylen.platform')

platform.on('my_library.ready', function(payload)
    print('ready', payload.version)
end)
native.load('my_library', {init = 'my_library_haylen_init'})

async.spawn(function()
    print(platform.call('my_library.echo', {word = 'hello'}):await().word)
end)
```
