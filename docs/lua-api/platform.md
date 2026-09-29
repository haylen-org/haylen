# haylen.platform

`haylen.platform` is the bridge between the app and native code. An app calls named methods with JSON parameters and gets a promise for their JSON result, and it listens to named events that native code sends. Use it for everything the engine does not wrap, such as sign-in, purchases, sharing, deep links or haptics, with the handlers living in Java or Kotlin on Android, Objective-C or Swift on Apple platforms, JavaScript on the web or C++ anywhere.

```lua
local platform = require('haylen.platform')
```

## How calls travel

`platform.call` looks for a handler in this order:

1. A handler registered in the engine, either from Lua with `platform.register` or from C++. The engine itself registers `engine.info` and `app.version` this way.
2. The native handler of the running platform: `HaylenBridge` on Android and Apple platforms, `Module.haylen` on the web, and the desktop handlers on Windows and Linux.

A method that no handler answers fails with `No native handler is registered for <method>.`, or `No page handler is registered for <method>.` on the web.

Parameters and results cross the bridge as JSON. Lua tables with only consecutive integer keys starting at 1 become arrays, other tables become objects, and an empty table becomes an empty object. Functions, userdata and other values that JSON cannot hold raise `A <type> cannot be converted to JSON.`. JSON `null` arrives in Lua as `nil`.

Results and events always reach Lua on the frame thread, at the start of a frame, before the app updates. A result never arrives during the `platform.call` that asked for it, even when a Lua handler answers at once. Every call gets an id that is unique in the whole process, so a native answer that arrives after the app restarted, for example after a hot reload, never answers a call of the restarted app and is dropped.

## Functions

### platform.call(method, params)

Calls the method named `method` with `params`, which default to an empty object, and returns a Varn promise and the id of the call, an integer that `platform.resolve()` takes. `promise:await()` inside a coroutine started with `async.spawn` returns the result, or `nil` and the error message when the call failed. The call fails with:

- The message of the native handler, which native code gives as a string or as an object with a `message` field. Any other failure payload, such as `null`, a number or an object without a string `message`, fails with `The native platform call failed without a message.`.
- `The platform returned invalid JSON.` when native code answers with text that is not JSON.
- The error of a Lua handler registered with `platform.register`.
- `No native handler is registered for <method>.` when nothing answers the method.

An empty method name raises `A platform call needs a method name.` at once.

```lua
local async = require('async')
local platform = require('haylen.platform')

async.spawn(function()
    local account, err = platform.call('auth.google.signIn', {prompt = true}):await()
    if account then
        print('signed in as ' .. account.email)
    else
        print('sign-in failed: ' .. err)
    end
end)
```

### platform.on(event, listener)

Calls `listener(payload)` every time native code sends the event named `event`, with the payload converted from JSON. Returns a `haylen.Connection`. Its `disconnect()` method stops the listener, and its `connected` property is `true` until then. A listener stays connected when the connection object is garbage collected. An error raised inside a listener stops the app and shows the error screen with the message and its stack trace. Events that no listener waits for are dropped.

The engine sends no events of its own, so event names are a contract between the app and its native code.

```lua
local platform = require('haylen.platform')

local links = platform.on('app.link', function(payload)
    print('opened from ' .. payload.url)
end)

local function leaveMenu()
    links:disconnect()
end
```

### platform.register(method, handler)

Answers the method named `method` with the Lua function `handler(params)`, which returns the result. The function runs during the `platform.call` that asks for it and must return at once, and its result must convert to JSON. An error raised inside it fails the call with the error message and its stack trace instead of stopping the app. A registered handler replaces any earlier engine handler of the same method, including `engine.info` and `app.version`, and it takes precedence over native handlers. Lua handlers suit desktop builds and tests that stand in for mobile services.

An empty method name raises `A platform handler needs a method name and a function.`, and a `handler` that is not a function raises an argument error.

