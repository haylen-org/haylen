# 🌉 Platform bridge

This guide explains the platform bridge, the road between a Lua app and native code. It is for developers who write native code for an app or a plugin. That code is Java or Kotlin on Android, Objective-C or Swift on Apple platforms, and JavaScript on the web. It is C in native libraries, and C++ in apps that compile the engine. It shows how a call travels, how events, bytes, timeouts and cancellation work, and how each platform registers its handlers. The [`haylen.platform` reference](../lua-api/platform.md) lists the Lua functions, and the [native code guide](native.md) compares the bridge with native libraries and C++ plugins.

## What the bridge carries

The bridge carries two things:

- **Calls.** The app calls a named method with JSON parameters. A handler answers once with a JSON result, or with a typed error that has a message, a code and data. The answer arrives later, so a call never blocks the app. A call can time out, and the app can cancel it. The handler hears both.
- **Events.** Native code sends a named event with a JSON payload, and the app listens to it. An event can be retained for a listener that comes later, or batched into one list per frame.

Parameters, results and events carry byte buffers next to their JSON, such as images and audio, which never turn into text. Every result and every event reaches the app on the frame thread, at the start of a frame. Plugins also open screens through the bridge: native UI that covers the app until it ends with one result.

The bridge covers what the engine does not wrap, such as sign-in, purchases, sharing, deep links or system settings. The engine wraps some services itself, and they never go through the bridge: what the device is, opening urls and vibrating in [`haylen.system`](../lua-api/system.md), and native dialogs in [`haylen.dialogs`](../lua-api/dialogs.md). The bridge has no methods of its own, so every method name and event name is a contract between an app and its native code.

```lua
local async = require('async')
local platform = require('haylen.platform')

platform.on('app.link', function(payload)
    print('Opened from ' .. payload.url .. '.')
end)

async.spawn(function()
    local answer, err = platform.call('app.greet', {name = 'Ana'}, {timeout = 5}):await()
    print(answer and answer.message or err)
end)
```

## How a call travels

```text
Lua platform.call ─► Bridge::call ─► engine handler (C++ or Lua), answered at once
                                  ├► NativeApi::dispatch ─► handler of a native library
                                  └► Host::dispatchPlatformCall ─► Services::dispatch ─► platform handler
native reply (any thread) ─► BridgeRelay::resolve ─► Bridge::resolve ─► queue
next frame: Engine::frame ─► Bridge::pump ─► callback or Lua call, on the frame thread
```

