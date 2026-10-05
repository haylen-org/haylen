# Native code

An app reaches anything native, from the APIs of the platform to native libraries and SDKs with a C API, in three ways. This guide explains them, how to choose between them, how native libraries are called from Lua and how their callbacks reach it, how an app ships its libraries on every platform, and how to integrate SDKs such as the flat C API of Steam or the C API of Epic Online Services with its NAT P2P. The [`haylen.native` reference](lua-api/native.md) and the [`haylen.platform` reference](lua-api/platform.md) list every function, and the [platform bridge guide](platform_bridge.md) describes how bridge calls travel.

The sample `samples/system/native` runs every part of this guide against the test library of the engine, `engine/tests/native/NativeTest.c`, on every platform, with a list of checks that pass or fail.

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
- The browser has no native libraries, so web builds reach JavaScript through the bridge. An app that calls a library through FFI elsewhere answers the same methods with a page handler on the web, as the local plugin `native-test` of the native sample does.

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

local lib = native.load('native_test')
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
- The constants of an `enum` are fields of `ffi.C`, such as `ffi.C.EOS_Success`, and an enum has the size of an `int`. A bitfield has the layout the C compiler of the platform gives it, and `intptr_t` and `uintptr_t` are integers as wide as a pointer. A function takes at most 30 arguments and a struct has at most 30 fields.
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

Varn callbacks fit SDKs that call back only on the thread that pumps them, such as `SteamAPI_RunCallbacks` or `EOS_Platform_Tick` called from Lua, and callbacks that must return a value to native code, such as a comparison function for a sort. They run on the main Lua state, never on the coroutine that called `ffi.cast`, so they keep working after it ended. An error raised during an `ffi` call, such as the sort that calls the comparison, is raised by that call once it returns, with the traceback of the callback after its message, and an error raised outside any `ffi` call stops the app with the error screen, like the error of an `async` task. A callback that fails returns zero to native code. An engine callback with `thread = 'frame'` runs inside the native call too, and it is the better fit for pumped SDKs whose callbacks return nothing, because it also takes calls from other threads.

## Threads

- Lua, and every callback that reaches it, runs on the frame thread.
- Engine callbacks, `HaylenNativeApi` entries and bridge replies may come from any thread, and they reach Lua at the start of a frame, before the app updates, in the order they arrived.
- A native library calls `ffi` callbacks only on the frame thread, since a call from its own threads returns zero without running Lua and stops the app, and it does not keep pointers to Lua memory, such as a Lua string or an `ffi.new` buffer, longer than Lua keeps the value.
- Pumped SDKs are pumped from the frame thread, typically from the `update` of a scene or of an autoload, so their callbacks run there.
- Blocking calls into a library stall the frame, so long work runs on a thread of the library, which reports back through an engine callback or an event.

## Libraries that talk to the app