```lua
local async = require('async')
local haylen = require('haylen')
local platform = require('haylen.platform')

-- Desktop builds and tests have no store, so a Lua stand-in answers there. Phones keep their native handler, which hasHandler cannot see.
local standIn = {windows = true, linux = true, headless = true}
if standIn[haylen.platform] then
    platform.register('store.buy', function(params)
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

Returns `true` when an engine handler answers `method`, which covers `engine.info`, `app.version`, methods registered with `platform.register` and methods C++ code registered. It does not see native handlers, so it returns `false` for `device.info` even where the platform answers it.

```lua
local platform = require('haylen.platform')

print(platform.hasHandler('engine.info'))
print(platform.hasHandler('device.info'))
```

### platform.pendingCalls()

Returns how many calls still wait for their result, counting calls whose answer arrived but has not reached Lua at the start of a frame yet.

```lua
local platform = require('haylen.platform')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        self.busy = platform.pendingCalls() > 0
    end,
})
```

### platform.resolve(id, ok, result)

Answers the pending call `id` the way native code does. With `ok` true the call resolves with `result`, and with `ok` false it fails with `result`, a message string or a table with a `message` field. The answer reaches Lua at the start of the next frame, and an id that no pending call has is dropped then. It suits tests that stand in for native code and timeouts that give up on a slow native answer, which is dropped when it arrives later. A negative id raises a bad argument error with `expected a non-negative integer`, and a result that JSON cannot hold raises `A <type> cannot be converted to JSON.`.

```lua
local async = require('async')
local platform = require('haylen.platform')
local timer = require('haylen.timer')

local purchase, id = platform.call('store.buy', {item = 'coins_100'})
timer.after(10, function()
    platform.resolve(id, false, {message = 'The store did not answer in time.'})
end)

async.spawn(function()
    local receipt, err = purchase:await()
    print(receipt and receipt.receipt or err)
end)
```

### platform.emit(event, payload)

Sends the event `event` with `payload` the way native code does, so `platform.on` listeners receive it at the start of the next frame. It suits tests and desktop builds that stand in for native events. A payload that JSON cannot hold raises `A <type> cannot be converted to JSON.`.

```lua
local platform = require('haylen.platform')

platform.on('app.link', function(payload)
    print('opened from ' .. payload.url)
end)
platform.emit('app.link', {url = 'island://beach'})
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

### system.open_url

Opens `params.url` in the browser or the app that handles it and returns `true` once the system took it, on every platform. A missing or empty URL fails the call with `The url is missing.`, and a URL that no application opens, or that the browser blocks, fails it with `The url could not be opened.`. Windows and Linux find out without holding up the app, since Linux waits for `xdg-open` on a helper thread.

```lua
local async = require('async')
local platform = require('haylen.platform')

async.spawn(function()
    local _, err = platform.call('system.open_url', {url = 'https://example.com/tiny-island'}):await()
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

Native handlers receive the parameters as parsed JSON and answer once, with success and a JSON value or with failure and a message. They may answer later, from any thread, and the answer reaches the app on the frame thread. Every platform hands a failure to the engine as an object with a `message` field. Native code sends events the same way, and `platform.on` receives them. An app handler registered under the name of a built-in method replaces it, whenever it registers.

## Android handlers

`dev.haylen.HaylenBridge` in the engine Android library holds the handlers. `HaylenBridge.register(method, handler)` adds a handler, `HaylenBridge.unregister(method)` removes it and `HaylenBridge.emit(event, payload)` sends an event. Handlers run on the main thread. `params` is what `org.json.JSONTokener` reads from the parameters, usually a `JSONObject`. `reply.success(value)` accepts `null`, strings, numbers, booleans, `JSONObject`, `JSONArray`, maps and collections, and `reply.failure(message)` fails the call. `HaylenBridge.activity()` returns the running activity. The built-in methods are registered when the class loads, before any other code can register, so a handler registered under one of their names, for example in `Application.onCreate`, replaces them.

```java
import dev.haylen.HaylenBridge;
import org.json.JSONException;
import org.json.JSONObject;

