# Platform bridge

The platform bridge connects an app to native code. An app calls a named method with JSON parameters and receives its JSON result asynchronously, or a typed error with a message, a code and data, and native code sends named events with JSON payloads that the app listens to. Calls time out and cancel, and the native handler hears about it. Every result and every event reaches the app on the frame thread, at the start of a frame. The bridge covers everything the engine does not wrap itself, such as sign-in, purchases, sharing, deep links or system settings, with handlers written in Java or Kotlin on Android, Objective-C or Swift on Apple platforms, JavaScript on the web, C in native libraries, and C++ or Lua anywhere.

This guide explains how the bridge works and how each platform implements methods. The Lua functions are documented in the [haylen.platform reference](lua-api/platform.md), the [native code guide](native.md) compares the bridge with native libraries called through FFI and with C++ plugins, and the [architecture guide](architecture.md) places the bridge among the other engine systems.

## How a call travels

```text
Lua platform.call ─► Bridge::call ─► engine handler (C++ or Lua), answered at once
                                  ├► NativeApi::dispatch ─► handler of a native library
                                  └► Host::dispatchPlatformCall ─► Services::dispatch ─► platform handler
native reply (any thread) ─► BridgeRelay::resolve ─► Bridge::resolve ─► queue
next frame: Engine::frame ─► Bridge::pump ─► callback or Lua call, on the frame thread
```

1. `platform.call(method, params, options)` reaches `platform::Bridge::call` (`engine/src/platform/Bridge.cpp`), which gives the call a numeric id, keeps its callback and, with a timeout, its deadline.
2. A handler registered inside the engine answers first. Those are C++ handlers added with `Bridge::registerHandler`, Lua handlers added with `platform.registerHandler`, and the built-in `engine.info` and `app.version`. The handler runs during the call itself.
3. Otherwise the bridge serializes the parameters and hands `(id, method, paramsJson)` to the dispatcher of the engine. A handler that a native library registered through `HaylenNativeApi` (`engine/src/platform/native/NativeApi.cpp`) answers next, and the host gets the call otherwise, through `Host::dispatchPlatformCall`. The Sokol runtime forwards it to `platform::Services::dispatch`, which each platform folder under `engine/src/platform` implements.
4. Native code answers once, from any thread, with success and a JSON value or with failure and a message, a code and data. The answer goes through `platform::BridgeRelay::resolve` in `engine/src/platform/BridgeRelay.cpp` to the bridge of the running engine, which the platform plugin attaches when the app starts, and the bridge parses it and queues it.
5. At the start of every frame, `Engine::frame` polls the Varn event loop and calls `Bridge::pump`, which runs the queued callbacks, events and the work native callbacks posted, on the frame thread before the fixed update and the update. In Lua, the call returned by `platform.call` settles there.

Native events take the same road: `BridgeRelay::emit(event, payloadJson, retain)` queues the event, and `pump` delivers it to every listener connected with `on`. An event with invalid JSON is logged and dropped. Events that arrive while no app is running are dropped too, and so are events that nothing listens to, unless they are retained.

## Retained events

Some events come before the app listens: the deep link or the notification that opened the app, a purchase that finished while it was closed, or the result of a sign-in that restores itself at launch. Native code sends them retained. A retained event that nothing listens to waits in the bridge, up to `Bridge::kRetainedLimit`, 32, per name with the oldest dropped first, and the first `on` of its name receives the waiting events in order at the next pump, before any newer event of that name. The events wait in the bridge of the running app, so a restarted app starts with none, and an event that arrives while no app runs is dropped even when it is retained. Every entry point takes the flag: `Bridge::emit` and `BridgeRelay::emit` in C++, `emit` of `HaylenNativeApi` in native libraries, `Module.haylen.emit(event, payload, {retain: true})` and `context.emit` of plugin modules on the web, and `platform.emit(event, payload, {retain = true})` in Lua tests.