Every step carries the JSON text of the payload together with its byte buffers, as [byte buffers](#byte-buffers) describes.

1. The function `platform.call(method, params, options)` reaches `platform::Bridge::call` in `engine/src/platform/Bridge.cpp`. The bridge gives the call a numeric id and keeps its callback. With a timeout it also keeps the deadline.
2. A handler registered inside the engine answers first. That is a C++ handler of `Bridge::registerHandler` or a Lua handler of `platform.registerHandler`. It runs during the call itself.
3. Otherwise the bridge serializes the parameters and hands `(id, method, paramsJson, buffers)` to the dispatcher of the engine. A handler that a native library registered through `HaylenNativeApi`, in `engine/src/platform/native/NativeApi.cpp`, answers next.
4. Otherwise the host gets the call through `Host::dispatchPlatformCall`. The native host, `platform::NativeHost`, forwards it to `platform::Services::dispatch`, which each platform folder under `engine/src/platform` implements: `HaylenBridge` on Android and Apple platforms and `Module.haylen` on the web. Windows and Linux have no registry of their own, as [desktop](#desktop) explains.
5. Native code answers once, from any thread. The answer goes through `platform::BridgeRelay::resolve` in `engine/src/platform/BridgeRelay.cpp` to the bridge of the running engine, which the platform plugin attaches when the app starts. The bridge parses the answer and queues it.
6. At the start of every frame, `Engine::frame` calls `Bridge::pump`. The pump runs the queued callbacks, the events and the work that native callbacks posted, on the frame thread. Then it polls the event loop of Varn, before the fixed update and the update.

A method that no handler answers fails with the code `noHandler`. The headless host of the engine tests and of `haylen-headless` has no native side. It records each call that no engine handler answers and leaves it pending, so a test answers it or the call waits, as [testing bridge code](#testing-bridge-code) shows.

### One frame per round trip

In Lua, the call that `platform.call` returns settles in the pump. A coroutine that awaits it resumes in the poll of the same frame. So an awaited round trip takes one frame when native code answers before the next frame starts. A result never arrives during the `platform.call` that asked for it, even when an engine handler answers at once.

Code that waits on many calls in a row pays one frame for each. Send them together and await them with `async.all` instead.

```lua
local async = require('async')
local platform = require('haylen.platform')

async.spawn(function()
    -- Both calls travel in the same frame, so the answers arrive together.
    local both = async.all({platform.call('profile.load', {id = 'me'}).promise, platform.call('store.products').promise}):await()
    print(both[1].name, #both[2])
end)
```

Every call gets an id that is unique in the whole process. So a native answer that arrives after the app restarted, for example after a hot reload, never answers a call of the new app, and the bridge drops it.

## Performance

The test `PLG-022` of the test project measures the bridge to the native part of the demo plugin. It measures four things. The first is the round trip of one call awaited after the other, and the second the calls per second of 2000 calls sent at once. The third is the work of a frame that sends N calls, and the fourth the work of the frames that receive N batched events 30 times per second. It logs every number with its code, so a run on a device, a simulator or a browser records them. These are the numbers of the Debug build of the test project:

| Measure | iOS simulator, Swift | Android emulator `Pixel_10a`, Kotlin | Chrome without a window, JavaScript |
| --- | --- | --- | --- |
| Frames per second of the run | 58 | 30 | 59 |
| Round trip of an awaited call | 16.67 ms | 34.98 ms | 16.66 ms |
| Calls per second, 2000 at once | 58824 | 25000 | 71429 |
| Frame work with 0, 10, 100 and 1000 calls | 0.56, 0.73, 0.78 and 2.16 ms | 0.70, 0.81, 1.00 and 4.80 ms | 0.90, 1.02, 1.36 and 2.49 ms |
| Frame work with 10, 100 and 1000 batched events 30 times per second | 0.33, 0.55 and 0.31 ms | 0.77, 0.91 and 1.37 ms | 0.65, 0.90 and 1.51 ms |

An awaited call costs one frame of the platform. The emulator draws without a window at about 30 frames per second, so its round trip is twice as long. The calls per second and the work of a frame measure the bridge itself. Batched events cost little whatever their number, because the events of one name reach their listener as one list per frame.

## Events

Native code sends events with `emit` and the app listens with `platform.on`. Events take the same road as answers: `BridgeRelay::emit(event, payloadJson, buffers, options)` queues the event, and `pump` delivers it to every listener of its name. An event with invalid JSON, or whose JSON refers to a buffer it lacks, is logged and dropped. An event that arrives while no app runs is dropped, and so is an event that nothing listens to, unless it is retained.

### Retained events

Some events come before the app listens: the deep link or the notification that opened the app, a purchase that finished while it was closed, or the result of a sign-in that restores itself at launch. Native code sends such events retained.

A retained event that nothing listens to waits in the bridge, up to `Bridge::kRetainedLimit`, which is 32, per name. The oldest is dropped first. The first `on` of its name receives the waiting events in order at the next pump, before any newer event of that name. The events wait in the bridge of the running app, so a restarted app starts with none. The bridge itself drops an event that reaches the engine while no app runs, even a retained one. So the Android library and the Apple runtime keep the events that native code sends while no app runs, such as while the app launches. They hand them to the next app in order, and an event sent retained before any app ran still reaches its first listener.

### Batched events

Native code that reports something many times per frame, such as a sensor, a location or the progress of a download, marks its events batched. The batched events of one name that reach the bridge before a pump arrive at the listeners once. They arrive as one event whose payload lists their payloads in order, at the place of the first of them.

- In Lua the listener receives a list of payloads.
- In C++ the listener receives a `Bridge::Payload` whose JSON is an array. Its buffers are the buffers of the events in order, and its references point at them.

Events of the same name without the flag still arrive on their own. A batch is retained when any of its events is. It waits as the list of its frame, up to 32 lists per name, and the first listener receives each list in order.

### Where the flags go

Every way to send an event takes both flags:

| Who sends | Retained | Batched |
| --- | --- | --- |
| C++ | The `retain` field of `Bridge::EmitOptions` for `Bridge::emit` and `BridgeRelay::emit`. | The `batched` field of `Bridge::EmitOptions`. |
| A native library, C | `HAYLEN_NATIVE_EMIT_RETAIN` in the flags of `emit` of `HaylenNativeApi`. | `HAYLEN_NATIVE_EMIT_BATCHED` in the same flags. |
| Android | `HaylenBridge.emit(event, payload, retain)`, and `emitRetained` of a plugin context. | `HaylenBridge.emit(event, payload, retain, batched)`, and `emit(event, payload, retain, batched)` of a plugin context. |
| Apple, Objective-C | `+emit:payload:retain:` of `HaylenBridge`, and `emitRetained:payload:` of a plugin context. | `+emit:payload:retain:batched:` of `HaylenBridge` and of a plugin context. |
| Apple, Swift | `try HaylenBridge.emit(event, payload, retain: true)`, and `try context.emit(event, payload, retain: true)`. | The argument `batched: true` of the same functions. |
| Web | `Module.haylen.emit(event, payload, {retain: true})`, and `context.emit` of a plugin module. | `{batched: true}` in the same options. |
| Lua, in tests | `platform.emit(event, payload, {retain = true})`. | `{batched = true}` in the same options. |

## Byte buffers

A payload is JSON with an ordered list of byte buffers next to it. The JSON refers to buffer N with the object `{"$bytes": N}`, an object whose only key is `$bytes` with an integer of 0 or more. A buffer can appear anywhere in the JSON, and more than once. So binary data, such as a photo, a recording or a file, crosses the bridge as bytes, never as base64 text. This works in the parameters of a call, in its result and in an event, retained and batched ones included.

Every entry point is safe from any thread, and each copies the bytes once, into the engine or out of it. Each language uses its own byte type:

| Where | Bytes that the app sends | Bytes that native code sends |
| --- | --- | --- |
| Lua | A string marked with [`platform.bytes(data)`](../lua-api/platform.md#platformbytesdata), since plain strings cross as text. | A Lua string in the place of the reference. |
| C++ | The buffers of `Bridge::Payload{json, buffers}`, which are `std::vector<std::byte>`, for `call`, `send`, handlers and listeners. | The buffers of `Bridge::resolve` and `Bridge::emit`. |
| C | A `const HaylenNativeBuffer*` array that a handler receives, valid while it runs. | The `HaylenNativeBuffer` arrays of `resolve` and `emit` of `HaylenNativeApi`. |
| Java and Kotlin | A `byte[]` value, `ByteArray` in Kotlin, in the `JSONObject` or `JSONArray` of the parameters. | `byte[]` and `ByteBuffer` values anywhere in the value of `reply.success` and of `emit`. A direct `ByteBuffer` crosses JNI without a copy in Java. |
| Objective-C and Swift | An `NSData` value, `Data` in Swift, in the parameters. | `NSData` and `Data` values anywhere in the value of a reply and of `emit`. |
| JavaScript | A `Uint8Array` value in the parameters. | `ArrayBuffer`, `Uint8Array` and every other `ArrayBuffer` view anywhere in the value of a handler and of `emit`. They are copied into wasm memory. |

JSON that refers to a buffer it lacks fails a call with the code `invalidBytes` and a message such as `The JSON refers to byte buffer 2, but only 1 came with it.`. The same JSON drops an event with an error in the log, and it makes `Bridge::call` and `Bridge::send` throw `std::invalid_argument`. A failure carries JSON alone, so the buffers of a failure are dropped. The Swift helpers for `Codable` values carry JSON alone too, so a Swift handler that takes or returns bytes uses `registerHandler` with dictionaries.

```lua
local async = require('async')
local platform = require('haylen.platform')

async.spawn(function()
    -- The string goes out as a buffer, and the buffer that comes back is a Lua string.
    local echoed = platform.call('photos.echo', {image = platform.bytes('\137PNG')}):await()
    print(#echoed.image)
end)
```

## Timeouts and cancellation

A call with a `timeout` fails with the code `timeout` at the first pump after its deadline. The method `call:cancel()` fails a pending call with the code `cancelled` at the next pump. In both cases the bridge tells the native side that the app gave the call up, through `Bridge::Canceller`:

- A handler of a native library hears it through its `HaylenNativeCancel` function.
- A platform handler hears it through `Host::cancelPlatformCall` and `Services::cancel`. These reach `HaylenBridge.cancel` on Android, the cancel block of a cancellable handler on Apple platforms and the `AbortSignal` of the handler on the web.
- Windows and Linux fail the calls that reach them during the call itself, so no call of theirs is ever pending.

An answer that comes after a call timed out or was cancelled is dropped, like a second answer to the same call. A call without a timeout waits for its answer as long as the app runs, so every handler answers exactly once. When the app stops or restarts, the bridge tells the native side of every call that is still pending that it was given up, the way `call:cancel()` does. So native work, such as a watch that streams events, stops with the app that started it.

## Screens

Plugins also open screens: native UI that takes over the app until it ends with one result, such as a paywall, a sign-in flow or a payment page. A screen travels like a call, with a cover around it.

```text
Lua handle:openScreen ─► Screens::open ─► ScreenRelay::show, or busy and notActive at the next pump
next frame: Engine::frame ─► cover ─► Screens::present ─► NativeApi, a screen of a native library
                                                        └► Host::openScreen ─► Services::openScreen ─► platform screen
native end (any thread) ─► ScreenRelay::finish ─► queue
next pump: Screens::pump ─► the call, or <plugin>.screenRestored for the next app
```

The engine covers the app before the platform shows the screen, and keeps it covered until the screen ends, however it ends. It draws nothing while an opaque screen shows. The screen that shows belongs to the process. So an app that restarts under it starts covered and receives its end as the retained event `<plugin>.screenRestored`, with the state that the earlier app gave. The platform keeps the pending screen where it survives the end of the process, such as the session storage of a page that leaves for a redirect. It hands the end to `ScreenRelay::restore` when the app starts again. A cancel or a timeout fails the call at once and asks the platform through `Host::cancelScreen` to dismiss the screen, whose end then reaches no app. The [plugin guide](plugins.md#plugin-screens) describes how screens work and how each platform opens them, and the [reference](../lua-api/platform.md#screens) lists the options and the codes.

## The JSON contract

- Parameters are a JSON value with its byte buffers. When Lua passes none, they are an empty object. The [reference](../lua-api/platform.md#values-that-cross-the-bridge) lists how Lua values become JSON.
- A successful result is any JSON value with its byte buffers. A JSON `null` reaches Lua as `nil`.
- A failure carries a JSON string, which becomes the error message, or an object with a `message` string and optional `code` and `data` values, which Lua receives as the fields of the error.
- Any other failure payload, such as `null`, a number or an object without a string `message`, fails with `The native platform call failed without a message.`. It keeps the code and the data of an object.
- An answer that is not JSON at all fails with the code `invalidJson` and `The platform returned invalid JSON.`.
- The bridge and the platforms fail calls with codes of their own. The bridge gives `timeout` and `cancelled`, and `noHandler` comes when nothing answers the method. The codes `invalidJson` and `invalidBytes` come for broken answers. The code `exception` comes when a Java, Kotlin, Swift or JavaScript handler threw an error without a code of its own. The [reference](../lua-api/platform.md#platformerror) describes each one.
- Method and event names are free-form strings, usually dotted names such as `auth.signIn`. [Plugins](plugins.md) put their id in front of their method and event names, which use camelCase, such as `ads.showBanner` and `ads.closed`. The plugin handle of Lua and the plugin contexts of native code add the prefix, so neither side writes it.

## Stand-ins in Lua

The function `platform.registerHandler` answers a method with a Lua function inside the engine, which goes before every native handler. Desktop builds and tests use it to stand in for a method that a phone answers natively. The function `platform.hasHandler` cannot see native handlers. So an app decides by `haylen.platform` where to register a stand-in, as the [complete example](#complete-example) does. A stand-in registered on every platform would hide the native handler everywhere.

## Android

The Android library module `engine/platform/android/haylen` holds `dev.haylen.HaylenActivity`, the GameActivity that runs the app, and `dev.haylen.HaylenBridge`, the registry of handlers.

| Member | Meaning |
| --- | --- |
| `HaylenBridge.register(String method, MethodHandler handler)` | Adds or replaces the handler of a method. The handler runs on the main thread. |
| `HaylenBridge.register(String method, MethodHandler handler, HaylenBridge.Threading threading)` | The same, with the thread the handler runs on: `MAIN` or `BACKGROUND`. |
| `HaylenBridge.unregister(String method)` | Removes a handler. |
| `HaylenBridge.emit(String event, Object payload)` | Sends an event to the app. The forms with `boolean retain` and `boolean retain, boolean batched` send it [retained](#retained-events) or [batched](#batched-events). |
| `HaylenBridge.activity()` | The running `HaylenActivity`, an `AppCompatActivity` and so a `ComponentActivity`, or `null` when there is none. |
| `MethodHandler.handle(Object params, Reply reply) throws Exception` | The interface of a handler, usable as a lambda. |
| `Reply.success(Object value)` | Answers the call. |
| `Reply.failure(String message)` | Fails the call with a message. |
| `Reply.failure(String message, String code, Object data)` | Fails the call with a code and data, both optional. |
| `Reply.failure(Throwable error)` | Fails the call with a thrown error. A `HaylenBridge.Failure` keeps its code and data, and any other error fails with the code `exception`. |
| `Reply.isCancelled()` | Whether the app cancelled the call or its timeout passed. |
| `Reply.onCancel(Runnable listener)` | Runs the listener on the main thread when the app gives the call up, or at once when that already happened. |
| `HaylenBridge.Failure(String message, String code, Object data)` | An exception that a handler throws to fail its call with a code and data. |
| `HaylenCoroutines.register(String method, suspend (Any?) -> Any?)` | Adds a Kotlin handler written as a suspending function. It comes with the `dev.haylen:haylen-coroutines` library. |

### How a call runs on Android

1. The engine calls the static `HaylenBridge.dispatch(long, byte[], byte[], byte[][])` through JNI on the frame thread. It passes the method name and the parameters as UTF-8 bytes, and the byte buffers of the parameters. Every text crosses JNI as UTF-8 bytes in both directions. The JNI string functions use a modified UTF-8 that breaks characters outside the Basic Multilingual Plane, such as emoji.
2. A method without a handler fails with the code `noHandler` and `No native handler is registered for "<method>".`. Otherwise the frame thread only posts the call to the thread of the handler and goes on.
3. The handler runs on the main thread, or on the one background thread that every handler of `HaylenBridge.Threading.BACKGROUND` shares, in the order the calls arrive. A main thread handler whose activity was destroyed in the meantime fails its call with `The activity was destroyed before "<method>" ran.`. Background handlers run without an activity.
4. The parameters are parsed with `org.json.JSONTokener` on the thread of the handler. So an object arrives as a `JSONObject`, and every reference to a buffer becomes a `byte[]` value in its place. Parameters that do not parse fail the call.
5. The handler replies at once or later, from any thread. The method `success` takes `null`, strings, numbers, booleans, `JSONObject`, `JSONArray`, maps, collections and arrays, which it converts with `JSONObject.wrap`. It takes `byte[]` and `ByteBuffer` values anywhere inside, which cross JNI as buffers. A direct `ByteBuffer` crosses with its remaining bytes without a copy in Java. A value that is not JSON, such as `NaN`, fails the call. The first answer counts, and later ones are dropped.
6. When the app cancels the call or its timeout passes, the engine calls the static `HaylenBridge.cancel(long)` through JNI. It marks the reply cancelled, drops the answer that may still come and runs the `onCancel` listeners on the main thread. A call cancelled before its handler ran never runs it.

The native side attaches each of its threads to the Java VM the first time the thread calls Java, and detaches it when the thread ends, so no call pays for an attachment. An exception that a Java method throws into any call of the engine never stays pending on the frame thread. The engine describes it to logcat, clears it and logs an error that names the method. A `dispatch` that threw fails its call with the code `exception`.

A handler that throws, a checked exception included, fails its call through `reply.failure(Throwable)` instead of crashing the app. So an unexpected `NullPointerException` becomes a failed call with the code `exception` and the class name in `data.type`. Errors of the virtual machine, such as `OutOfMemoryError`, still end the app.

The method `HaylenBridge.emit` works from any thread at any time. It converts its payload like `success`, and it logs and drops a payload that is not JSON. While no app runs, the library keeps the events, up to 32 per name with the oldest dropped first, and hands them to the next app in order once it starts. That covers the time before the first activity loads the native library, while an app restarts and between two activities.

R8 keeps the classes that the engine reaches by name through the `consumer-rules.pro` of the module, so handlers need no rules of their own.

### Where Android handlers register

[Plugins](plugins.md#the-android-part) register their handlers through their context, which puts the id of the plugin in front of the name. Apps register theirs once, in `Application.onCreate` of the application class, before the first activity starts. The `<application>` of the manifest of the [Android project](../tools/distribution.md#the-android-project) names that class. The Android template gives it as `App` in `app/src/main/kotlin/com/haylen/app/App.kt`, with the handler of the [bridge example](../tools/distribution.md#the-bridge-example) of the templates.

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
                throw new HaylenBridge.Failure("The method store.buy needs an item.", "missingItem", null);
            }
            reply.success(new JSONObject().put("item", item).put("receipt", "play-store-token"));
        });
    }

    public static void onDeepLink(String url) throws Exception {
        HaylenBridge.emit("app.link", new JSONObject().put("url", url), true);
    }
}
```

The sign-in of Tiny Island is a real handler of this kind. The local plugin `samples/games/tiny-island/plugins/sign-in` has a `SignInPlugin` that answers `sign-in.signIn` with Credential Manager. It builds the credential option of the identity service with the web client id from the `clientId` parameter of the plugin. It calls `getCredentialAsync` on the main executor of the activity and replies with `idToken`, `email`, `name` and `picture`. Without a client id it fails with a message that names the key of `app.json` to set.

### Kotlin coroutines

`HaylenCoroutines.register(method) { params -> result }` registers a Kotlin handler written as a suspending function. Each call runs in a coroutine of a scope on `Dispatchers.Main.immediate`, so it runs on the main thread. The return value answers the call. A thrown exception fails it like `failure(throwable)`. The coroutine is cancelled when the app cancels the call or its timeout passes.

`HaylenCoroutines` lives in `engine/platform/android/haylen-coroutines/src/main/kotlin/dev/haylen/HaylenCoroutines.kt`, in the `dev.haylen:haylen-coroutines` library. That library depends on `kotlinx-coroutines-android` as an API, so only apps and plugins that depend on it get coroutines. An app with such handlers depends on it in `app/build.gradle.kts` of its Android project, and a plugin in its own module, as the local plugin [`native-sample`](../../samples/tests/plugins/native-sample) of the test project does.

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
                throw HaylenBridge.Failure("The method profile.load needs an id.", "missingId", null)
            }
            delay(100)
            mapOf("id" to id, "name" to "Player")
        }
    }
}
```

## Apple platforms

The file `engine/include/haylen/platform/apple/HaylenBridge.h` declares the registry for macOS, iOS and tvOS. The file `engine/src/platform/apple/HaylenBridge.mm` implements it on top of `AppleBridge`, the table of handlers of `engine/src/platform/apple/AppleBridge.mm`.

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
+ (void)emit:(NSString*)event payload:(nullable id)payload retain:(BOOL)retain;
+ (void)emit:(NSString*)event payload:(nullable id)payload retain:(BOOL)retain batched:(BOOL)batched;
@end
```

- **Parameters.** They are parsed with `NSJSONSerialization`. So an object arrives as an `NSDictionary`, and every reference to a buffer becomes an `NSData` value in its place. Parameters that do not parse arrive as an empty dictionary.
- **Threads.** Handlers run on the main queue and may reply later from any thread. The table of handlers exists from the first registration, so native code may register at any time, even before the app starts.
- **Success.** The call `reply(YES, result)` succeeds with `nil` or any value that `NSJSONSerialization` accepts, with `NSData` values anywhere inside, which cross as buffers. A value that it rejects fails the call with `The native handler for "<method>" returned a value that is not JSON.`.
- **Failure.** The call `reply(NO, failure)` fails with a message string, or a dictionary with a `message` string and optional `code` and `data`. Anything else fails with `The native handler for "<method>" failed.`. The first answer counts, and later ones are dropped.
- **Cancellation.** A cancellable handler returns a block, or `nil`. The block runs on the main queue when the app cancels the call or its timeout passes, and an answer after that is dropped. A call cancelled before its handler ran never runs it.
- **No handler.** A method without a handler fails with the code `noHandler` and `No native handler is registered for "<method>".`.
- **Events.** The method `emit:payload:` sends an event from any thread, with `NSData` values as buffers. A payload that `NSJSONSerialization` rejects is logged as an error and dropped. Events sent while no app runs, such as while the app launches, before its first frame or while it restarts, wait in the runtime and reach the next app in order once it starts. So the link or the notification that opened the app is never lost.
- **Plugins.** The context of a [plugin](plugins.md#the-apple-context) registers and emits under the id of the plugin. So `[context registerHandler:@"show" ...]` of the plugin `ads` answers `ads.show`, and `[context emitRetained:@"closed" payload:nil]` sends `ads.closed`.

```objc
#import "haylen/platform/apple/HaylenBridge.h"

@interface StorePlugin : NSObject
@end

@implementation StorePlugin

+ (void)load {
    [HaylenBridge registerHandler:@"store.buy" handler:^(id params, HaylenReply reply) {
        NSString* item = [params isKindOfClass:NSDictionary.class] ? params[@"item"] : nil;
        if (item == nil) {
            reply(NO, @{@"message" : @"The method store.buy needs an item.", @"code" : @"missingItem"});
            return;
        }
        reply(YES, @{@"item" : item, @"receipt" : @"app-store-token"});
    }];
}

@end
```

```objc
// The link that opened the app arrives before the app listens, so it waits for the first listener of app.link.
[HaylenBridge emit:@"app.link" payload:@{@"url" : url.absoluteString} retain:YES];
```

### Swift

Swift handlers use the helper `HaylenBridge.register` of `HaylenBridgeAsync.swift` of the engine, in the form `HaylenBridge.register(method) { (params: Params) async throws -> Result in ... }`. `Params` is `Decodable`, and the helper decodes the parameters with `JSONDecoder`. `Result` is `Encodable`, and the helper encodes it with `JSONEncoder`. The handler runs on the main actor. A thrown `HaylenFailure(message, code:, data:)` fails the call with its code and data, and any other error fails it with the code `exception`. The task of the call is cancelled when the app gives the call up. The call `try HaylenBridge.emit(event, payload, retain: false)` sends an event with an `Encodable` payload. Plugin contexts have the same helpers, as the [plugin guide](plugins.md#swift-helpers) describes.

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
                throw HaylenFailure("The method profile.load needs an id.", code: "missingId")
            }
            try await Task.sleep(nanoseconds: 100_000_000)
            return Profile(id: request.id, name: "Player")
        }
    }
}
```

The engine ships `HaylenBridgeAsync.swift` as a source file. The tool `haylen.py` writes it into `haylen/` of the project, and the target templates compile it. The engine artifact is a static library of C, C++ and Objective-C, whose headers Swift reaches through `source/HaylenBridging.h`. A Swift module inside the artifact would have to match the Swift compiler of each app, so the helpers stay a source file. The target names its module `HaylenApp`, so Objective-C code such as `main.m` calls Swift classes through `#import "HaylenApp-Swift.h"`.

### Where Apple handlers register

Apps made from the Apple template register in the `main` function of `source/main.m`, before it calls `haylen_main`. Their Objective-C, C++ and Swift files sit next to it in `platform/apple/source/` of the app, and every file there builds into every target, as the [distribution guide](../tools/distribution.md#the-apple-project) describes. The template registers its Swift handlers of `AppBridge.swift` from there.

Apps built with `haylen_add_app` add their Objective-C file with the `SOURCES` argument. They compile it with `-fobjc-arc`, as the engine compiles its Apple sources, and register from `+load` or once the app has launched, as `src/AppBridge.mm` of the C++ template does.

## Web

The page side of the bridge lives in `engine/platform/web/haylen-runtime.js`. Every web target links it with `--pre-js`, and it fills `Module.haylen`:

| Function | Meaning |
| --- | --- |
| `Module.haylen.register(method, handler)` | Adds or replaces the handler of a method. |
| `Module.haylen.unregister(method)` | Removes a handler. |
| `Module.haylen.emit(event, payload, options)` | Sends an event to the app. The payload is converted with `JSON.stringify`, with `ArrayBuffer` and `Uint8Array` values as bytes. The option `retain` keeps the event for the first listener of its name, and `batched` sends it in the list of its frame. Events sent before the first app starts reach it once it starts. |
| `Module.haylen.createPluginContext(id, config)` | Makes the context that the web module of a plugin receives. Its `register` and `emit` put the id of the plugin in front of the name, as the [plugin guide](plugins.md#web-modules) describes. |

The runtime calls a handler after the frame that made the call. The handler receives two arguments:

- The parsed parameters, where bytes of the app arrive as `Uint8Array` values.
- A context with the `call` id and a `signal`, an `AbortSignal` that aborts when the app cancels the call or its timeout passes.

The handler returns the result or a promise of it. `ArrayBuffer` and `Uint8Array` values in the result cross as bytes. A thrown error or a rejected promise fails the call with the message of the error and its `code` and `data` properties. An error without a code fails with the code `exception` and the `name` of the error in `data.type`. The first answer counts, and an answer after a cancel is dropped. A call cancelled in the frame that made it never runs its handler. A method without a handler fails with the code `noHandler` and `No page handler is registered for "<method>".`.

The web build has one thread, so handlers run on the main thread of the browser between frames. An asynchronous handler, such as one that waits for `fetch` or for a sign-in popup, never blocks the app while it waits.

The function `register` exists once the runtime script has run, so a page registers its handlers in `Module.preRun`. The web template runs the `app.js` of an app for that, and a C++ app made from the C++ template links the same file into its runtime script with `--pre-js`. Plugins register theirs with `context.register` of their web module. One such module is the sign-in of Tiny Island, which answers `sign-in.signIn`. It loads the script of the identity service only when the player first signs in and reads the client id from the parameters of the plugin. It resolves with `idToken`, `email`, `name` and `picture`, decoded from the credential that comes back. The development server sends no embedder policy, so the script loads. A site that serves the page with `Cross-Origin-Embedder-Policy: require-corp` blocks it, as the [distribution guide](../tools/distribution.md#serve) explains.

```html
<script>
    var Module = {
        canvas: document.getElementById("canvas"),
        preRun: [function () {
            Module.haylen.register("store.buy", async (params, context) => {
                if (!params.item) {
                    throw Object.assign(new Error("The method store.buy needs an item."), { code: "missingItem" });
                }
                const response = await fetch("/api/buy", { method: "POST", body: JSON.stringify(params), signal: context.signal });
                return await response.json();
            });
            window.addEventListener("hashchange", () => Module.haylen.emit("app.link", { url: location.href }));
        }],
    };
</script>
```

## Desktop

macOS uses the Apple registry above. Windows and Linux have no registry in the language of the platform. So their services fail every call that reaches them with the code `noHandler`, at once on the frame thread, and the failure still reaches the app at the start of the next frame. Apps add methods on these platforms with the handlers of [native libraries](#native-library-handlers), which a Lua app loads without compiling the engine, or with C++ or Lua handlers.

## Native library handlers

A native library answers methods in C on every platform that loads native libraries. The function `native.load(name, {init = 'symbol'})` hands the init function of the library the `HaylenNativeApi` of `haylen/platform/native/HaylenNative.h`. Through it the library can:

- register handlers with an optional cancel function, which receive the parameters with their byte buffers, and answer them from any thread with `resolve`;
- send events from any thread with `emit`, retained or batched;
- write to the engine log, declare itself the native part of a plugin and hear the errors that stop the app;
- feed the [streams](plugins.md#streams) of a plugin, open the [screens](plugins.md#desktop-screens) of a plugin over the window of the app and cover the app.

The handlers belong to the process. They answer after the engine handlers and before the platform handlers. The [native code guide](native.md#libraries-that-talk-to-the-app) shows a library, and the [`haylen.native` reference](../lua-api/native.md#the-c-interface-of-the-engine) lists every entry.

## C++ handlers and calls

The class `haylen::platform::Bridge` in `engine/include/haylen/platform/Bridge.hpp` is the whole bridge, and `engine.getPlatform()` returns the running one.

| Member | Meaning |
| --- | --- |
| `registerHandler(std::string method, Bridge::Handler handler)` | Answers a method inside the engine. The handler receives the `const Bridge::Payload& params`, JSON with its `std::vector<std::byte>` buffers, and a `Bridge::Reply`. The reply takes a `Bridge::Result{ok, value, error}`, whose `value` is a `Bridge::Payload` and whose `Bridge::Error` has a `message`, a `code` and `data`. An empty method name throws `A platform handler needs a method name and a function.`. |
| `hasHandler(std::string_view method)` | Whether an engine handler answers the method. |
| `call(std::string_view method, const Bridge::Payload& params, Bridge::Callback callback, timeout)` | Calls a method and returns the id of the call. The callback receives the `Bridge::Result` on the frame thread. An optional `std::chrono::steady_clock::duration` fails the call with the code `timeout` once it passes. |
| `cancel(std::uint64_t id)` | Fails a pending call with the code `cancelled` at the next pump, tells native code, and returns whether the call was pending. |
| `getMailbox()` | A `Bridge::Mailbox`, whose `post` queues work for the frame thread from any thread until the bridge is gone. Native callbacks deliver their calls through it. |
| `on(const std::string& event, std::function<void(const Bridge::Payload&)> listener)` | Listens to native events and returns a `Connection`. |
| `send(std::string_view method, const Bridge::Payload& params)` | Calls a method whose answer nobody needs, without a pending call. |
| `resolve(std::uint64_t id, bool ok, std::string_view resultJson, buffers = {})` | Answers a call with the byte buffers of its JSON. It is safe from any thread. |
| `emit(std::string_view event, std::string_view payloadJson, buffers = {}, const Bridge::EmitOptions& options = {})` | Sends an event with the byte buffers of its JSON, from any thread. With `options.retain` it waits for the first listener of its name, and with `options.batched` it arrives in the list of its frame. |
| `getPendingCallCount()` | The calls that still wait for an answer. |

The engine gives C++ code two more services next to the bridge. The function `engine.getAppPlugins()` returns the plugins of `app.json` as `haylen::platform::AppPlugin` records with `id`, `version`, `config` and `native`. The function `engine.getScreens()` returns the `haylen::platform::Screens` that open the screens of plugins, as the [plugin guide](plugins.md#screens-in-c) describes.

A plugin is the usual home for C++ handlers, because its `start` runs with every new engine:

```cpp
class CloudSavePlugin final : public haylen::plugins::Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "cloudSave";
    }

    void start(haylen::core::Engine& engine) override {
        engine.getPlatform().registerHandler("save.cloudSync", [](const haylen::platform::Bridge::Payload& params, haylen::platform::Bridge::Reply reply) {
            if (!params.json.contains("slot")) {
                reply({.error = {.message = "The method save.cloudSync needs a slot.", .code = "missingSlot"}});
                return;
            }
            reply({.ok = true, .value = {.json = {{"synced", params.json.at("slot")}}}});
        });
    }
};
```

A C++ application adds it with `engine.addPlugin(std::make_unique<CloudSavePlugin>())`, and Lua then calls `platform.call('save.cloudSync', {slot = 2})`. A handler may keep the reply and answer later from another thread. It must answer before the engine stops, because the reply belongs to the bridge of that engine. C++ code sends events and calls methods the same way Lua does:

```cpp
engine.getPlatform().emit("app.link", R"({"url": "island://beach"})");
engine.getPlatform().emit("motion.reading", R"({"x": 1, "y": 2})", {}, {.batched = true});
engine.getPlatform().call("profile.load", {.json = {{"id", "me"}}}, [](haylen::platform::Bridge::Result result) {
    if (!result.ok) {
        haylen::core::Log::warning("The method profile.load failed: {}", result.error.message);
        return;
    }
    haylen::core::Log::info("Playing as {}", result.value.json.value("name", std::string("a guest")));
});
```

Bytes travel next to the JSON, which refers to each buffer with `haylen::core::JsonBytes::makeReference(index)`. The function `JsonBytes::findReference` reads a reference back:

```cpp
#include "haylen/core/JsonBytes.hpp"

const std::vector<std::uint8_t> png = engine.getPackage().readAsset("photos/beach.png");
std::vector<std::byte> photo(reinterpret_cast<const std::byte*>(png.data()), reinterpret_cast<const std::byte*>(png.data()) + png.size());
engine.getPlatform().call("gallery.save", {.json = {{"image", haylen::core::JsonBytes::makeReference(0)}, {"album", "Island"}}, .buffers = {std::move(photo)}}, [](haylen::platform::Bridge::Result result) {
    if (result.ok) {
        const std::size_t thumbnail = *haylen::core::JsonBytes::findReference(result.value.json.at("thumbnail"));
        haylen::core::Log::info("The thumbnail takes {} bytes", result.value.buffers[thumbnail].size());
    }
});
```

## Complete example

This example adds the method `accessibility.reducedMotion`, which tells the app whether the player asked the system for less motion, and the event `accessibility.reducedMotionChanged`. Android and the web answer natively. The app turns its camera shake off when the answer says so, and a Lua stand-in answers on the other platforms.

### Android

The file `app/src/main/java/com/haylen/app/MotionPlugin.java` in the [Android project](../tools/distribution.md#the-android-project) of the app:

```java
package com.haylen.app;

import android.content.ContentResolver;
import android.database.ContentObserver;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import dev.haylen.HaylenBridge;
import java.util.Collections;

// Tells the app whether the player turned the animations of Android off, and when that changes.
final class MotionPlugin {
    private MotionPlugin() {}

    static void register(ContentResolver resolver) {
        HaylenBridge.register("accessibility.reducedMotion", (params, reply) -> reply.success(Collections.singletonMap("reduced", isReduced(resolver))), HaylenBridge.Threading.BACKGROUND);
        resolver.registerContentObserver(Settings.Global.getUriFor(Settings.Global.ANIMATOR_DURATION_SCALE), false, new ContentObserver(new Handler(Looper.getMainLooper())) {
            @Override
            public void onChange(boolean selfChange) {
                HaylenBridge.emit("accessibility.reducedMotionChanged", Collections.singletonMap("reduced", isReduced(resolver)));
            }
        });
    }

    private static boolean isReduced(ContentResolver resolver) {
        return Settings.Global.getFloat(resolver, Settings.Global.ANIMATOR_DURATION_SCALE, 1f) == 0f;
    }
}
```

The application class of the Android template, `app/src/main/kotlin/com/haylen/app/App.kt`, registers it:

```kotlin
package com.haylen.app

import android.app.Application

class App : Application() {
    override fun onCreate() {
        super.onCreate()
        MotionPlugin.register(contentResolver)
    }
}
```

The handler reads a setting and touches no view, so it runs on the background thread of the bridge. The observer and `Collections.singletonMap` work from API 27, the minimum of the engine. The event reaches the app even when the player changes the setting while the app is in the background.

### Web

The shell of the app, which `haylen_add_app` takes as `WEB_SHELL`, registers the handler in `preRun`:

```html
<canvas id="canvas" tabindex="-1" oncontextmenu="event.preventDefault()"></canvas>
<script>
    function registerMotion() {
        const query = window.matchMedia("(prefers-reduced-motion: reduce)");
        Module.haylen.register("accessibility.reducedMotion", () => ({ reduced: query.matches }));
        query.addEventListener("change", () => Module.haylen.emit("accessibility.reducedMotionChanged", { reduced: query.matches }));
    }

    var Module = {
        canvas: document.getElementById("canvas"),
        preRun: [registerMotion],
        haylen: {
            packageUrl: new URLSearchParams(location.search).get("package"),
        },
    };
</script>
{{{ SCRIPT }}}
```

### Lua

The file `source/main.lua`:

```lua
local async = require('async')
local haylen = require('haylen')
local platform = require('haylen.platform')

local motion = {shake = true}

-- Only Android and the web answer accessibility.reducedMotion natively, so a Lua handler stands in elsewhere.
if haylen.platform ~= 'android' and haylen.platform ~= 'web' then
    platform.registerHandler('accessibility.reducedMotion', function()
        return {reduced = false}
    end)
end

local function applyMotion(answer)
    motion.shake = not answer.reduced
end

async.spawn(function()
    local answer, err = platform.call('accessibility.reducedMotion'):await()
    if answer then
        applyMotion(answer)
    else
        print('The method accessibility.reducedMotion failed: ' .. err)
    end
end)

platform.on('accessibility.reducedMotionChanged', applyMotion)
```

## Testing bridge code

The headless host of the engine tests records every call that reaches native code, and a test answers it through the bridge:

```cpp
ASSERT_TRUE(fixture.frameUntil([&] { return fixture.host().getPlatformCalls().size() == 1; }));
fixture.engine().getPlatform().resolve(fixture.host().getPlatformCalls()[0].id, true, R"({"model": "phone"})");
```

The function `platform::HeadlessHost::getPlatformCalls()` returns the id, the method, the JSON parameters and the byte buffers of each call. The function `getCancelledCalls()` returns the ids of the calls that the bridge gave up through a timeout or a cancel. A test answers with buffers the way native code does, such as `resolve(id, true, R"({"png": {"$bytes": 0}})", {pngBytes})`, and sends batched events with `emit(event, json, {}, {.batched = true})`. Lua code under test can stand in for native methods with `platform.registerHandler`. The native interop tests load `engine/tests/native/NativeTest.c`, whose init function registers handlers through `HaylenNativeApi`. The [testing guide](../tools/testing.md) describes the whole test support.