A library written for Haylen receives the C interface of the engine, `HaylenNativeApi` from `haylen/platform/native/HaylenNative.h`, when the app loads it with `native.load(name, {init = 'my_library_haylen_init'})`. With it the library registers bridge handlers in C, answers them from any thread with JSON and byte buffers, sends events, retained or batched, feeds the video and audio streams of a plugin, opens the screens of a plugin over the window of the app, covers the app, writes to the engine log and hears the errors that stop the app, the same way on every platform. The [reference](lua-api/native.md#library-handlers) lists the entries.

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

The `native` section of `app.json` lists the libraries an app ships, by the name it loads them with. A library is either prebuilt files per platform or a CMake project that `haylen.py` builds for each platform it lists.

```json
{
    "native": {
        "steam_api": {
            "files": {
                "macos": "platform/apple/native/libsteam_api.dylib",
                "windows": "platform/windows/steam_api64.dll",
                "linux": "platform/linux/libsteam_api.so"
            }
        },
        "native_test": {
            "cmake": "native",
            "platforms": ["macos", "ios", "tvos", "android", "windows", "linux"]
        },
        "native_test_static": {
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
| `link` | Either `dynamic`, the default, or `static`, which only iOS and tvOS apps do. |
| `symbols` | The symbols of a static library that Lua reaches, which `haylen.py` keeps and registers. |

The script `haylen.py` places each library where the app loads it:

| Platform | Place |
| --- | --- |
| macOS and Mac Catalyst | The folder `Contents/Frameworks` of the app bundle, copied and signed by the `Embed native libraries` phase of `App.xcodeproj` with the identity of the app. The executable has `@executable_path/../Frameworks` in its runpath. |
| iOS and tvOS, dynamic | A framework in `Frameworks` of the app bundle, embedded and signed the same way. A CMake library becomes a framework whose bundle identifier is the one of the app followed by `.native.<name>`. |
| iOS and tvOS, static | Linked into the app through `HAYLEN_NATIVE_LDFLAGS` in `haylen/Haylen.xcconfig` of the Apple project, with `haylen/HaylenNativeSymbols.mm` generated next to it. |
| Android | The folder `haylen/jniLibs/<abi>/` of the Gradle project for `arm64-v8a`, `armeabi-v7a` and `x86_64`, the ABIs of the engine, which the app module takes as a folder of native libraries. The linker of Android finds them by name. An AAR dependency or a `jniLibs` folder of the Android project of the app works too. |
| Windows | Next to the executable. |
| Linux | The folder `lib/` next to the executable, whose `RUNPATH` of `$ORIGIN:$ORIGIN/lib` lets the libraries find each other. |
| Desktop player | The folder `build/apps/<app>-<hash>/native/development/`, in the build folder of the app, which `haylen.py run` passes to the player with `--native`. |

The file `project.yml` of the app never changes for its libraries: `haylen.py` writes them into `haylen/native/<platform>-<sdk>/` of the Apple project, the file lists that the embed phase reads into `haylen/native/<platform>-<sdk>.xcfilelist`, the link settings into `haylen/Haylen.xcconfig`, and the embed phase into the target templates of the platforms that have libraries. The embed phase runs without the script sandbox of Xcode, because the sandbox would need every file of a bundle and the temporary files of `codesign` listed one by one. Files that the app keeps in `platform/<platform>/` for Windows and Linux land next to the executable too.

### Static libraries on iOS and tvOS

An iOS app may link a library statically instead of embedding a framework, which some SDKs require. Dead code stripping would then remove every function the app never calls from native code, so `haylen.py` writes `source/HaylenNativeSymbols.mm`, whose `+load` registers the listed symbols with `haylen::platform::NativeLibraries::registerLinked`. The references keep the functions in the app, and every Lua state the engine creates gets the table through `varn::runtime::Runtime::addSymbol`, so `native.load('native_test_static')` returns `ffi.C`, which finds the listed functions by name without the app exporting them, and `native.findSymbol` finds them too. The `symbols` list names the functions Lua calls, including an `init` function.

### Development

The command `python3 haylen.py run <app>` builds or copies the libraries of this desktop into `build/apps/<app>-<hash>/native/development/`, in the [build folder of the app](distribution.md#platform-projects), and starts the player with `--native` and that folder, which `native.load` searches first. The player takes `--native <folder>` more than once, so a player started by hand finds libraries anywhere.

## C++ plugins

An app that compiles the engine with `haylen_add_app` extends it with plugins of its own, which have the same lifecycle as the plugins of the engine: `start`, `fixedUpdate`, `update`, `render`, `renderUi`, `stop` and the lifecycle events, all on the frame thread. The method `installLua` installs a Lua module with the binding toolkit of `haylen/lua/`. A plugin added before the engine starts, or while it runs, gets `start` and `installLua` like a built-in one. The [embedding guide](embedding.md#plugins-of-an-app) shows a complete plugin.

```cpp
class SteamPlugin final : public haylen::plugins::Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "steam";
    }

    void update(haylen::core::Engine&, float) override {
        SteamAPI_RunCallbacks();
    }

    void installLua(haylen::core::Engine&, lua_State* L) override {
        haylen::lua::Binding::preload(L, "game.steam", &SteamPlugin::open);
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

### Steam

The flat API of Steamworks, `steam_api_flat.h`, is plain C. The function `SteamAPI_InitFlat` starts it, and the manual dispatch functions turn every callback into a message that Lua reads each frame, so no C callback is needed at all. The names of the interface accessors, such as `SteamAPI_SteamFriends_v017`, carry the version of the interface and change with the SDK, so they come from the headers of the SDK version the app ships. The desktop library is `libsteam_api.dylib` on macOS, `steam_api64.dll` on 64-bit Windows and `libsteam_api.so` on Linux.

```lua
local ffi = require('ffi')
local haylen = require('haylen')
local native = require('haylen.native')

ffi.cdef[[
    typedef struct { char message[1024]; } SteamErrMsg;
    typedef struct CallbackMsg_t { int32_t m_hSteamUser; int m_iCallback; uint8_t* m_pubParam; int m_cubParam; } CallbackMsg_t;
    int SteamAPI_InitFlat(SteamErrMsg* message);
    void SteamAPI_Shutdown(void);
    int32_t SteamAPI_GetHSteamPipe(void);
    void SteamAPI_ManualDispatch_Init(void);
    void SteamAPI_ManualDispatch_RunFrame(int32_t pipe);
    bool SteamAPI_ManualDispatch_GetNextCallback(int32_t pipe, CallbackMsg_t* message);
    void SteamAPI_ManualDispatch_FreeLastCallback(int32_t pipe);
    void* SteamAPI_SteamFriends_v017(void);
    const char* SteamAPI_ISteamFriends_GetPersonaName(void* friends);
]]

local files = {macos = 'libsteam_api.dylib', windows = 'steam_api64.dll', linux = 'libsteam_api.so'}
local steam = native.load(files[haylen.platform])

local failure = ffi.new('SteamErrMsg')
if steam.SteamAPI_InitFlat(failure) ~= 0 then
    error('Steam did not start: ' .. ffi.string(failure.message))
end
steam.SteamAPI_ManualDispatch_Init()
local pipe = steam.SteamAPI_GetHSteamPipe()
print('playing as ' .. ffi.string(steam.SteamAPI_ISteamFriends_GetPersonaName(steam.SteamAPI_SteamFriends_v017())))

-- Called once per frame from an update. Each message is a callback of Steam, whose id and parameter struct come from the headers.
local message = ffi.new('CallbackMsg_t')
local function pump()
    steam.SteamAPI_ManualDispatch_RunFrame(pipe)
    while steam.SteamAPI_ManualDispatch_GetNextCallback(pipe, message) do
        print('Steam callback ' .. message.m_iCallback .. ' with ' .. message.m_cubParam .. ' bytes')
        steam.SteamAPI_ManualDispatch_FreeLastCallback(pipe)
    end
end
```

A parameter struct of a callback is read with `ffi.cast('const SomeCallback_t*', message.m_pubParam)` before `FreeLastCallback`, after declaring the struct with the packing its header uses. The Steam client reads the app id from `steam_appid.txt` next to the executable during development, which the Windows and Linux folders of the app keep in `platform/windows/` and `platform/linux/`, and from the macOS app bundle in `Contents/MacOS`.

### Epic Online Services and NAT P2P

The Epic Online Services SDK is a C API. The functions `EOS_Initialize` and `EOS_Platform_Create` start it, `EOS_Platform_Tick` runs its work and calls its callbacks on the thread that ticks it, and `EOS_Platform_GetP2PInterface` gives the peer-to-peer interface, which traverses NAT with relays when a direct connection fails. Its option structs start with an `ApiVersion` field set to the `*_API_LATEST` constant of the header, and its handles are opaque pointers. The library is `EOSSDK-Win64-Shipping.dll` on Windows, `libEOSSDK-Mac-Shipping.dylib` on macOS, `libEOSSDK-Linux-Shipping.so` on Linux, `libEOSSDK.so` on Android and `EOSSDK.framework` on iOS.

Callbacks such as `EOS_P2P_OnIncomingConnectionRequestCallback` receive a pointer to an info struct that is valid only during the callback. Ticking the platform from Lua on the frame thread with an engine callback of `thread = 'frame'` runs the Lua function inside the tick, where the pointer is still valid.

```lua
local ffi = require('ffi')
local native = require('haylen.native')

ffi.cdef[[
    typedef struct EOS_PlatformHandle* EOS_HPlatform;
    typedef struct EOS_P2PHandle* EOS_HP2P;
    typedef struct EOS_ProductUserIdDetails* EOS_ProductUserId;
    typedef struct EOS_P2P_SocketId { int32_t ApiVersion; char SocketName[33]; } EOS_P2P_SocketId;
    typedef struct EOS_P2P_AddNotifyPeerConnectionRequestOptions { int32_t ApiVersion; EOS_ProductUserId LocalUserId; const EOS_P2P_SocketId* SocketId; } EOS_P2P_AddNotifyPeerConnectionRequestOptions;
    typedef struct EOS_P2P_OnIncomingConnectionRequestInfo { void* ClientData; EOS_ProductUserId LocalUserId; EOS_ProductUserId RemoteUserId; const EOS_P2P_SocketId* SocketId; } EOS_P2P_OnIncomingConnectionRequestInfo;
    typedef void (*EOS_P2P_OnIncomingConnectionRequestCallback)(const EOS_P2P_OnIncomingConnectionRequestInfo* Data);
    void EOS_Platform_Tick(EOS_HPlatform platform);
    EOS_HP2P EOS_Platform_GetP2PInterface(EOS_HPlatform platform);
    uint64_t EOS_P2P_AddNotifyPeerConnectionRequest(EOS_HP2P p2p, const EOS_P2P_AddNotifyPeerConnectionRequestOptions* options, void* clientData, EOS_P2P_OnIncomingConnectionRequestCallback callback);
]]

local eos = native.load('EOSSDK-Win64-Shipping')

-- The platform handle and the local user come from `EOS_Platform_Create` and a login, with their option structs declared the same way.
local function listen(platform, localUser)
    local socket = ffi.new('EOS_P2P_SocketId', {ApiVersion = 1, SocketName = 'game'})
    local options = ffi.new('EOS_P2P_AddNotifyPeerConnectionRequestOptions', {ApiVersion = 1, LocalUserId = localUser, SocketId = socket})
    local requests = native.callback('void (const struct EOS_P2P_OnIncomingConnectionRequestInfo* data)', function(data)
        local info = ffi.cast('const EOS_P2P_OnIncomingConnectionRequestInfo*', data)
        print('connection request from', info.RemoteUserId)
    end, {thread = 'frame'})
    eos.EOS_P2P_AddNotifyPeerConnectionRequest(eos.EOS_Platform_GetP2PInterface(platform), options, nil, requests.pointer)
    return requests, socket
end

-- Called once per frame from an update, so the callbacks of the tick run on the frame thread.
local function pump(platform)
    eos.EOS_Platform_Tick(platform)
end
```

Accepting a connection, sending packets with `EOS_P2P_SendPacket` and reading them with `EOS_P2P_GetNextReceivedPacketSize` and `EOS_P2P_ReceivePacket` after each tick follow the same pattern, with buffers from `ffi.new('uint8_t[?]', size)`. The `ApiVersion` values come from the header of the SDK version the app ships, and Android and iOS need the platform options that the SDK documents for them.

### Other SDKs

- An SDK that calls back from its own threads gets engine callbacks with the default thread, and declares the data it needs as byte ranges or copies it before the callback returns.
- An SDK with a C++ API gets a small C wrapper library with a flat API, built by the `cmake` entry of the `native` section, or a C++ plugin in an app that compiles the engine.
- An SDK that needs Java, Kotlin, Objective-C or Swift for part of its work, such as sign-in on a phone, answers those parts through a [plugin](plugins.md) of the app, while its C API goes through FFI.

## The web

The browser runs no native code, so `native.available()` is `false` there and the app reaches the page through the bridge instead. JavaScript handlers, registered with `Module.haylen.register` in `platform/web/app.js` of the app or with `context.register` of the web module of a plugin, answer the same methods that native code answers elsewhere, with typed errors, `AbortSignal` cancellation and events, as the [platform reference](lua-api/platform.md#web-handlers) shows.