## Timeouts and cancellation

A call with a `timeout` fails with the code `timeout` at the first pump after its deadline, and `call:cancel()` fails a pending call with the code `cancelled` at the next pump. In both cases the bridge tells the native side that the app gave the call up, through `Bridge::Canceller`: a handler of a native library hears it through its `HaylenNativeCancel` function, and a platform handler through `Host::cancelPlatformCall` and `Services::cancel`, which reach `HaylenBridge.cancel` on Android, the cancel block of a cancellable handler on Apple platforms and the `AbortSignal` of the handler on the web. The desktop handlers answer during the call, so no call of theirs is ever pending. An answer that comes after a call timed out or was cancelled is dropped, like a second answer to the same call. A call without a timeout waits for its answer as long as the app runs, so every handler answers exactly once.

## The JSON contract

- Parameters are a JSON value, an empty object when Lua passes none. Lua tables become JSON as the [reference](lua-api/platform.md#how-calls-travel) describes.
- A successful result is any JSON value, and `null` reaches Lua as `nil`.
- A failure carries either a JSON string, which becomes the error message, or an object with a `message` string and optional `code` and `data` values, which Lua receives as the fields of the error. Any other failure payload, such as `null`, a number or an object without a string `message`, fails with `The native platform call failed without a message.` and keeps the code and data of an object, and text that is not JSON fails with the code `invalidJson` and `The platform returned invalid JSON.`
- The engine and the platform sides fail calls with these codes: `timeout` and `cancelled` from the bridge, `noHandler` when nothing answers the method, `invalidJson`, and `exception` when a Java, Kotlin, Swift or JavaScript handler threw an error without a code of its own instead of answering, with the class of the exception or the type of the error in `data.type`.
- Method and event names are free-form strings. The built-in methods use dotted names such as `device.info` and `system.openUrl`, and the Tiny Island sample uses `auth.google.signIn`. [Plugins](plugins.md) put their id in front of their method and event names, with the names in camelCase, such as `admob.showBanner` and `admob.closed`: the plugin handle of Lua and the plugin contexts of native code and the web add the prefix, so neither side writes it.

## Built-in methods

These methods work without app code. The results are documented in the [reference](lua-api/platform.md#built-in-methods).

| Method | Answered by |
| --- | --- |
| `engine.info` | The engine on every platform. |
| `app.version` | The engine on every platform, with the `version` of `app.json`. |
| `device.info` | `HaylenBridge` on Android and Apple platforms, the page on the web, and the desktop handlers on Windows and Linux. |
| `system.locale` | Same as `device.info`. |
| `system.openUrl` | Same as `device.info`. |
| `haptics.vibrate` | Same as `device.info`. Android and the web vibrate, iOS plays an impact, and the other platforms answer without doing anything. |

An app can replace any of them. Engine handlers always take precedence over native ones. A native handler registered under the name of a built-in replaces it on every platform, whenever the app registers it: Android registers its built-ins when the `HaylenBridge` class loads, before any app code can register, the built-ins of Apple platforms never replace a handler of the same name, and the web runtime registers its built-ins before the page code runs.

## The Lua side

| Function | Purpose |
| --- | --- |
| `platform.call(method, params, options)` | Calls a method and returns a call. `call:await()` inside a coroutine returns the result, or `nil` and the error, a table with `message`, `code` and `data` that reads as its message. `options.timeout` gives up after that many seconds, and `call:cancel()` gives up at once. |
| `platform.send(method, params)` | Calls a method whose answer nobody needs. It creates no call and drops the answer. |
| `platform.on(event, listener)` | Calls `listener(payload)` for every native event of that name and returns a connection with `disconnect()`. The first listener of a name also receives the retained events that wait for it. |
| `platform.plugin(id)` | Returns the handle of a plugin of the app, whose `call`, `send` and `on` put the id of the plugin in front of the name. |
| `platform.registerHandler(method, handler)` | Answers a method with a Lua function inside the engine, which takes precedence over native handlers. |
| `platform.hasHandler(method)` | Returns whether an engine handler answers the method. Native handlers are invisible to it. |

Because `hasHandler` cannot see native handlers, an app that provides Lua stand-ins for methods implemented natively on some platforms decides by `haylen.platform`, as the [complete example](#complete-example) does. Registering a stand-in unconditionally would hide the native handler on every platform.

## Android

The engine's Android library module `engine/platform/android/haylen` holds `dev.haylen.HaylenActivity`, the native activity that runs the app, and `dev.haylen.HaylenBridge`, the handler registry:

| Member | Meaning |
| --- | --- |
| `HaylenBridge.register(String method, MethodHandler handler)` | Adds or replaces the handler of a method. |
| `HaylenBridge.unregister(String method)` | Removes a handler. |
| `HaylenBridge.emit(String event, Object payload)` | Sends an event to the app. |
| `HaylenBridge.activity()` | The running activity, or `null` when there is none. |
| `MethodHandler.handle(Object params, Reply reply) throws Exception` | The handler interface, usable as a lambda. |
| `Reply.success(Object value)` | Answers the call. |
| `Reply.failure(String message)`, `Reply.failure(String message, String code, Object data)`, `Reply.failure(Throwable error)` | Fail the call, with a code and data, or with a thrown error: a `HaylenBridge.Failure` keeps its code and data, and any other error fails with the code `exception`. |
| `Reply.isCancelled()`, `Reply.onCancel(Runnable listener)` | Whether the app gave the call up, and a listener that runs on the main thread when it does. |
| `HaylenBridge.Failure(String message, String code, Object data)` | An exception that a handler throws to fail its call with a code and data. |
| `HaylenCoroutines.register(String method, suspend (Any?) -> Any?)` | Adds a Kotlin handler written as a suspending function, which runs in a coroutine on the main thread and is cancelled when the app gives the call up. |

How a call runs:

1. The engine calls the static `HaylenBridge.dispatch(long, byte[], byte[])` through JNI on the frame thread, with the method name and the parameters as UTF-8 bytes. Every text crosses JNI as UTF-8 bytes in both directions, because the JNI string functions use a modified UTF-8 that breaks characters outside the Basic Multilingual Plane, such as emoji. A method without a handler fails with the code `noHandler` and `No native handler is registered for <method>.`.
2. The parameters are parsed with `org.json.JSONTokener`, so an object arrives as a `JSONObject`. Parameters that do not parse fail the call.
3. The handler runs on the main thread. When the activity was destroyed in the meantime, the call fails with `The activity was destroyed before <method> ran.`
4. The handler replies at once or later, from any thread. `success` accepts `null`, strings, numbers, booleans, `JSONObject`, `JSONArray`, maps, collections and arrays, converted with `JSONObject.wrap`. The first answer counts, and later ones are dropped.
5. When the app cancels the call or its timeout passes, the engine calls the static `HaylenBridge.cancel(long)` through JNI, which marks the reply cancelled, drops the answer that may still come and runs the `onCancel` listeners on the main thread. A call cancelled before its handler ran never runs it.

A handler that throws, a checked exception included, fails its call through `reply.failure(Throwable)` instead of crashing the app, so an unexpected `NullPointerException` becomes a failed call with the code `exception` and the class name in `data.type`. Errors of the virtual machine, such as `OutOfMemoryError`, still end the app. `HaylenBridge.emit` calls into the app's native library, which `HaylenActivity` loads when it is created, so native code emits events only while an activity exists.

`HaylenCoroutines` in `engine/platform/android/haylen/src/main/kotlin/dev/haylen/HaylenCoroutines.kt` registers Kotlin handlers written as suspending functions. It launches each call in a coroutine of a scope on `Dispatchers.Main.immediate`, answers with the return value, fails the call with what the function throws, and cancels the coroutine from `onCancel`. The library depends on `kotlinx-coroutines-android` as an API, so apps get it with the library. The [reference](lua-api/platform.md#android-handlers) has examples in Java and Kotlin.

Apps register their handlers once, in `Application.onCreate`, before the first activity starts. Tiny Island does this in `samples/games/tiny-island/platform/android/app/src/main/java/dev/haylen/tinyisland/TinyIslandApplication.java`, which its `app/app.gradle` names through the `haylenApplication` manifest placeholder of the [Android template](distribution.md#platform-overrides). Its `GoogleSignInPlugin` answers `auth.google.signIn` with Credential Manager: it builds a `GetGoogleIdOption` with the web client id of the game's Google Cloud project, which the build takes from the `googleServerClientId` Gradle property into the `google_server_client_id` string resource, calls `getCredentialAsync` on the activity's main executor and replies with `idToken`, `email`, `name` and `picture`. Without a client id it fails with a message that explains the build property. R8 keeps the classes the engine reaches by name through the module's `consumer-rules.pro`, so handlers need no rules of their own.

## Apple platforms

`engine/include/haylen/platform/apple/HaylenBridge.h` declares the registry for macOS, iOS and tvOS, and `engine/src/platform/apple/HaylenBridge.mm` implements it on top of `AppleBridge`, the handler table of `engine/src/platform/apple/AppleBridge.mm`:

```objc
typedef void (^HaylenReply)(BOOL ok, id _Nullable result);
typedef void (^HaylenHandler)(id params, HaylenReply reply);
typedef void (^HaylenCancel)(void);
typedef HaylenCancel _Nullable (^HaylenCancellableHandler)(id params, HaylenReply reply);

@interface HaylenBridge : NSObject
+ (void)registerHandler:(NSString*)method handler:(HaylenHandler)handler;
+ (void)registerCancellableHandler:(NSString*)method handler:(HaylenCancellableHandler)handler;
+ (void)removeHandler:(NSString*)method;
+ (void)emit:(NSString*)event payload:(nullable id)payload;
@end
```

- The parameters are parsed with `NSJSONSerialization`, so an object arrives as an `NSDictionary`, and parameters that do not parse arrive as an empty dictionary.
- Handlers run on the main queue and may reply later from any thread. `reply(YES, result)` succeeds with any value `NSJSONSerialization` accepts, or `nil`. `reply(NO, failure)` fails with a message string or a dictionary with a `message` string and optional `code` and `data`. The first answer counts, and later ones are dropped.
- A cancellable handler returns a block, or `nil`, that runs on the main queue when the app cancels the call or its timeout passes, after which an answer is dropped. A call cancelled before its handler ran never runs it.
- A method without a handler fails with the code `noHandler` and `No native handler is registered for <method>.`

Swift handlers use `HaylenBridge.register(method) { (params: Params) async throws -> Result in ... }` from `source/HaylenBridgeAsync.swift` of the Apple template, which decodes the parameters with `JSONDecoder`, encodes the result with `JSONEncoder`, fails the call with the code and data of a thrown `HaylenFailure` or with the code `exception` for any other error, and cancels the task of the call when the app gives it up. The template ships it as a source file, because the engine artifact is a static library of C, C++ and Objective-C whose headers Swift reaches through `source/HaylenBridging.h`, and a Swift module in the artifact would have to match the Swift compiler of each app. The target names its module `HaylenApp`, so Objective-C++ code such as `main.mm` calls Swift classes through `#import "HaylenApp-Swift.h"`.

The handler table exists from the first registration, and the built-in methods that the runtime registers when it starts never replace a handler of the same name, so native code may register at any time. Apps made from the Apple template register in the `main` function of `source/main.mm`, before it calls `haylen_main`, with their Objective-C, C++ and Swift files next to it in `platform/apple/source/` of the app, whose files all build into every target, as the [distribution guide](distribution.md#platform-overrides) describes. Apps built with `haylen_add_app` add the Objective-C file with its `SOURCES` argument, compile it with `-fobjc-arc` as the engine compiles its Apple sources, and register from `+load` or once the app has launched. The [reference](lua-api/platform.md#apple-handlers) has a complete handler.

## Web

The page side of the bridge lives in `engine/platform/web/haylen-runtime.js`, which every web target links with `--pre-js` and which fills `Module.haylen`:

| Function | Meaning |
| --- | --- |
| `Module.haylen.register(method, handler)` | Adds or replaces the handler of a method. |
| `Module.haylen.unregister(method)` | Removes a handler. |
| `Module.haylen.emit(event, payload, options)` | Sends an event to the app. The payload is converted with `JSON.stringify`, and `options.retain` keeps the event for the first listener of its name. Events sent before the first app starts reach it once it starts. |
| `Module.haylen.createPluginContext(id, config)` | Makes the context that the web module of a plugin receives, whose `register` and `emit` put the id of the plugin in front of the name, as the [plugin guide](plugins.md#web-modules) describes. |

The runtime calls the handler after the frame that made the call, with the parsed parameters and a context with the `call` id and a `signal`, an `AbortSignal` that aborts when the app cancels the call or its timeout passes. A call cancelled in the frame that made it never runs its handler. The handler returns the result or a promise for it, and a thrown error or a rejected promise fails the call with the error message and the `code` and `data` properties of the error, or with the code `exception` and the `name` of the error in `data.type` when the error has no code. The first answer counts, and an answer after a cancel is dropped. A method without a handler fails with the code `noHandler` and `No page handler is registered for <method>.` The page answers `device.info`, `system.locale`, `system.openUrl` and `haptics.vibrate` with its own handlers, which `register` can replace.

The web build is single-threaded, so handlers run on the browser's main thread between frames. An asynchronous handler, such as one that waits for `fetch` or for a sign-in popup, never blocks the app while it waits.

`register` exists once the runtime script has run, so a page registers its handlers in `Module.preRun`. The web template runs the `app.js` of an app for that, and Tiny Island's `samples/games/tiny-island/platform/web/app.js` registers `auth.google.signIn` there. It loads Google Identity Services only when the player first signs in, reads the client id from a constant at the top of the file, and resolves with `idToken`, `email`, `name` and `picture` decoded from the returned credential. The Google script needs `make.py run --platform web --coep off`, as the [distribution guide](distribution.md#serve) explains.

## Desktop

macOS uses the Apple registry above. Windows and Linux have no registry in the language of the platform: `DesktopMethods` in `engine/src/platform/desktop/DesktopMethods.cpp` answers the four built-in methods with the answers of its `WindowsMethods` and `LinuxMethods` subclasses, synchronously on the frame thread, and fails every other method with the code `noHandler`. Their answers still reach the app at the start of the next frame. `system.openUrl` uses `ShellExecuteW` on Windows and runs `xdg-open` on Linux, where a helper thread waits for it to exit so the frame never blocks. Apps add methods on these platforms with handlers of native libraries, which a Lua app loads without compiling the engine, or with C++ or Lua handlers.

## Native library handlers

A native library answers methods in C on every platform that loads native libraries. `native.load(name, {init = 'symbol'})` hands its init function the `HaylenNativeApi` of `haylen/platform/native/HaylenNative.h`, whose `registerHandler` adds a handler with an optional cancel function, `resolve` answers a call from any thread, `emit` sends an event from any thread, retained or not, `log` writes to the engine log and `registerPlugin` declares the library the native part of a plugin. The handlers belong to the process and answer after the engine handlers and before the platform handlers. The [native code guide](native.md#libraries-that-talk-to-the-app) shows a library, and the [reference](lua-api/native.md#library-handlers) lists the entries.

## C++ handlers and calls

`haylen::platform::Bridge` in `engine/include/haylen/platform/Bridge.hpp` is the whole bridge, and `engine.getPlatform()` returns the running one:

| Member | Meaning |
| --- | --- |
| `registerHandler(std::string method, Bridge::Handler handler)` | Answers a method inside the engine. The handler receives `const Json& params` and a `Bridge::Reply`, which takes a `Bridge::Result{ok, value, error}` whose `Bridge::Error` has a `message`, a `code` and `data`. |
| `hasHandler(std::string_view method)` | Whether an engine handler answers the method. |
| `call(std::string_view method, const Json& params, Bridge::Callback callback, timeout)` | Calls a method and returns the call id. The callback receives the `Bridge::Result` on the frame thread, and an optional `std::chrono::steady_clock::duration` fails the call with the code `timeout` once it passes. |
| `cancel(std::uint64_t id)` | Fails a pending call with the code `cancelled` at the next pump and tells native code, and returns whether the call was pending. |
| `getMailbox()` | A `Bridge::Mailbox` whose `post` queues work for the frame thread from any thread, until the bridge is gone. Native callbacks deliver their calls through it. |
| `on(const std::string& event, std::function<void(const Json&)> listener)` | Listens to native events and returns a `Connection`. |
| `send(std::string_view method, const Json& params)` | Calls a method whose answer nobody needs, without a pending call. |
| `resolve(std::uint64_t id, bool ok, std::string_view resultJson)` | Answers a call. Safe from any thread. |
| `emit(std::string_view event, std::string_view payloadJson, bool retain = false)` | Sends an event, which waits for the first listener of its name when it is retained. Safe from any thread. |
| `getPendingCallCount()` | Calls still waiting for an answer. |

A plugin is the usual home for C++ handlers, because its `start` runs with every new engine:

```cpp
class CloudSavePlugin final : public haylen::plugins::Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "cloudSave";
    }

    void start(haylen::core::Engine& engine) override {
        engine.getPlatform().registerHandler("save.cloudSync", [](const haylen::core::Json& params, haylen::platform::Bridge::Reply reply) {
            reply({.ok = true, .value = {{"synced", params.value("slot", 0)}}});
        });
    }
};
```

A C++ application adds it with `engine.addPlugin(std::make_unique<CloudSavePlugin>())`, and Lua then calls `platform.call('save.cloudSync', {slot = 2})`. A handler may keep the reply and answer later from another thread, but it must answer before the engine stops, because the reply belongs to that engine's bridge. C++ code calls methods the same way Lua does:

```cpp
engine.getPlatform().call("device.info", haylen::core::Json::object(), [](haylen::platform::Bridge::Result result) {
    if (!result.ok) {
        haylen::core::Log::warning("device.info failed: {}", result.error.message);
        return;
    }
    haylen::core::Log::info("Running on {}", result.value.value("model", std::string("an unknown device")));
});
```

## Complete example

This example adds `system.theme`, which tells the app whether the device uses a dark theme, and a `system.themeChanged` event, implemented natively on Android and on the web. The app matches its UI theme to the answer and uses a Lua stand-in on the other platforms.

### Android

`app/src/main/java/com/example/myapp/ThemePlugin.java`:

```java
package com.example.myapp;

import android.content.res.Configuration;
import dev.haylen.HaylenBridge;
import java.util.Collections;

// Tells the app whether the device uses a dark theme, and when that changes.
final class ThemePlugin {
    private ThemePlugin() {}

    static void register() {
        HaylenBridge.register("system.theme", (params, reply) -> {
            boolean dark = isDark(HaylenBridge.activity().getResources().getConfiguration());
            reply.success(Collections.singletonMap("dark", dark));
        });
    }

    static void onConfigurationChanged(Configuration configuration) {
        if (HaylenBridge.activity() != null) {
            HaylenBridge.emit("system.themeChanged", Collections.singletonMap("dark", isDark(configuration)));
        }
    }

    private static boolean isDark(Configuration configuration) {
        return (configuration.uiMode & Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES;
    }
}
```

`app/src/main/java/com/example/myapp/MyAppApplication.java`:

```java
package com.example.myapp;

import android.app.Application;
import android.content.res.Configuration;

public final class MyAppApplication extends Application {
    @Override
    public void onCreate() {
        super.onCreate();
        ThemePlugin.register();
    }

    @Override
    public void onConfigurationChanged(Configuration configuration) {
        super.onConfigurationChanged(configuration);
        ThemePlugin.onConfigurationChanged(configuration);
    }
}
```

The manifest names the application class and keeps `uiMode` in the activity's `android:configChanges`, so a theme change reaches the app instead of recreating the activity:

```xml
<application android:name=".MyAppApplication" android:label="My App">
    <activity
        android:name="dev.haylen.HaylenActivity"
        android:configChanges="orientation|screenSize|screenLayout|smallestScreenSize|keyboard|keyboardHidden|navigation|density|uiMode"
        android:exported="true">
        <meta-data android:name="android.app.lib_name" android:value="my-app" />
        <intent-filter>
            <action android:name="android.intent.action.MAIN" />
            <category android:name="android.intent.category.LAUNCHER" />
        </intent-filter>
    </activity>
</application>
```

The handler runs on the main thread only while an activity exists, so `HaylenBridge.activity()` is never `null` inside it. The night mode bits of `uiMode` and `Collections.singletonMap` work from API 27, the engine's minimum, while `Configuration.isNightModeActive` and `Map.of` need API 30.

### Web

The app's shell, passed to `haylen_add_app` as `WEB_SHELL`, registers the handler in `preRun`:

```html
<canvas id="canvas" tabindex="-1" oncontextmenu="event.preventDefault()"></canvas>
<script>
    function registerTheme() {
        const query = window.matchMedia("(prefers-color-scheme: dark)");
        Module.haylen.register("system.theme", () => ({ dark: query.matches }));
        query.addEventListener("change", () => Module.haylen.emit("system.themeChanged", { dark: query.matches }));
    }

    var Module = {
        canvas: document.getElementById("canvas"),
        preRun: [registerTheme],
        haylen: {
            packageUrl: new URLSearchParams(location.search).get("package"),
        },
    };
</script>
{{{ SCRIPT }}}
```

### Lua

`source/main.lua`:

```lua
local async = require('async')
local haylen = require('haylen')
local platform = require('haylen.platform')
local ui = require('haylen.ui')

-- Only Android and the web answer system.theme natively, so a Lua handler stands in elsewhere.
if haylen.platform ~= 'android' and haylen.platform ~= 'web' then
    platform.registerHandler('system.theme', function()
        return {dark = true}
    end)
end

local function applyTheme(theme)
    ui.setTheme(theme.dark and 'dark' or 'light')
end

async.spawn(function()
    local theme, err = platform.call('system.theme'):await()
    if theme then
        applyTheme(theme)
    else
        print('system.theme failed: ' .. err)
    end
end)

platform.on('system.themeChanged', applyTheme)
```

## Testing bridge code

The headless host used by the engine tests records every call that reaches native code, and a test answers it through the bridge:

```cpp
ASSERT_TRUE(fixture.frameUntil([&] { return fixture.host().getPlatformCalls().size() == 1; }));
fixture.engine().getPlatform().resolve(fixture.host().getPlatformCalls()[0].id, true, R"({"model": "phone"})");
```

`platform::HeadlessHost::getPlatformCalls()` returns the id, method and JSON parameters of each call, and `getCancelledCalls()` the ids of the calls the bridge gave up through a timeout or a cancel. Lua code under test can also stand in for native methods with `platform.registerHandler`, and the native interop tests load `engine/tests/native/NativeTest.c`, whose init function registers handlers through `HaylenNativeApi`. See the [testing guide](testing.md).
