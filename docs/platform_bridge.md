# Platform bridge

The platform bridge connects an app to native code. An app calls a named method with JSON parameters and receives its JSON result asynchronously, and native code sends named events with JSON payloads that the app listens to. Every result and every event reaches the app on the frame thread, at the start of a frame. The bridge covers everything the engine does not wrap itself, such as sign-in, purchases, sharing, deep links or system settings, with handlers written in Java or Kotlin on Android, Objective-C or Swift on Apple platforms, JavaScript on the web, and C++ or Lua anywhere.

This guide explains how the bridge works and how each platform implements methods. The Lua functions are documented in the [haylen.platform reference](lua-api/platform.md), and the [architecture guide](architecture.md) places the bridge among the other engine systems.

## How a call travels

```text
Lua platform.call ─► Bridge::call ─► engine handler (C++ or Lua), answered at once
                                  └► Host::dispatchPlatformCall ─► Services::dispatch ─► native handler
native reply (any thread) ─► BridgeRelay::resolve ─► Bridge::resolve ─► queue
next frame: Engine::frame ─► Bridge::pump ─► callback or Lua promise, on the frame thread
```

1. `platform.call(method, params)` reaches `platform::Bridge::call` (`engine/src/platform/Bridge.cpp`), which gives the call a numeric id and keeps its callback.
2. A handler registered inside the engine answers first. Those are C++ handlers added with `Bridge::registerHandler`, Lua handlers added with `platform.register`, and the built-in `engine.info` and `app.version`. The handler runs during the call itself.
3. Otherwise the bridge serializes the parameters and hands `(id, method, paramsJson)` to the host through `Host::dispatchPlatformCall`. The Sokol runtime forwards it to `platform::Services::dispatch`, which each platform folder under `engine/src/platform` implements.
4. Native code answers once, from any thread, with success and a JSON value or with failure and a message. The answer goes through `platform::BridgeRelay::resolve` in `engine/src/platform/sokol/BridgeRelay.cpp` to the bridge of the running engine, which parses it and queues it.
5. At the start of every frame, `Engine::frame` polls the Varn event loop and calls `Bridge::pump`, which runs the queued callbacks and events on the frame thread before the fixed update and the update. In Lua, the promise returned by `platform.call` resolves there.

Native events take the same road: `BridgeRelay::emit(event, payloadJson)` queues the event, and `pump` delivers it to every listener connected with `on`. An event with invalid JSON is logged and dropped. Events that arrive while no app is running, and events that nothing listens to, are dropped too.

The bridge has no timeout. A method whose handler never answers leaves its promise pending, so every handler must answer exactly once. A second answer to the same call is ignored.

## The JSON contract