public final class StorePlugin {
    private StorePlugin() {}

    // Called from Application.onCreate, before the first activity starts.
    public static void register() {
        HaylenBridge.register("store.buy", (params, reply) -> {
            String item = params instanceof JSONObject ? ((JSONObject) params).optString("item", "") : "";
            if (item.isEmpty()) {
                reply.failure("store.buy needs an item.");
                return;
            }
            try {
                JSONObject receipt = new JSONObject();
                receipt.put("item", item);
                receipt.put("receipt", "play-store-token");
                reply.success(receipt);
            } catch (JSONException error) {
                reply.failure(error.getMessage());
            }
        });
    }

    public static void onDeepLink(String url) throws JSONException {
        HaylenBridge.emit("app.link", new JSONObject().put("url", url));
    }
}
```

## Apple handlers

`HaylenBridge` in `haylen/platform/apple/HaylenBridge.h` holds the handlers on iOS, tvOS and macOS. `+registerHandler:handler:` adds a handler, `+removeHandler:` removes it and `+emit:payload:` sends an event. Handlers run on the main queue. `params` is the parsed JSON, an `NSDictionary` for object parameters, and the handler answers with `reply(YES, result)` or `reply(NO, message)`. `result` is any value `NSJSONSerialization` accepts or `nil`, and a success value it rejects fails the call with `The native handler for <method> returned a value that is not JSON.`. A failure passes a message string or a dictionary with a `message` string, and anything else fails with `The native handler for <method> failed.`. Handlers can be registered at any time, even before the app starts, and a handler under the name of a built-in method replaces it. An event payload that is not JSON is logged as an error and dropped.

```objc
#import "haylen/platform/apple/HaylenBridge.h"

@interface StorePlugin : NSObject
@end

@implementation StorePlugin

+ (void)load {
    [HaylenBridge registerHandler:@"store.buy" handler:^(id params, HaylenReply reply) {
        NSString* item = [params isKindOfClass:NSDictionary.class] ? params[@"item"] : nil;
        if (item == nil) {
            reply(NO, @"store.buy needs an item.");
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

## Web handlers

`Module.haylen` in the page holds the handlers, and the page answers `device.info`, `system.locale`, `system.open_url` and `haptics.vibrate` itself. `Module.haylen.register(method, handler)` adds or replaces a handler, `Module.haylen.unregister(method)` removes it and `Module.haylen.emit(event, payload)` sends an event. A handler receives the parsed parameters and returns the result or a promise for it. A thrown error or a rejected promise fails the call with the error message. The page registers its handlers before the runtime starts, for example in `Module.preRun`.

```html
<script>
    var Module = {
        canvas: document.getElementById("canvas"),
        preRun: [function () {
            Module.haylen.register("store.buy", async (params) => {
                if (!params.item) {
                    throw new Error("store.buy needs an item.");
                }
                const response = await fetch("/api/buy", { method: "POST", body: JSON.stringify(params) });
                return await response.json();
            });
            window.addEventListener("hashchange", () => Module.haylen.emit("app.link", { url: location.href }));
        }],
    };
</script>
```

## C++ handlers

C++ code registers handlers that run inside the engine with `engine.getPlatform().registerHandler(method, handler)`, where the handler receives the parameters as `haylen::core::Json` and a reply function that takes a `haylen::platform::Bridge::Result`. `engine.getPlatform().emit(event, payloadJson)` sends an event from any thread.

```cpp
engine.getPlatform().registerHandler("save.cloudSync", [](const haylen::core::Json& params, haylen::platform::Bridge::Reply reply) {
    reply({.ok = true, .value = {{"synced", params.value("slot", 0)}}});
});
engine.getPlatform().emit("app.link", R"({"url": "island://beach"})");
```
