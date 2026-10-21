# 🔌 Native code

An app reaches anything native, from the APIs of the platform to native libraries and SDKs with a C API, in three ways. This guide explains them, how to choose between them, how native libraries are called from Lua and how their callbacks reach it, how an app ships its libraries on every platform, and how to integrate SDKs with a C API, such as a game platform SDK with a flat C API or an online services SDK with peer-to-peer connections through NAT. The [`haylen.native` reference](../lua-api/native.md) and the [`haylen.platform` reference](../lua-api/platform.md) list every function, and the [platform bridge guide](platform-bridge.md) describes how bridge calls travel.

The Native libraries category of the test project, `NAT-001` to `NAT-005` in `samples/tests/source/categories/native/`, runs every part of this guide against the test library of the engine, `engine/tests/native/NativeTest.c`, on every platform, with a list of checks that pass or fail. The `native` section of `samples/tests/app.json` builds the library from `samples/tests/plugins/native-test/native/`.

## The three ways

| Way | What it is | Where it runs |
| --- | --- | --- |
| The platform bridge, `haylen.platform` | JSON calls and events with byte buffers next to them, answered by Java or Kotlin on Android, Objective-C or Swift on Apple platforms, JavaScript on the web, C in native libraries, or C++ and Lua anywhere. Answers are asynchronous, typed errors carry a code and data, calls time out and cancel, and the native parts of plugins feed video and audio streams. | Every platform. |
| Native libraries through FFI, `haylen.native` and Varn's `ffi` | Lua declares the C functions and types of a library and calls them directly, with numbers, text, structs, pointers, buffers and callbacks. | macOS, Windows, Linux, iOS, tvOS, Mac Catalyst and Android. Not the browser. |
| C++ plugins | A `haylen::plugins::Plugin` of an app that compiles the engine, with the lifecycle of the engine and a Lua module of its own. | Apps built with `haylen_add_app`, on every platform. |

## Choosing a way

- Platform APIs that live in Java, Kotlin, Objective-C, Swift or JavaScript, such as sign-in, purchases, sharing or permissions, go through the bridge. Their handlers are written in the language of the platform, in a [plugin](plugins.md) of the app or in its platform projects in `platform/<template>/`.
- A library or SDK with a C API that ships as a `.dll`, `.so`, `.dylib`, framework or static library goes through FFI. Lua calls it directly, with no glue code to compile, and the calls cost what a C call costs plus the conversion of the arguments.
- A library with a C++ API, one that needs many calls per frame from native code, or one whose data never needs to reach Lua, goes into a C++ plugin of an app that compiles the engine, or into a small C library with a flat API that FFI calls.
- A native library that answers calls or sends events on its own threads registers bridge handlers through the `HaylenNativeApi` of the engine, which works the same on every platform that loads native libraries.
- The browser has no native libraries, so web builds reach JavaScript through the bridge. An app that calls a library through FFI elsewhere answers the same methods with a page handler on the web, as the local plugin `native-test` of the test project does.

## Calling a library

The function `native.load` finds the file of a library where the app ships it and hands it to Varn's `ffi`, which calls the functions that `ffi.cdef` declares.

```lua
local ffi = require('ffi')
local native = require('haylen.native')

ffi.cdef[[
    typedef struct NativeTestPoint { int32_t x; int32_t y; } NativeTestPoint;
    typedef struct NativeTestRect { NativeTestPoint origin; float width; float height; } NativeTestRect;
    int32_t native_test_add(int32_t a, int32_t b);
    double native_test_scale(double value, double factor);
    const char* native_test_origin(void);
    void native_test_rect_grow(NativeTestRect* rect, float amount);
    void native_test_fill(uint8_t* buffer, size_t size, uint8_t seed);
    uint32_t native_test_checksum(const uint8_t* buffer, size_t size);
]]

local lib = native.load('native-test')
print(lib.native_test_add(20, 22), lib.native_test_scale(1.5, 4), ffi.string(lib.native_test_origin()))

local rect = ffi.new('NativeTestRect', {origin = {5, 5}, width = 10, height = 4})
lib.native_test_rect_grow(rect, 2)
print(rect.origin.x, rect.width)

local buffer = ffi.new('uint8_t[?]', 8)
lib.native_test_fill(buffer, 8, 250)
print(ffi.string(buffer, 8):byte(1, -1))
print(lib.native_test_checksum(buffer, 8))
```