- Parameters are a JSON value, an empty object when Lua passes none. Lua tables become JSON as the [reference](lua-api/platform.md#how-calls-travel) describes.
- A successful result is any JSON value, and `null` reaches Lua as `nil`.
- A failure carries either a JSON string, which becomes the error message, or an object whose `message` field becomes the error message. Any other failure payload, such as `null`, a number or an object without a string `message`, fails with `The native platform call failed without a message.`, and text that is not JSON fails with `The platform returned invalid JSON.`
- Method and event names are free-form strings. The built-in methods use dotted names such as `device.info` and `system.open_url`, and the Tiny Island sample uses `auth.google.signIn`.

## Built-in methods

These methods work without app code. The results are documented in the [reference](lua-api/platform.md#built-in-methods).

| Method | Answered by |
| --- | --- |
| `engine.info` | The engine on every platform. |
| `app.version` | The engine on every platform, with the `version` of `app.json`. |
| `device.info` | `HaylenBridge` on Android and Apple platforms, the page on the web, and the desktop handlers on Windows and Linux. |
| `system.locale` | Same as `device.info`. |
| `system.open_url` | Same as `device.info`. |
| `haptics.vibrate` | Same as `device.info`. Android and the web vibrate, iOS plays an impact, and the other platforms answer without doing anything. |

An app can replace any of them. Engine handlers always take precedence over native ones. On Android, `HaylenActivity` registers the native built-ins when it is created, which replaces handlers of the same names registered before, so an app that overrides a built-in registers its handler after the activity exists. On Apple platforms and the web, a later registration replaces the built-in.

## The Lua side

| Function | Purpose |
| --- | --- |
| `platform.call(method, params)` | Calls a method and returns a Varn promise. `:await()` inside a coroutine returns the result, or `nil` and the error message. |
| `platform.on(event, listener)` | Calls `listener(payload)` for every native event of that name and returns a connection with `disconnect()`. |
| `platform.register(method, handler)` | Answers a method with a Lua function inside the engine, which takes precedence over native handlers. |
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
| `MethodHandler.handle(Object params, Reply reply)` | The handler interface, usable as a lambda. |
| `Reply.success(Object value)`, `Reply.failure(String message)` | Answer the call once. |

How a call runs:

1. The engine calls the static `HaylenBridge.dispatch(long, byte[], byte[])` through JNI on the frame thread, with the method name and the parameters as UTF-8 bytes. Every text crosses JNI as UTF-8 bytes in both directions, because the JNI string functions use a modified UTF-8 that breaks characters outside the Basic Multilingual Plane, such as emoji. A method without a handler fails with `No native handler is registered for <method>.`.
2. The parameters are parsed with `org.json.JSONTokener`, so an object arrives as a `JSONObject`. Parameters that do not parse fail the call.
3. The handler runs on the main thread. When the activity was destroyed in the meantime, the call fails with `The activity was destroyed before <method> ran.`
4. The handler replies at once or later, from any thread. `success` accepts `null`, strings, numbers, booleans, `JSONObject`, `JSONArray`, maps, collections and arrays, converted with `JSONObject.wrap`.

An exception thrown out of a handler is not caught, so it crashes the app like any exception on the main thread. Catch what can fail and answer with `reply.failure`. `HaylenBridge.emit` calls into the app's native library, which `HaylenActivity` loads when it is created, so native code emits events only while an activity exists.

Apps register their handlers once, in `Application.onCreate`, before the first activity starts. Tiny Island does this in `samples/games/tiny-island/platform/android/app/src/main/java/dev/haylen/tinyisland/TinyIslandApplication.java`, which its `app/app.gradle` names through the `haylenApplication` manifest placeholder of the [Android template](distribution.md#platform-overrides). Its `GoogleSignInPlugin` answers `auth.google.signIn` with Credential Manager: it builds a `GetGoogleIdOption` with the web client id of the game's Google Cloud project, which the build takes from the `googleServerClientId` Gradle property into the `google_server_client_id` string resource, calls `getCredentialAsync` on the activity's main executor and replies with `idToken`, `email`, `name` and `picture`. Without a client id it fails with a message that explains the build property. R8 keeps the classes the engine reaches by name through the module's `consumer-rules.pro`, so handlers need no rules of their own.

## Apple platforms

`engine/include/haylen/platform/apple/HaylenBridge.h` declares the registry for macOS, iOS and tvOS, and `engine/src/platform/apple/HaylenBridge.mm` implements it on top of `AppleBridge`, the handler table of `engine/src/platform/apple/AppleBridge.mm`:

```objc
typedef void (^HaylenReply)(BOOL ok, id _Nullable result);
typedef void (^HaylenHandler)(id params, HaylenReply reply);

@interface HaylenBridge : NSObject
+ (void)registerHandler:(NSString*)method handler:(HaylenHandler)handler;
+ (void)removeHandler:(NSString*)method;
+ (void)emit:(NSString*)event payload:(nullable id)payload;
@end
```

- The parameters are parsed with `NSJSONSerialization`, so an object arrives as an `NSDictionary`, and parameters that do not parse arrive as an empty dictionary.
- Handlers run on the main queue and may reply later from any thread. `reply(YES, result)` succeeds with any value `NSJSONSerialization` accepts, or `nil`. `reply(NO, message)` fails with a message string or a dictionary with a `message` string.
- A method without a handler fails with `No native handler is registered for <method>.`

The handler table exists from the first registration, and the built-in methods that the runtime registers when it starts never replace a handler of the same name, so native code may register at any time. Apps made from the Apple template register in the `main` function of `source/main.mm`, before it calls `haylen_main`, with their Objective-C files next to it in the `platform/apple` folder of the app, as the [distribution guide](distribution.md#platform-overrides) describes. Apps built with `haylen_add_app` add the Objective-C file with its `SOURCES` argument, compile it with `-fobjc-arc` as the engine compiles its Apple sources, and register from `+load` or once the app has launched. The [reference](lua-api/platform.md#apple-handlers) has a complete handler.

## Web

The page side of the bridge lives in `engine/platform/web/haylen-runtime.js`, which every web target links with `--pre-js` and which fills `Module.haylen`:

| Function | Meaning |
| --- | --- |
| `Module.haylen.register(method, handler)` | Adds or replaces the handler of a method. |
| `Module.haylen.unregister(method)` | Removes a handler. |
| `Module.haylen.emit(event, payload)` | Sends an event to the app. The payload is converted with `JSON.stringify`. |

The runtime calls the handler with the parsed parameters. The handler returns the result or a promise for it, and a thrown error or a rejected promise fails the call with the error message. A method without a handler fails with `No page handler is registered for <method>.` The page answers `device.info`, `system.locale`, `system.open_url` and `haptics.vibrate` with its own handlers, which `register` can replace.

The web build is single-threaded, so handlers run on the browser's main thread between frames. An asynchronous handler, such as one that waits for `fetch` or for a sign-in popup, never blocks the app while it waits.

`register` exists once the runtime script has run, so a page registers its handlers in `Module.preRun`. The web template runs the `app.js` of an app for that, and Tiny Island's `samples/games/tiny-island/platform/web/app.js` registers `auth.google.signIn` there. It loads Google Identity Services only when the player first signs in, reads the client id from a constant at the top of the file, and resolves with `idToken`, `email`, `name` and `picture` decoded from the returned credential. The Google script needs `make.py run --platform web --coep off`, as the [distribution guide](distribution.md#serve) explains.

## Desktop

macOS uses the Apple registry above. Windows and Linux have no native registry: `DesktopMethods` in `engine/src/platform/desktop/DesktopMethods.cpp` answers the four built-in methods with the answers of its `WindowsMethods` and `LinuxMethods` subclasses, synchronously on the frame thread, and fails every other method. Their answers still reach the app at the start of the next frame. `system.open_url` uses `ShellExecuteW` on Windows and runs `xdg-open` on Linux, where a helper thread waits for it to exit so the frame never blocks. Apps add methods on these platforms with C++ or Lua handlers.

## C++ handlers and calls

`haylen::platform::Bridge` in `engine/include/haylen/platform/Bridge.hpp` is the whole bridge, and `engine.getPlatform()` returns the running one:

| Member | Meaning |
| --- | --- |
| `registerHandler(std::string method, Bridge::Handler handler)` | Answers a method inside the engine. The handler receives `const Json& params` and a `Bridge::Reply`, which takes a `Bridge::Result{ok, value, error}`. |
| `hasHandler(std::string_view method)` | Whether an engine handler answers the method. |
| `call(std::string_view method, const Json& params, Bridge::Callback callback)` | Calls a method and returns the call id. The callback receives the `Bridge::Result` on the frame thread. |
| `on(const std::string& event, std::function<void(const Json&)> listener)` | Listens to native events and returns a `Connection`. |
| `resolve(std::uint64_t id, bool ok, std::string_view resultJson)` | Answers a call. Safe from any thread. |
| `emit(std::string_view event, std::string_view payloadJson)` | Sends an event. Safe from any thread. |
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
        haylen::core::Log::warning("device.info failed: {}", result.error);
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
import java.util.Map;

// Tells the app whether the device uses a dark theme, and when that changes.
final class ThemePlugin {
    private ThemePlugin() {}

    static void register() {
        HaylenBridge.register("system.theme", (params, reply) -> {
            boolean dark = HaylenBridge.activity().getResources().getConfiguration().isNightModeActive();
            reply.success(Map.of("dark", dark));
        });
    }

    static void onConfigurationChanged(Configuration configuration) {
        if (HaylenBridge.activity() != null) {
            HaylenBridge.emit("system.themeChanged", Map.of("dark", configuration.isNightModeActive()));
        }
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

The handler runs on the main thread only while an activity exists, so `HaylenBridge.activity()` is never `null` inside it, and `isNightModeActive` is available from API 30, the engine's minimum.

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
    platform.register('system.theme', function()
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

`platform::HeadlessHost::getPlatformCalls()` returns the id, method and JSON parameters of each call. Lua code under test can also stand in for native methods with `platform.register`. See the [testing guide](testing.md).
