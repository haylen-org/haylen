# haylen.platform

`haylen.platform` is the bridge between the app and native code. An app calls named methods with JSON parameters and awaits their JSON result, which may fail with a typed error, time out or be cancelled, and it listens to named events that native code sends. Use it for everything the engine does not wrap, such as sign-in, purchases, sharing, deep links or haptics, with the handlers living in Java or Kotlin on Android, Objective-C or Swift on Apple platforms, JavaScript on the web, C in native libraries, or C++ anywhere. The Lua modules of [plugins](../plugins.md) reach their native parts through the [handle of their plugin](#plugin-handles). The [native code guide](../native.md) compares the bridge with the other ways to reach native code.

```lua
local platform = require('haylen.platform')
```

## How calls travel

`platform.call` looks for a handler in this order:

1. A handler registered in the engine, either from Lua with `platform.registerHandler` or from C++. The engine itself registers `engine.info` and `app.version` this way.
2. A handler that a native library registered through the `HaylenNativeApi` of the engine, as [haylen.native](native.md#library-handlers) describes.
3. The native handler of the running platform: `HaylenBridge` on Android and Apple platforms, `Module.haylen` on the web, and the desktop handlers on Windows and Linux.

A method that no handler answers fails with the code `noHandler` and `No native handler is registered for <method>.`, or `No page handler is registered for <method>.` on the web.

Parameters and results cross the bridge as JSON. Lua tables with only consecutive integer keys starting at 1 become arrays, other tables become objects, and an empty table becomes an empty object. Functions, userdata and other values that JSON cannot hold raise `A <type> cannot be converted to JSON.`. JSON `null` arrives in Lua as `nil`.

Results and events always reach Lua on the frame thread, at the start of a frame, before the app updates. A result never arrives during the `platform.call` that asked for it, even when a Lua handler answers at once. Every call gets an id that is unique in the whole process, so a native answer that arrives after the app restarted, for example after a hot reload, never answers a call of the restarted app and is dropped.

## Functions

### platform.call(method, params, options)

Calls the method named `method` with `params`, which default to an empty object, and returns a [call](#calls) at once. `call:await()` inside a coroutine, such as one started with `async.spawn` or `scene.spawn`, returns the result, or `nil` and an [error](#errors). `options` is an optional table:

| Option | Type | Meaning |
| --- | --- | --- |
| `timeout` | number | Seconds of real time after which the call fails with the code `timeout`, and the native handler hears that the app gave it up. It counts while the app is paused, because the native side keeps working. |

An empty method name raises `A platform call needs a method name.` at once, a timeout that is not a positive number raises `The timeout of a platform call is a positive number of seconds.`, and an unknown option raises an error that names it.

```lua
local async = require('async')
local platform = require('haylen.platform')

async.spawn(function()
    local account, err = platform.call('auth.google.signIn', {prompt = true}, {timeout = 60}):await()
    if account then
        print('signed in as ' .. account.email)
    elseif err.code == 'timeout' then
        print('the sign-in took too long')
    else
        print('sign-in failed: ' .. err)
    end
end)
```

### platform.send(method, params)

Calls the method named `method` with `params`, which default to an empty object, when nothing needs its answer, such as an analytics event. It goes to the same handler as `platform.call`, creates no call, never counts in `platform.pendingCallCount()` and drops the answer, a failure included. An empty method name raises `A platform call needs a method name.`.

```lua
local platform = require('haylen.platform')

platform.send('analytics.logEvent', {name = 'levelStart', level = 3})
```

### platform.on(event, listener)

Calls `listener(payload)` every time native code sends the event named `event`, with the payload converted from JSON. Returns a `haylen.Connection`. Its `disconnect()` method stops the listener, and its `connected` property is `true` until then. A listener stays connected when the connection object is garbage collected. An error raised inside a listener stops the app and shows the error screen with the message and its stack trace. Events that no listener waits for are dropped, except retained ones.

Native code retains an event that may come before the app listens, such as the deep link or the notification that opened the app, or purchases that finished while it was closed. A retained event that nothing listens to waits, up to 32 per name with the oldest dropped first, and the first listener of its name receives the waiting events in order at the start of the next frame, before any newer event of that name. Events wait in the running app, so a restarted app starts with none.

The engine sends no events of its own, so event names are a contract between the app and its native code. Plugins name their events after their id, such as `admob.closed`.

```lua
local platform = require('haylen.platform')

local links = platform.on('app.link', function(payload)
    print('opened from ' .. payload.url)
end)

local function leaveMenu()
    links:disconnect()
end
```

### platform.registerHandler(method, handler)

Answers the method named `method` with the Lua function `handler(params)`, which returns the result. The function runs during the `platform.call` that asks for it and must return at once, and its result must convert to JSON. An error raised inside it fails the call with the error message and its stack trace instead of stopping the app. A registered handler replaces any earlier engine handler of the same method, including `engine.info` and `app.version`, and it takes precedence over native handlers. Lua handlers suit desktop builds and tests that stand in for mobile services.

An empty method name raises `A platform handler needs a method name and a function.`, and a `handler` that is not a function raises an argument error.

```lua
local async = require('async')
local haylen = require('haylen')
local platform = require('haylen.platform')

-- Desktop builds and tests have no store, so a Lua stand-in answers there. Phones keep their native handler, which hasHandler cannot see.
local standIn = {windows = true, linux = true, headless = true}
if standIn[haylen.platform] then
    platform.registerHandler('store.buy', function(params)
        if params.item == nil then
            error('store.buy needs an item')
        end
        return {item = params.item, receipt = 'desktop-test'}
    end)
end

async.spawn(function()
    local purchase, err = platform.call('store.buy', {item = 'coins_100'}):await()
    print(purchase and purchase.receipt or err)
end)
```

### platform.hasHandler(method)

Returns `true` when an engine handler answers `method`, which covers `engine.info`, `app.version`, methods registered with `platform.registerHandler` and methods C++ code registered. It does not see native handlers, so it returns `false` for `device.info` even where the platform answers it.

```lua
local platform = require('haylen.platform')

print(platform.hasHandler('engine.info'))
print(platform.hasHandler('device.info'))
```

### platform.pendingCallCount()

Returns how many calls still wait for their result, counting calls whose answer arrived but has not reached Lua at the start of a frame yet.

```lua
local platform = require('haylen.platform')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        self.busy = platform.pendingCallCount() > 0
    end,
})
```

### platform.resolve(id, ok, result)

Answers the pending call `id` the way native code does. With `ok` true the call resolves with `result`, and with `ok` false it fails with `result`, a message string or a table with `message`, `code` and `data`. The answer reaches Lua at the start of the next frame, and an id that no pending call has is dropped then. It suits tests that stand in for native code. A negative id raises a bad argument error with `expected a non-negative integer`, and a result that JSON cannot hold raises `A <type> cannot be converted to JSON.`.

```lua
local async = require('async')
local platform = require('haylen.platform')

local purchase = platform.call('store.buy', {item = 'coins_100'})
platform.resolve(purchase.id, false, {message = 'The card was declined.', code = 'declined', data = {retry = false}})

async.spawn(function()
    local _, err = purchase:await()
    print(err.code .. ': ' .. err.message)
end)
```

### platform.emit(event, payload, options)

Sends the event `event` with `payload` the way native code does, so `platform.on` listeners receive it at the start of the next frame. With `options.retain` set to `true`, the event waits for the first listener of its name like a [retained event](#platformonevent-listener) of native code. It suits tests and desktop builds that stand in for native events. A payload that JSON cannot hold raises `A <type> cannot be converted to JSON.`, and an unknown option raises an error that names it.

```lua
local platform = require('haylen.platform')

platform.emit('app.link', {url = 'island://beach'}, {retain = true})

-- The link was retained, so this listener receives it at the start of the next frame.
platform.on('app.link', function(payload)
    print('opened from ' .. payload.url)
end)
```

### platform.plugins()

Returns a list with one table per plugin that `app.json` lists, in the order of their ids, each with `id`, the `version` of its `plugin.json` and `native`, which is `true` when the native part of the plugin runs on this platform: the platform loaded it, or a native library of the app declared it through [`registerPlugin`](native.md#library-handlers). A `plugin.json` of the package that is not a JSON object with a `version` raises an error that names it.

```lua
local platform = require('haylen.platform')

for _, plugin in ipairs(platform.plugins()) do
    print(plugin.id .. ' ' .. plugin.version .. (plugin.native and '' or ' without its native part'))
end
```

### platform.plugin(id)

Returns the [handle](#plugin-handles) of the plugin `id` that `app.json` lists, which the Lua modules of the plugin use to reach its native part. An id that `app.json` does not list raises `The plugin <id> is not among the plugins of app.json.`.

```lua
local platform = require('haylen.platform')

local ads = platform.plugin('admob')
print(ads.id, ads.version, ads.native, ads.config.testMode)
```

## Calls

`platform.call` returns a `haylen.PlatformCall`.

| Member | Meaning |
| --- | --- |
| `call:await()` | Waits inside a coroutine and returns the result, or `nil` and the [error](#errors) of the call. Awaiting a call that already settled returns at once. |
| `call:cancel()` | Fails the call with the code `cancelled` at the start of the next frame and tells the native handler, which may stop its work. Returns `true` when the call was still pending, and `false` when it had already settled, when the call stays as it was. |
| `call.id` | The id of the call, an integer that is unique in the process and that `platform.resolve` takes. |
| `call.done` | Whether the call has settled. |
| `call.promise` | The Varn promise of the call, for the combinators of `async` such as `async.all` and `async.race`, where a failure is only its message. |

```lua
local async = require('async')
local platform = require('haylen.platform')

local download = platform.call('store.downloadLevels', {pack = 2})

async.spawn(function()
    local levels, err = download:await()
    print(levels and #levels or err.code)
end)

local function leaveShop()
    download:cancel()
end

async.spawn(function()
    local both = async.all({platform.call('engine.info').promise, platform.call('app.version').promise}):await()
    print(both[1].engine .. ' ' .. both[2])
end)
```

## Plugin handles

`platform.plugin(id)` returns a `haylen.AppPlugin`, the handle through which the Lua API of a plugin talks to its native part on every platform. Its methods and events carry the id of the plugin in front of their names, which is how the native parts of plugins register them.

| Member | Meaning |
| --- | --- |
| `handle.id` | The id of the plugin, such as `'admob'`. |
| `handle.version` | The `version` of its `plugin.json`. |
| `handle.config` | A table with the parameter values of the plugin in `app.json`, over the `default` of every parameter of its `plugin.json`, the same values its native parts receive. |
| `handle.native` | Whether the native part of the plugin runs on this platform, read anew every time, since a native library may declare it later. |
| `handle:call(method, params, options)` | The same as `platform.call(id .. '.' .. method, params, options)`. |
| `handle:send(method, params)` | The same as `platform.send(id .. '.' .. method, params)`. |
| `handle:on(event, listener)` | The same as `platform.on(id .. '.' .. event, listener)`. |

The members are read-only, and an empty method or event name raises `A method or event of the plugin <id> needs a name.`. A platform without the native part of the plugin answers its calls with the code `noHandler`, so a plugin checks `handle.native` when it can do without it. The [plugin guide](../plugins.md#lua-api-of-plugins) shows the Lua API of a whole plugin.

```lua
-- plugins/admob/source/init.lua
local platform = require('haylen.platform')

local handle = platform.plugin('admob')
local admob = {}

function admob.showBanner(placement)
    return handle:call('showBanner', {placement = placement or handle.config.placement})
end

function admob.logImpression(name)
    handle:send('logImpression', {name = name})
end

function admob.onClosed(listener)
    return handle:on('closed', listener)
end

return admob
```

## Errors

A failed call returns a table with three fields, which reads as its message in `tostring` and in string concatenation, so `'failed: ' .. err` prints the message.

| Field | Type | Meaning |
| --- | --- | --- |
| `message` | string | What went wrong, for people. |
| `code` | any JSON value or `nil` | What went wrong, for code: the code that the native handler gave, or one of the codes below. |
| `data` | any JSON value or `nil` | Details that the native handler added, such as a retry delay. |

| Code | When |
| --- | --- |
| `timeout` | The `timeout` of the call passed first. |
| `cancelled` | `call:cancel()` gave up the call. |
| `noHandler` | No handler answers the method. |
| `invalidJson` | Native code answered with text that is not JSON, with the message `The platform returned invalid JSON.`. |
| `exception` | A Java, Kotlin, Swift or JavaScript handler threw an error without a code of its own instead of answering. `data.type` names the class of the exception or the type of the error. |

A native failure that is a string becomes the message. A failure object gives its `message`, `code` and `data`, and one without a string `message`, like any other failure payload such as `null` or a number, fails with `The native platform call failed without a message.`. A Lua handler registered with `platform.registerHandler` fails with the error it raised and its stack trace as the message.

```lua
local async = require('async')
local platform = require('haylen.platform')

async.spawn(function()
    local _, err = platform.call('store.buy', {item = 'coins_100'}):await()
    if err and err.code == 'declined' then
        print('try another card: ' .. tostring(err))
    elseif err then
        print('store.buy failed with ' .. tostring(err.code) .. ': ' .. err.message)
    end
end)
```

## Built-in methods

These methods work without app code. `engine.info` and `app.version` are answered by the engine on every platform, and the others by the native side of each platform, where an app can replace them.

### engine.info

Takes no parameters and returns a table about the engine.

| Field | Type | Meaning |
| --- | --- | --- |
| `engine` | string | Always `'Haylen'`. |
| `version` | string | Engine version. |
| `platform` | string | `'android'`, `'ios'`, `'tvos'`, `'macos'`, `'windows'`, `'linux'` or `'web'`. |
| `backend` | string | Graphics backend: `'metal'`, `'d3d11'`, `'glcore'`, `'gles3'` or `'webgpu'`. |

```lua
local async = require('async')
local platform = require('haylen.platform')

async.spawn(function()
    local info = platform.call('engine.info'):await()
    print(info.engine .. ' ' .. info.version .. ' on ' .. info.platform .. ' with ' .. info.backend)
end)
```

### app.version

Takes no parameters and returns the `version` string of `app.json`, which defaults to `'1.0.0'`.

```lua
local async = require('async')
local platform = require('haylen.platform')

async.spawn(function()
    print('version ' .. platform.call('app.version'):await())
end)
```

### device.info

Takes no parameters and returns a table about the device.

| Field | Type | Meaning |
| --- | --- | --- |
| `model` | string | Device model on Android, the `UIDevice` model on iOS and tvOS, `'Mac'` on macOS, `'PC'` on Windows and Linux and `'Browser'` on the web. |
| `system` | string | `'Android'`, the system name on iOS and tvOS, `'macOS'`, `'Windows'`, `'Linux'` or `'Web'`. |
| `systemVersion` | string | Version of the operating system: the Android release, the iOS or tvOS version, the macOS version such as `'14.5.0'`, the Windows version such as `'10.0.22631'`, the Linux kernel release and the browser user agent on the web. |
| `locale` | string | Language of the player, as `system.locale` returns it. |

```lua
local async = require('async')
local platform = require('haylen.platform')

async.spawn(function()
    local device = platform.call('device.info'):await()
    print(device.model .. ', ' .. device.system .. ' ' .. device.systemVersion .. ', ' .. device.locale)
end)
```

### system.locale

Takes no parameters and returns the language of the player as a BCP 47 tag such as `'pt-BR'`. Android reports the default locale, Apple platforms the first preferred language, Windows the user default locale, Linux the `LANG` variable with `'en-US'` for the `C` and `POSIX` locales, and the web `navigator.language`.

```lua
local async = require('async')
local platform = require('haylen.platform')

async.spawn(function()
    local locale = platform.call('system.locale'):await()
    print('player language ' .. locale)
end)
```

### system.openUrl

Opens `params.url` in the browser or the app that handles it and returns `true` once the system took it, on every platform. A missing or empty URL fails the call with `The url is missing.`, and a URL that no application opens, or that the browser blocks, fails it with `The url could not be opened.`. Windows and Linux find out without holding up the app, since Linux waits for `xdg-open` on a helper thread.

```lua
local async = require('async')
local platform = require('haylen.platform')

async.spawn(function()
    local _, err = platform.call('system.openUrl', {url = 'https://example.com/tiny-island'}):await()
    if err then
        print('could not open the page: ' .. err)
    end
end)
```

### haptics.vibrate

Vibrates the device and returns `nil`. Android and the web vibrate for `params.duration` milliseconds, which defaults to 40, where the device and the browser support it. iOS plays a medium impact. macOS, tvOS, Windows and Linux do nothing.

```lua
local platform = require('haylen.platform')

platform.call('haptics.vibrate', {duration = 80})
```

## Native handlers

Native handlers receive the parameters as parsed JSON and answer once, with success and a JSON value or with failure and a message, a code and data. They may answer later, from any thread, and the answer reaches the app on the frame thread. A second answer, and an answer that comes after the app cancelled the call or its timeout passed, is dropped. Native code sends events the same way, and `platform.on` receives them. An app handler registered under the name of a built-in method replaces it, whenever it registers.

## Android handlers

`dev.haylen.HaylenBridge` in the engine Android library holds the handlers. `HaylenBridge.register(method, handler)` adds a handler, `HaylenBridge.unregister(method)` removes it and `HaylenBridge.emit(event, payload)` sends an event. Handlers run on the main thread. `params` is what `org.json.JSONTokener` reads from the parameters, usually a `JSONObject`. The reply of a call has these members:

| Member | Meaning |
| --- | --- |
| `success(value)` | Answers with `null`, a string, a number, a boolean, a `JSONObject`, a `JSONArray`, a map or a collection. |
| `failure(message)` | Fails the call with a message. |
| `failure(message, code, data)` | Fails the call with a code and data, both optional. |
| `failure(throwable)` | Fails the call with a thrown error: a `HaylenBridge.Failure` keeps its code and data, and any other error fails with the code `exception`. |
| `isCancelled()` | Whether the app cancelled the call or its timeout passed. |
| `onCancel(runnable)` | Runs on the main thread when the app cancels the call or its timeout passes, at once when that already happened. |

A handler that throws fails its call through `failure(throwable)` instead of crashing the app, so `throw new HaylenBridge.Failure(message, code, data)` fails a call with a code. `HaylenBridge.activity()` returns the running activity. The built-in methods are registered when the class loads, before any other code can register, so a handler registered under one of their names, for example in `Application.onCreate`, replaces them.

```java
import dev.haylen.HaylenBridge;
import org.json.JSONObject;

public final class StorePlugin {
    private StorePlugin() {}

    // Called from Application.onCreate, before the first activity starts.
    public static void register() {
        HaylenBridge.register("store.buy", (params, reply) -> {
            String item = params instanceof JSONObject ? ((JSONObject) params).optString("item", "") : "";
            if (item.isEmpty()) {
                throw new HaylenBridge.Failure("store.buy needs an item.", "missingItem", null);
            }
            reply.success(new JSONObject().put("item", item).put("receipt", "play-store-token"));
        });
    }

    public static void onDeepLink(String url) throws Exception {
        HaylenBridge.emit("app.link", new JSONObject().put("url", url));
    }
}
```

Kotlin handlers written as suspending functions register with `HaylenCoroutines.register(method) { params -> result }`. Each call runs in a coroutine on the main thread, its result answers the call, a thrown exception fails it like `failure(throwable)`, and the coroutine is cancelled when the app cancels the call or its timeout passes. The library brings `kotlinx-coroutines-android` with it.

```kotlin
import dev.haylen.HaylenBridge
import dev.haylen.HaylenCoroutines
import kotlinx.coroutines.delay
import org.json.JSONObject

object ProfilePlugin {
    fun register() {
        HaylenCoroutines.register("profile.load") { params ->
            val id = (params as? JSONObject)?.optString("id").orEmpty()
            if (id.isEmpty()) {
                throw HaylenBridge.Failure("profile.load needs an id.", "missingId", null)
            }
            delay(100)
            mapOf("id" to id, "name" to "Player")
        }
    }
}
```

## Apple handlers

`HaylenBridge` in `haylen/platform/apple/HaylenBridge.h` holds the handlers on iOS, tvOS and macOS. `+registerHandler:handler:` adds a handler, `+registerCancellableHandler:handler:` adds one that returns a block to run when the app cancels the call or its timeout passes, `+removeHandler:` removes a handler and `+emit:payload:` sends an event. Handlers run on the main queue, and so does the cancel block. `params` is the parsed JSON, an `NSDictionary` for object parameters, and the handler answers with `reply(YES, result)` or `reply(NO, failure)`. `result` is any value `NSJSONSerialization` accepts or `nil`, and a success value it rejects fails the call with `The native handler for <method> returned a value that is not JSON.`. A failure passes a message string or a dictionary with a `message` string and optional `code` and `data`, and anything else fails with `The native handler for <method> failed.`. Handlers can be registered at any time, even before the app starts, and a handler under the name of a built-in method replaces it. An event payload that is not JSON is logged as an error and dropped.

```objc
#import "haylen/platform/apple/HaylenBridge.h"

@interface StorePlugin : NSObject
@end

@implementation StorePlugin

+ (void)load {
    [HaylenBridge registerHandler:@"store.buy" handler:^(id params, HaylenReply reply) {
        NSString* item = [params isKindOfClass:NSDictionary.class] ? params[@"item"] : nil;
        if (item == nil) {
            reply(NO, @{@"message" : @"store.buy needs an item.", @"code" : @"missingItem"});
            return;
        }
        reply(YES, @{@"item" : item, @"receipt" : @"app-store-token"});
    }];
}

@end
```

```objc
[HaylenBridge emit:@"app.link" payload:@{@"url" : url.absoluteString}];
```

Swift handlers written as async functions register with `HaylenBridge.register(method) { (params: Params) async throws -> Result in ... }` from `HaylenBridgeAsync.swift` of the Apple template, where `Params` is `Decodable` and `Result` is `Encodable`. A handler runs on the main actor, a thrown `HaylenFailure(message, code:, data:)` fails the call with its code and data, any other error fails it with the code `exception`, and the task is cancelled when the app cancels the call or its timeout passes.

```swift
import Foundation

@objc final class ProfileHandlers: NSObject {
    struct Request: Decodable {
        let id: String
    }

    struct Profile: Encodable {
        let id: String
        let name: String
    }

    @objc static func registerHandlers() {
        HaylenBridge.register("profile.load") { (request: Request) async throws -> Profile in
            if request.id.isEmpty {
                throw HaylenFailure("profile.load needs an id.", code: "missingId")
            }
            try await Task.sleep(nanoseconds: 100_000_000)
            return Profile(id: request.id, name: "Player")
        }
    }
}
```

## Web handlers

`Module.haylen` in the page holds the handlers, and the page answers `device.info`, `system.locale`, `system.openUrl` and `haptics.vibrate` itself. `Module.haylen.register(method, handler)` adds or replaces a handler, `Module.haylen.unregister(method)` removes it and `Module.haylen.emit(event, payload, {retain})` sends an event, which waits for the first listener of its name when `retain` is `true`. Events sent before the first app starts reach it once it starts. A handler runs after the frame that made the call and receives the parsed parameters and a context with the `call` id and a `signal`, an `AbortSignal` that aborts when the app cancels the call or its timeout passes, and it returns the result or a promise for it. A call cancelled in the frame that made it never runs its handler. A thrown error or a rejected promise fails the call with the error message and the `code` and `data` properties of the error, or with the code `exception` and the `name` of the error in `data.type` when the error has no code. The page registers its handlers before the runtime starts, for example in `Module.preRun`.

```html
<script>
    var Module = {
        canvas: document.getElementById("canvas"),
        preRun: [function () {
            Module.haylen.register("store.buy", async (params, context) => {
                if (!params.item) {
                    throw Object.assign(new Error("store.buy needs an item."), { code: "missingItem" });
                }
                const response = await fetch("/api/buy", { method: "POST", body: JSON.stringify(params), signal: context.signal });
                return await response.json();
            });
            window.addEventListener("hashchange", () => Module.haylen.emit("app.link", { url: location.href }));
        }],
    };
</script>
```

## Native library handlers

A native library registers handlers written in C through the `HaylenNativeApi` that `native.load` hands to its init function, on every platform that loads native libraries, sends events that may be retained, and declares itself the native part of a plugin. The [haylen.native reference](native.md#library-handlers) describes them.

## C++ handlers

C++ code registers handlers that run inside the engine with `engine.getPlatform().registerHandler(method, handler)`, where the handler receives the parameters as `haylen::core::Json` and a reply function that takes a `haylen::platform::Bridge::Result` with `ok`, `value` and an `error` of `message`, `code` and `data`. `engine.getPlatform().emit(event, payloadJson, retain)` sends an event from any thread, retained when `retain` is `true`, `engine.getPlatform().send(method, params)` calls a method whose answer nobody needs, and `engine.getAppPlugins()` returns the plugins of `app.json` as `haylen::platform::AppPlugin` records with `id`, `version`, `config` and `native`.

```cpp
engine.getPlatform().registerHandler("save.cloudSync", [](const haylen::core::Json& params, haylen::platform::Bridge::Reply reply) {
    if (!params.contains("slot")) {
        reply({.error = {.message = "save.cloudSync needs a slot.", .code = "missingSlot"}});
        return;
    }
    reply({.ok = true, .value = {{"synced", params.at("slot")}}});
});
engine.getPlatform().emit("app.link", R"({"url": "island://beach"})");
```