The rules of Varn's `ffi` that matter most:

- The function `ffi.cdef` declares functions, structs, unions, enums, bitfields, arrays, pointers, function pointer types and typedefs once per Lua state, and declaring a name twice raises an error, so declarations belong in a module that `require` loads once.
- Numbers convert both ways, `const char*` parameters take Lua strings, `ffi.string(pointer[, length])` copies text or bytes out, `ffi.new('T[?]', n)` allocates a buffer that Lua owns, and `ffi.new('T', table)` fills a struct from a table by field name or by position, nested structs included.
- A struct passes by value as a cdata of its type and by pointer as the same cdata. An array field, such as `char name[33]`, reads and writes its elements in place and travels with its struct when the struct passes by value.
- A `T*` and a `T[n]` pass where C expects a `const T*`, so a buffer from `ffi.new` goes to a `const uint8_t*` parameter as it is. A `const T*` does not pass where C expects a `T*` unless `ffi.cast` drops the `const`, and a `void*` takes and gives any pointer.
- The constants of an `enum` are fields of `ffi.C`, such as `ffi.C.SDK_SUCCESS`, and an enum has the size of an `int`. A bitfield has the layout the C compiler of the platform gives it, and `intptr_t` and `uintptr_t` are integers as wide as a pointer. A function takes at most 30 arguments and a struct has at most 30 fields.
- A function pointer is called like a function, whether a function returned it, a struct holds it or `ffi.cast` made it from an address, such as the light userdata of `native.findSymbol`, and calling a null one raises an error.
- Variadic functions are called with Lua numbers, strings and pointers after the fixed arguments.
- A value that does not fit a parameter or a field raises an error that names both types, such as `The C type "const int *" does not convert to "int *".`.

The function `native.findSymbol(name)` returns the address of a symbol of the linked libraries, the loaded ones or the app, as a light userdata that goes wherever C takes a pointer and that `ffi.cast` turns into any typed pointer, a function pointer included.

## Callbacks

Native code calls back into Lua through a function pointer. There are two kinds:

| Kind | Created with | Runs | Returns a value | Safe from other threads |
| --- | --- | --- | --- | --- |
| Engine callback | `native.callback(declaration, fn, {thread = 'any' or 'frame'})` | On the frame thread: at the start of the next frame, or inside the native call when `thread = 'frame'` and the call comes from the frame thread. | No, its declaration returns `void`. | Yes. The arguments are copied and the call returns at once. |
| Varn callback | `ffi.cast('void (*)(int)', fn)` | At once, on the main Lua state, when native code calls it on the frame thread. | Yes. | No. A call from another thread returns zero without running `fn`, and the app stops with the error screen. |

Engine callbacks are the safe default. A library that calls from a thread of its own, such as a network SDK, a media decoder or a file watcher, gets an engine callback: the call copies numbers, booleans, pointers, text and byte ranges that the declaration bounds, queues them and returns at once, and the Lua function receives them at the start of the next frame, next to the answers of the bridge. When the app stops, its callbacks stop reaching Lua, so a call that arrives later, even after a restart, does nothing and never touches a Lua state that is gone. A callback lives until `callback:free()`, because native code may keep its pointer.

```lua
local reporter = native.callback('void (int32_t value, const uint8_t data[size], size_t size)', function(value, data, size)
    print(value, #data)
end)
lib.native_test_report_later(reporter.pointer, 42)
```

Varn callbacks fit SDKs that call back only on the thread that pumps them, such as the run or tick function of a game platform SDK called from Lua, and callbacks that must return a value to native code, such as a comparison function for a sort. They run on the main Lua state, never on the coroutine that called `ffi.cast`, so they keep working after it ended. An error raised during an `ffi` call, such as the sort that calls the comparison, is raised by that call once it returns, with the traceback of the callback after its message, and an error raised outside any `ffi` call stops the app with the error screen, like the error of an `async` task. A callback that fails returns zero to native code. An engine callback with `thread = 'frame'` runs inside the native call too, and it is the better fit for pumped SDKs whose callbacks return nothing, because it also takes calls from other threads.

## Threads

- Lua, and every callback that reaches it, runs on the frame thread.
- Engine callbacks, `HaylenNativeApi` entries and bridge replies may come from any thread, and they reach Lua at the start of a frame, before the app updates, in the order they arrived.
- A native library calls `ffi` callbacks only on the frame thread, since a call from its own threads returns zero without running Lua and stops the app, and it does not keep pointers to Lua memory, such as a Lua string or an `ffi.new` buffer, longer than Lua keeps the value.
- Pumped SDKs are pumped from the frame thread, typically from the `update` of a scene or of an autoload, so their callbacks run there.
- Blocking calls into a library stall the frame, so long work runs on a thread of the library, which reports back through an engine callback or an event.

## Libraries that talk to the app

A library written for Haylen receives the C interface of the engine, `HaylenNativeApi` from `haylen/platform/native/HaylenNative.h`, when the app loads it with `native.load(name, {init = 'my_library_haylen_init'})`. With it the library registers bridge handlers in C, answers them from any thread with JSON and byte buffers, sends events, retained or batched, feeds the video and audio streams of a plugin, opens the screens of a plugin over the window of the app, covers the app, writes to the engine log and hears the errors that stop the app, the same way on every platform. The [reference](../lua-api/native.md#library-handlers) lists the entries.

```c
#include "haylen/platform/native/HaylenNative.h"

static const HaylenNativeApi* engine = 0;

static void download(void* user, uint64_t call, const char* method, const char* paramsJson, const HaylenNativeBuffer* buffers, size_t bufferCount) {
    /* Start the work on a thread of the library, which calls `engine->resolve(call, 1, "{\"file\": {\"$bytes\": 0}}", &file, 1)` with the downloaded bytes when it is done. */
}

static void stop(void* user, uint64_t call) {
    /* The app cancelled the call or its timeout passed, so the work may stop. */
}

int my_library_haylen_init(const HaylenNativeApi* api) {
    if (api->version != HAYLEN_NATIVE_API_VERSION) {
        return 1;
    }
    engine = api;
    api->registerHandler("my_library.download", download, stop, 0);
    return 0;
}
```

This is also the way Windows and Linux apps, which have no native handler registry of their own, answer bridge methods with native code without compiling the engine.

## Packaging libraries with an app

The `native` section of `app.json` lists the libraries an app ships, by the name it loads them with. A library is either prebuilt files per platform or a CMake project that `haylen.py` builds for each platform it lists. A name starts with a letter and holds letters, digits, dashes and underscores: a library that the app builds takes dashes, such as `native-test`, a prebuilt library keeps the name of its files, such as `game_platform`, and the `init` function of a library takes underscores, such as `native_test_haylen_init`, because a C function is named with an identifier.

```json
{
    "native": {
        "game_platform": {
            "files": {
                "macos": "platform/apple/native/libgame_platform.dylib",
                "windows": "platform/windows/game_platform.dll",
                "linux": "platform/linux/libgame_platform.so"
            }
        },
        "native-test": {
            "cmake": "native",
            "platforms": ["macos", "ios", "tvos", "android", "windows", "linux"]
        },
        "native-test-static": {
            "cmake": "native",
            "platforms": ["ios", "tvos"],
            "link": "static",
            "symbols": ["native_test_add", "native_test_origin"]
        }
    }
}
```

| Key | Meaning |
| --- | --- |
| `files` | The prebuilt file of each platform, relative to the app folder: a `.dylib`, `.framework` or `.xcframework` on macOS, a `.framework`, `.xcframework` or, when linked statically, a `.a` on iOS and tvOS, a `.dll` on Windows, a `.so` on Linux, and on Android a folder with a subfolder of `.so` files for each ABI, the layout of `jniLibs`. An `.xcframework` gives the slice of each Apple platform. |
| `cmake` | A folder with a `CMakeLists.txt` that defines a library target with the name of the library, built as a shared or a static library as `BUILD_SHARED_LIBS` says. The script `haylen.py` passes `HAYLEN_INCLUDE_DIR`, the folder of `haylen/platform/native/HaylenNative.h`. |
| `platforms` | The platforms `haylen.py` builds a CMake library for, among `macos`, `ios`, `tvos`, `android`, `windows` and `linux`. |
| `link` | Either `dynamic`, the default, or `static`, which only iOS and tvOS apps do, Mac Catalyst included. |
| `symbols` | The symbols of a static library that Lua reaches, which `haylen.py` keeps and registers. |

The script `haylen.py` places each library where the app loads it:

| Platform | Place |
| --- | --- |
| macOS and Mac Catalyst | The folder `Contents/Frameworks` of the app bundle, copied and signed by the `Embed native libraries` phase of `App.xcodeproj` with the identity of the app. The executable has `@executable_path/../Frameworks` in its runpath. |
| iOS and tvOS, dynamic | A framework in `Frameworks` of the app bundle, embedded and signed the same way. A CMake library becomes a framework whose bundle identifier is the one of the app followed by `.native.<name>`. |
| iOS, Mac Catalyst and tvOS, static | Linked into the app through `HAYLEN_NATIVE_LDFLAGS` in `haylen/Haylen.xcconfig` of the Apple project, with `haylen/HaylenNativeSymbols.mm` generated next to it. On Mac Catalyst the setting holds only for the build of the iOS target whose `IS_MACCATALYST` is `YES`, so the macOS target, which builds with the same SDK, never links the Mac Catalyst copy. |
| Android | The folder `haylen/jniLibs/<abi>/` of the Gradle project for `arm64-v8a`, `armeabi-v7a` and `x86_64`, the ABIs of the engine, which the app module takes as a folder of native libraries. The linker of Android finds them by name. An AAR dependency or a `jniLibs` folder of the Android project of the app works too. |
| Windows | Next to the executable. |
| Linux | The folder `lib/` next to the executable, whose `RUNPATH` of `$ORIGIN:$ORIGIN/lib` lets the libraries find each other. |
| Desktop player | The folder `build/apps/<app>-<hash>/native/development/`, in the build folder of the app, which `haylen.py run` passes to the player with `--native`. |

The file `project.yml` of the app never changes for its libraries: `haylen.py` writes them into `haylen/native/<platform>-<sdk>/` of the Apple project, the file lists that the embed phase reads into `haylen/native/<platform>-<sdk>.xcfilelist`, the link settings into `haylen/Haylen.xcconfig`, and the embed phase into the target templates of the platforms that have libraries. The embed phase runs without the script sandbox of Xcode, because the sandbox would need every file of a bundle and the temporary files of `codesign` listed one by one. Files that the app keeps in `platform/<platform>/` for Windows and Linux land next to the executable too.

### Static libraries on iOS and tvOS

An iOS or tvOS app, and the Mac Catalyst build of the iOS app, may link a library statically instead of embedding a framework, which some SDKs require. Dead code stripping would then remove every function the app never calls from native code, so `haylen.py` writes `haylen/HaylenNativeSymbols.mm`, whose `+load` registers the listed symbols with `haylen::platform::NativeLibraries::registerLinked`. The references keep the functions in the app, and every Lua state the engine creates gets the table through `varn::runtime::Runtime::addSymbol`, so `native.load('native-test-static')` returns `ffi.C`, which finds the listed functions by name without the app exporting them, and `native.findSymbol` finds them too. The `symbols` list names the functions Lua calls, including an `init` function.

### Development

The command `haylen run <app>` builds or copies the libraries of this desktop into `build/apps/<app>-<hash>/native/development/`, in the [build folder of the app](../tools/distribution.md#platform-projects), and starts the player with `--native` and that folder, which `native.load` searches first. The player takes `--native <folder>` more than once, so a player started by hand finds libraries anywhere.

## C++ plugins

An app that compiles the engine with `haylen_add_app` extends it with plugins of its own, which have the same lifecycle as the plugins of the engine: `start`, `fixedUpdate`, `update`, `render`, `renderUi`, `stop` and the lifecycle events, all on the frame thread. The method `installLua` installs a Lua module with the binding toolkit of `haylen/lua/`. A plugin added before the engine starts, or while it runs, gets `start` and `installLua` like a built-in one. The [embedding guide](../tools/embedding.md#plugins-of-an-app) shows a complete plugin.

```cpp
class GamePlatformPlugin final : public haylen::plugins::Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "gamePlatform";
    }

    void update(haylen::core::Engine&, float) override {
        gp_run_callbacks();
    }

    void installLua(haylen::core::Engine&, lua_State* L) override {
        haylen::lua::Binding::preload(L, "game.platform", &GamePlatformPlugin::open);
    }

  private:
    static int open(lua_State* L);
};
```

## Integrating SDKs with a C API

SDKs with a C API work through FFI without glue code, and the SDKs themselves never enter the Haylen repository: each app ships the files its license allows with its `native` section. The steps are the same for every SDK:

1. Declare the functions, structs, enums and constants the app uses with `ffi.cdef`, copied from the headers of the SDK version the app ships. The constants of enums are fields of `ffi.C`, and handles become `void*` or pointers to opaque structs.
2. List the files of each platform in the `native` section, and load them with `native.load` and the file name of each platform.
3. Pump the SDK once per frame from the frame thread, in the `update` of an autoload or of the scene that owns the connection.
4. Receive its callbacks on the frame thread: engine callbacks with `thread = 'frame'` for SDKs that call back while they are pumped, engine callbacks with the default thread for SDKs that call back from their own threads, or polling for SDKs that queue their events.

### A game platform SDK with a flat C API

A game platform SDK often has a flat C API that starts with an init function and turns every callback into a message that Lua reads each frame through manual dispatch functions, so no C callback is needed at all. The names below stand for the functions of such an SDK, whose real names, versioned interface accessors and structs come from the headers of the SDK version the app ships, together with its desktop library of each platform.

```lua
local ffi = require('ffi')
local haylen = require('haylen')
local native = require('haylen.native')

ffi.cdef[[
    typedef struct { char message[1024]; } gp_error;
    typedef struct { int32_t user; int32_t id; uint8_t* data; int32_t size; } gp_message;
    int gp_init(gp_error* error);
    void gp_shutdown(void);
    int32_t gp_pipe(void);
    void gp_dispatch_run_frame(int32_t pipe);
    bool gp_dispatch_next(int32_t pipe, gp_message* message);
    void gp_dispatch_free_last(int32_t pipe);
    const char* gp_user_name(void);
]]

local files = {macos = 'libgame_platform.dylib', windows = 'game_platform.dll', linux = 'libgame_platform.so'}
local sdk = native.load(files[haylen.platform])

local failure = ffi.new('gp_error')
if sdk.gp_init(failure) ~= 0 then
    error('The game platform did not start: ' .. ffi.string(failure.message))
end
local pipe = sdk.gp_pipe()
print('Playing as ' .. ffi.string(sdk.gp_user_name()))

-- Called once per frame from an update. Each message is a callback of the SDK, whose id and parameter struct come from its headers.
local message = ffi.new('gp_message')
local function pump()
    sdk.gp_dispatch_run_frame(pipe)
    while sdk.gp_dispatch_next(pipe, message) do
        print('Callback ' .. message.id .. ' with ' .. message.size .. ' bytes')
        sdk.gp_dispatch_free_last(pipe)
    end
end
```

A parameter struct of a callback is read with `ffi.cast('const some_callback*', message.data)` before the message is freed, after declaring the struct with the packing its header uses. Files that such an SDK reads next to the executable during development stay in `platform/windows/` and `platform/linux/` of the app, which land next to the executable, and in `Contents/MacOS` of the macOS app bundle.

### An online services SDK with NAT P2P

An online services SDK with a C API is usually started by an initialize function and a platform create function, runs its work and calls its callbacks on the thread that ticks it, and gives a peer-to-peer interface that traverses NAT with relays when a direct connection fails. Its option structs often start with a version field set to the latest constant of the header, and its handles are opaque pointers. The names below stand for the functions of such an SDK.

Callbacks such as the one of an incoming connection request receive a pointer to an info struct that is valid only during the callback. Ticking the platform from Lua on the frame thread with an engine callback of `thread = 'frame'` runs the Lua function inside the tick, where the pointer is still valid.

```lua
local ffi = require('ffi')
local native = require('haylen.native')

ffi.cdef[[
    typedef struct os_platform* os_platform_handle;
    typedef struct os_p2p* os_p2p_handle;
    typedef struct os_user* os_user_id;
    typedef struct os_socket_id { int32_t version; char name[33]; } os_socket_id;
    typedef struct os_request_options { int32_t version; os_user_id local_user; const os_socket_id* socket; } os_request_options;
    typedef struct os_request_info { void* client_data; os_user_id local_user; os_user_id remote_user; const os_socket_id* socket; } os_request_info;
    typedef void (*os_request_callback)(const os_request_info* info);
    void os_platform_tick(os_platform_handle platform);
    os_p2p_handle os_platform_p2p(os_platform_handle platform);
    uint64_t os_p2p_notify_requests(os_p2p_handle p2p, const os_request_options* options, void* client_data, os_request_callback callback);
]]

local sdk = native.load('online_services')

-- The platform handle and the local user come from the create function and a login, with their option structs declared the same way.
local function listen(platform, localUser)
    local socket = ffi.new('os_socket_id', {version = 1, name = 'game'})
    local options = ffi.new('os_request_options', {version = 1, local_user = localUser, socket = socket})
    local requests = native.callback('void (const struct os_request_info* info)', function(info)
        local request = ffi.cast('const os_request_info*', info)
        print('Connection request from', request.remote_user)
    end, {thread = 'frame'})
    sdk.os_p2p_notify_requests(sdk.os_platform_p2p(platform), options, nil, requests.pointer)
    return requests, socket
end

-- Called once per frame from an update, so the callbacks of the tick run on the frame thread.
local function pump(platform)
    sdk.os_platform_tick(platform)
end
```

Accepting a connection, sending packets and reading them after each tick follow the same pattern, with buffers from `ffi.new('uint8_t[?]', size)`. The version values come from the header of the SDK version the app ships, and Android and iOS need the platform options that the SDK documents for them.

### Other SDKs

- An SDK that calls back from its own threads gets engine callbacks with the default thread, and declares the data it needs as byte ranges or copies it before the callback returns.
- An SDK with a C++ API gets a small C wrapper library with a flat API, built by the `cmake` entry of the `native` section, or a C++ plugin in an app that compiles the engine.
- An SDK that needs Java, Kotlin, Objective-C or Swift for part of its work, such as sign-in on a phone, answers those parts through a [plugin](plugins.md) of the app, while its C API goes through FFI.

## The web

The browser runs no native code, so `native.available()` is `false` there and the app reaches the page through the bridge instead. JavaScript handlers, registered with `Module.haylen.register` in `platform/web/app.js` of the app or with `context.register` of the web module of a plugin, answer the same methods that native code answers elsewhere, with typed errors, `AbortSignal` cancellation and events, as the [platform reference](../lua-api/platform.md#web-handlers) shows.
