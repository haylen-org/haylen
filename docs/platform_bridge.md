# Platform bridge

The platform bridge connects an app to native code. An app calls a named method with JSON parameters and receives its JSON result asynchronously, or a typed error with a message, a code and data, and native code sends named events with JSON payloads that the app listens to. Parameters, results and events carry byte buffers next to their JSON, such as images and audio, which never turn into text, and events can arrive retained for a later listener or batched into one list per frame. Calls time out and cancel, and the native handler hears about it. Every result and every event reaches the app on the frame thread, at the start of a frame. Plugins open screens through it as well, native UI that covers the app until it ends with one result. The bridge covers everything the engine does not wrap itself, such as sign-in, purchases, sharing, deep links or system settings, while what the device is, opening urls, vibrating and native dialogs are engine services of [haylen.system](lua-api/system.md) and [haylen.dialogs](lua-api/dialogs.md). Handlers written in Java or Kotlin on Android, Objective-C or Swift on Apple platforms, JavaScript on the web, C in native libraries, and C++ or Lua anywhere.

This guide explains how the bridge works and how each platform implements methods. The Lua functions are documented in the [haylen.platform reference](lua-api/platform.md), the [native code guide](native.md) compares the bridge with native libraries called through FFI and with C++ plugins, and the [architecture guide](architecture.md) places the bridge among the other engine systems.

## How a call travels

```text
Lua platform.call ─► Bridge::call ─► engine handler (C++ or Lua), answered at once
                                  ├► NativeApi::dispatch ─► handler of a native library
                                  └► Host::dispatchPlatformCall ─► Services::dispatch ─► platform handler
native reply (any thread) ─► BridgeRelay::resolve ─► Bridge::resolve ─► queue
next frame: Engine::frame ─► Bridge::pump ─► callback or Lua call, on the frame thread
```

Every step carries the JSON text of the payload together with its byte buffers, as [byte buffers](#byte-buffers) describes.

1. `platform.call(method, params, options)` reaches `platform::Bridge::call` (`engine/src/platform/Bridge.cpp`), which gives the call a numeric id, keeps its callback and, with a timeout, its deadline.
2. A handler registered inside the engine answers first. Those are C++ handlers added with `Bridge::registerHandler` and Lua handlers added with `platform.registerHandler`. The handler runs during the call itself.
3. Otherwise the bridge serializes the parameters and hands `(id, method, paramsJson, buffers)` to the dispatcher of the engine. A handler that a native library registered through `HaylenNativeApi` (`engine/src/platform/native/NativeApi.cpp`) answers next, and the host gets the call otherwise, through `Host::dispatchPlatformCall`. The Sokol runtime forwards it to `platform::Services::dispatch`, which each platform folder under `engine/src/platform` implements.
4. Native code answers once, from any thread, with success and a JSON value or with failure and a message, a code and data. The answer goes through `platform::BridgeRelay::resolve` in `engine/src/platform/BridgeRelay.cpp` to the bridge of the running engine, which the platform plugin attaches when the app starts, and the bridge parses it and queues it.
5. At the start of every frame, `Engine::frame` polls the Varn event loop and calls `Bridge::pump`, which runs the queued callbacks, events and the work native callbacks posted, on the frame thread before the fixed update and the update. In Lua, the call returned by `platform.call` settles there.

Native events take the same road: `BridgeRelay::emit(event, payloadJson, buffers, options)` queues the event, and `pump` delivers it to every listener connected with `on`. An event with invalid JSON, or whose JSON refers to a buffer it lacks, is logged and dropped. Events that arrive while no app is running are dropped too, and so are events that nothing listens to, unless they are retained.

## Retained events

Some events come before the app listens: the deep link or the notification that opened the app, a purchase that finished while it was closed, or the result of a sign-in that restores itself at launch. Native code sends them retained. A retained event that nothing listens to waits in the bridge, up to `Bridge::kRetainedLimit`, 32, per name with the oldest dropped first, and the first `on` of its name receives the waiting events in order at the next pump, before any newer event of that name. The events wait in the bridge of the running app, so a restarted app starts with none, and an event that reaches the engine while no app runs is dropped even when it is retained. The Android library and the Apple runtime keep the events that native code sends while no app runs, such as while the app launches, and hand them to the next app in order, so an event sent retained before any app ran still reaches its first listener. Every entry point takes the flag: the `retain` field of `Bridge::EmitOptions` for `Bridge::emit` and `BridgeRelay::emit` in C++, `HAYLEN_NATIVE_EMIT_RETAIN` in the flags of `emit` of `HaylenNativeApi` in native libraries, `HaylenBridge.emit(event, payload, retain)`, `emitRetained` and `emit(event, payload, retain, batched)` of plugin contexts on Android, `+emit:payload:retain:` of `HaylenBridge`, `emitRetained:payload:` and `emit:payload:retain:batched:` of plugin contexts on Apple platforms, `Module.haylen.emit(event, payload, {retain: true})` and `context.emit` of plugin modules on the web, and `platform.emit(event, payload, {retain = true})` in Lua tests.

## Batched events

Native code that reports something many times per frame, such as a sensor, a location or the progress of a download, marks its events batched. The batched events of one name that reach the bridge before a pump arrive at the listeners once, as one event whose payload lists their payloads in order, placed where the first of them came. In Lua the listener receives a list of payloads, and in C++ a `Bridge::Payload` whose JSON is an array, whose buffers are the buffers of the events in order and whose references point at them. Events of the same name without the flag arrive on their own, as always. A batch is retained when any of its events is, and it waits as the list of its frame, up to 32 lists per name, each of which the first listener receives in order. The flag goes next to the retain flag on every platform: the `batched` field of `Bridge::EmitOptions` in C++, `HAYLEN_NATIVE_EMIT_BATCHED` in the flags of `emit` of `HaylenNativeApi`, `HaylenBridge.emit(event, payload, retain, batched)` and `emit(event, payload, retain, batched)` of plugin contexts on Android, `+emit:payload:retain:batched:` of `HaylenBridge` and `emit:payload:retain:batched:` of plugin contexts on Apple platforms, `{batched: true}` in the options of `Module.haylen.emit` and `context.emit` on the web, and `{batched = true}` in the options of `platform.emit` in Lua.

## Byte buffers

A payload is JSON with an ordered list of byte buffers next to it, and the JSON refers to buffer N as the object `{"$bytes": N}`, an object whose only key is `$bytes` with an integer of 0 or more. A buffer can appear in any place of the JSON and more than once. Binary data such as a photo, a recording or a file therefore crosses the bridge as bytes, never as base64 text, in the parameters of a call, in its result and in an event, retained and batched ones included. Every entry point is thread-safe, and each copies the bytes once, into the engine or out of it:

| Where | Bytes of the app | Bytes of native code |
| --- | --- | --- |
| Lua | A string marked with [`platform.bytes(data)`](lua-api/platform.md#platformbytesdata), since plain strings cross as text. | A Lua string in the place of the reference. |
| C++ | `Bridge::Payload{json, buffers}`, whose buffers are `std::vector<std::byte>`, for `call`, `send`, handlers and listeners. | The buffers of `Bridge::resolve` and `Bridge::emit`. |
| C | A `const HaylenNativeBuffer*` array that handlers receive, valid while they run. | `HaylenNativeBuffer` arrays of `resolve` and `emit` of `HaylenNativeApi`. |
| Java and Kotlin | A `byte[]` value in the `JSONObject` or `JSONArray` of the parameters. | `byte[]` and `ByteBuffer` values anywhere in the value of `reply.success` and `emit`, where a direct `ByteBuffer` crosses JNI without a copy in Java. |
| Objective-C and Swift | An `NSData` value, `Data` in Swift, in the parameters. | `NSData` and `Data` values anywhere in the value of a reply and of `emit`. |
| JavaScript | A `Uint8Array` value in the parameters. | `ArrayBuffer`, `Uint8Array` and every other `ArrayBuffer` view anywhere in the value of a handler and of `emit`, copied into wasm memory. |

JSON that refers to a buffer it lacks fails a call with the code `invalidBytes` and a message such as `The JSON refers to byte buffer 2, but only 1 came with it.`, drops an event with an error in the log, and makes `Bridge::call` and `Bridge::send` throw `std::invalid_argument`. Failures carry JSON alone, so the buffers of a failure are dropped. The Swift helpers for `Codable` values carry JSON alone as well, so Swift handlers that take or return bytes use `registerHandler` with dictionaries.

## Timeouts and cancellation

A call with a `timeout` fails with the code `timeout` at the first pump after its deadline, and `call:cancel()` fails a pending call with the code `cancelled` at the next pump. In both cases the bridge tells the native side that the app gave the call up, through `Bridge::Canceller`: a handler of a native library hears it through its `HaylenNativeCancel` function, and a platform handler through `Host::cancelPlatformCall` and `Services::cancel`, which reach `HaylenBridge.cancel` on Android, the cancel block of a cancellable handler on Apple platforms and the `AbortSignal` of the handler on the web. Windows and Linux fail the calls that reach them during the call itself, so no call of theirs is ever pending. An answer that comes after a call timed out or was cancelled is dropped, like a second answer to the same call. A call without a timeout waits for its answer as long as the app runs, so every handler answers exactly once.

## Screens

Plugins also open screens: native UI that takes over the app until it ends with one result, such as a paywall, a sign-in flow or a payment page. A screen travels like a call, with a cover around it.

```text
Lua handle:openScreen ─► Screens::open ─► ScreenRelay::show, or busy and notActive at the next pump
next frame: Engine::frame ─► cover ─► Screens::present ─► NativeApi, a screen of a native library
                                                        └► Host::openScreen ─► Services::openScreen ─► platform screen
native end (any thread) ─► ScreenRelay::finish ─► queue
next pump: Screens::pump ─► the call, or <plugin>.screenRestored for the next app
```

The engine covers the app before the platform shows the screen and keeps it covered until the screen ends, however it ends, and it draws nothing while an opaque screen shows. The screen that shows belongs to the process, so an app that restarts under it starts covered and receives its end as the retained event `<plugin>.screenRestored`, with the state that the earlier app gave. The platform keeps the pending screen where it survives the end of the process, such as the session storage of a page that leaves for a redirect, and hands its end to `ScreenRelay::restore` when the app starts again. A cancel or a timeout fails the call at once and asks the platform through `Host::cancelScreen` to dismiss the screen, whose end then reaches no app. The [plugin guide](plugins.md#plugin-screens) describes the model, the Lua API and the native contract on each platform, and the [haylen.platform reference](lua-api/platform.md#screens) the options and the codes.

## The JSON contract

- Parameters are a JSON value with its byte buffers, an empty object when Lua passes none. Lua tables become JSON as the [reference](lua-api/platform.md#how-calls-travel) describes.
- A successful result is any JSON value with its byte buffers, and `null` reaches Lua as `nil`.
- A failure carries either a JSON string, which becomes the error message, or an object with a `message` string and optional `code` and `data` values, which Lua receives as the fields of the error. Any other failure payload, such as `null`, a number or an object without a string `message`, fails with `The native platform call failed without a message.` and keeps the code and data of an object, and text that is not JSON fails with the code `invalidJson` and `The platform returned invalid JSON.`
- The engine and the platform sides fail calls with these codes: `timeout` and `cancelled` from the bridge, `noHandler` when nothing answers the method, `invalidJson`, `invalidBytes` for JSON that refers to a buffer it lacks, and `exception` when a Java, Kotlin, Swift or JavaScript handler threw an error without a code of its own instead of answering, with the class of the exception or the type of the error in `data.type`.
- Method and event names are free-form strings, usually dotted names such as `auth.google.signIn` of the Tiny Island sample. [Plugins](plugins.md) put their id in front of their method and event names, with the names in camelCase, such as `admob.showBanner` and `admob.closed`: the plugin handle of Lua and the plugin contexts of native code and the web add the prefix, so neither side writes it.

## The Lua side

| Function | Purpose |
| --- | --- |
| `platform.call(method, params, options)` | Calls a method and returns a call. `call:await()` inside a coroutine returns the result, or `nil` and the error, a table with `message`, `code` and `data` that reads as its message. `options.timeout` gives up after that many seconds, and `call:cancel()` gives up at once. |
| `platform.send(method, params)` | Calls a method whose answer nobody needs. It creates no call and drops the answer. |
| `platform.on(event, listener)` | Calls `listener(payload)` for every native event of that name, or once per frame with the list of the batched ones, and returns a connection with `disconnect()`. The first listener of a name also receives the retained events that wait for it. |
| `platform.bytes(data)` | Marks a string that crosses the bridge as bytes. |
| `platform.plugin(id)` | Returns the handle of a plugin of the app, whose `call`, `send` and `on` put the id of the plugin in front of the name, and whose `openScreen` opens a [screen](#screens) of the plugin. |
| `platform.screenShowing()` | Returns whether the screen of a plugin shows, whichever app of the process opened it. |
| `platform.registerHandler(method, handler)` | Answers a method with a Lua function inside the engine, which takes precedence over native handlers. |
| `platform.hasHandler(method)` | Returns whether an engine handler answers the method. Native handlers are invisible to it. |

Because `hasHandler` cannot see native handlers, an app that provides Lua stand-ins for methods implemented natively on some platforms decides by `haylen.platform`, as the [complete example](#complete-example) does. Registering a stand-in unconditionally would hide the native handler on every platform.

## Android

The engine's Android library module `engine/platform/android/haylen` holds `dev.haylen.HaylenActivity`, the GameActivity that runs the app, and `dev.haylen.HaylenBridge`, the handler registry:

| Member | Meaning |
| --- | --- |
| `HaylenBridge.register(String method, MethodHandler handler)` | Adds or replaces the handler of a method, which runs on the main thread. |
| `HaylenBridge.register(String method, MethodHandler handler, HaylenBridge.Threading threading)` | The same, with the thread the handler runs on: `MAIN` or `BACKGROUND`. |
| `HaylenBridge.unregister(String method)` | Removes a handler. |
| `HaylenBridge.emit(String event, Object payload)`, `HaylenBridge.emit(String event, Object payload, boolean retain)`, `HaylenBridge.emit(String event, Object payload, boolean retain, boolean batched)` | Sends an event to the app, retained for the first listener of its name when `retain` is `true`, and in the list of its frame when `batched` is `true`. |
| `HaylenBridge.activity()` | The running `HaylenActivity`, an `AppCompatActivity` and so a `ComponentActivity`, or `null` when there is none. |
| `MethodHandler.handle(Object params, Reply reply) throws Exception` | The handler interface, usable as a lambda. |
| `Reply.success(Object value)` | Answers the call, with `byte[]` and `ByteBuffer` values as bytes. |
| `Reply.failure(String message)`, `Reply.failure(String message, String code, Object data)`, `Reply.failure(Throwable error)` | Fail the call, with a code and data, or with a thrown error: a `HaylenBridge.Failure` keeps its code and data, and any other error fails with the code `exception`. |
| `Reply.isCancelled()`, `Reply.onCancel(Runnable listener)` | Whether the app gave the call up, and a listener that runs on the main thread when it does. |
| `HaylenBridge.Failure(String message, String code, Object data)` | An exception that a handler throws to fail its call with a code and data. |
| `HaylenCoroutines.register(String method, suspend (Any?) -> Any?)` | Adds a Kotlin handler written as a suspending function, which runs in a coroutine on the main thread and is cancelled when the app gives the call up. |

How a call runs:

1. The engine calls the static `HaylenBridge.dispatch(long, byte[], byte[], byte[][])` through JNI on the frame thread, with the method name and the parameters as UTF-8 bytes and the byte buffers of the parameters. Every text crosses JNI as UTF-8 bytes in both directions, because the JNI string functions use a modified UTF-8 that breaks characters outside the Basic Multilingual Plane, such as emoji. A method without a handler fails with the code `noHandler` and `No native handler is registered for <method>.`. Otherwise the frame thread only posts the call to the thread of the handler and goes on. The native side attaches each of its threads to the Java VM the first time it calls Java and detaches it when the thread ends, so no call pays for an attachment.
2. The handler runs on the main thread, or on the one background thread that every handler registered with `HaylenBridge.Threading.BACKGROUND` shares, in the order the calls arrive. A main thread handler whose activity was destroyed in the meantime fails its call with `The activity was destroyed before <method> ran.`, while background handlers run without an activity.
3. The parameters are parsed with `org.json.JSONTokener` on the thread of the handler, so an object arrives as a `JSONObject`, and every reference to a buffer becomes a `byte[]` value in its place. Parameters that do not parse fail the call.
4. The handler replies at once or later, from any thread. `success` accepts `null`, strings, numbers, booleans, `JSONObject`, `JSONArray`, maps, collections and arrays, converted with `JSONObject.wrap`, with `byte[]` and `ByteBuffer` values anywhere inside, which cross JNI as buffers. A direct `ByteBuffer` crosses with its remaining bytes without a copy in Java, and a value that is not JSON, such as a `NaN`, fails the call. The first answer counts, and later ones are dropped.
5. When the app cancels the call or its timeout passes, the engine calls the static `HaylenBridge.cancel(long)` through JNI, which marks the reply cancelled, drops the answer that may still come and runs the `onCancel` listeners on the main thread. A call cancelled before its handler ran never runs it.

A handler that throws, a checked exception included, fails its call through `reply.failure(Throwable)` instead of crashing the app, so an unexpected `NullPointerException` becomes a failed call with the code `exception` and the class name in `data.type`. Errors of the virtual machine, such as `OutOfMemoryError`, still end the app. `HaylenBridge.emit` works from any thread at any time and converts its payload like `success`, logging and dropping one that is not JSON: while no app runs, such as before the first activity loads the native library, while an app restarts or between two activities, the library keeps the events, up to 32 per name with the oldest dropped first, and hands them to the next app in order once it starts.

`HaylenCoroutines` in `engine/platform/android/haylen/src/main/kotlin/dev/haylen/HaylenCoroutines.kt` registers Kotlin handlers written as suspending functions. It launches each call in a coroutine of a scope on `Dispatchers.Main.immediate`, answers with the return value, fails the call with what the function throws, and cancels the coroutine from `onCancel`. The library depends on `kotlinx-coroutines-android` as an API, so apps get it with the library. The [reference](lua-api/platform.md#android-handlers) has examples in Java and Kotlin.

[Plugins](plugins.md#the-android-part) register their handlers through their context, which puts the id of the plugin in front of the name, and apps register theirs once, in `Application.onCreate`, before the first activity starts. Tiny Island does this in `samples/games/tiny-island/platform/android/app/src/main/java/dev/haylen/tinyisland/TinyIslandApplication.java`, which its `app/app.gradle` names through the `haylenApplication` manifest placeholder of the [Android template](distribution.md#platform-overrides). Its `GoogleSignInPlugin` answers `auth.google.signIn` with Credential Manager: it builds a `GetGoogleIdOption` with the web client id of the game's Google Cloud project, which the build takes from the `googleServerClientId` Gradle property into the `google_server_client_id` string resource, calls `getCredentialAsync` on the activity's main executor and replies with `idToken`, `email`, `name` and `picture`. Without a client id it fails with a message that explains the build property. R8 keeps the classes the engine reaches by name through the module's `consumer-rules.pro`, so handlers need no rules of their own.

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
+ (void)emit:(NSString*)event payload:(nullable id)payload retain:(BOOL)retain;
+ (void)emit:(NSString*)event payload:(nullable id)payload retain:(BOOL)retain batched:(BOOL)batched;
@end
```

- The parameters are parsed with `NSJSONSerialization`, so an object arrives as an `NSDictionary`, every reference to a buffer becomes an `NSData` value in its place, and parameters that do not parse arrive as an empty dictionary.
- Handlers run on the main queue and may reply later from any thread. `reply(YES, result)` succeeds with any value `NSJSONSerialization` accepts, with `NSData` values anywhere inside, which cross as buffers, or `nil`. `reply(NO, failure)` fails with a message string or a dictionary with a `message` string and optional `code` and `data`. The first answer counts, and later ones are dropped.
- A cancellable handler returns a block, or `nil`, that runs on the main queue when the app cancels the call or its timeout passes, after which an answer is dropped. A call cancelled before its handler ran never runs it.
- A method without a handler fails with the code `noHandler` and `No native handler is registered for <method>.`
- `emit:payload:` sends an event from any thread, with `NSData` values as buffers, `emit:payload:retain:` with `YES` sends it [retained](#retained-events), and `emit:payload:retain:batched:` with `batched` set to `YES` sends it [batched](#batched-events). An event whose payload `NSJSONSerialization` rejects is logged and dropped. Events sent while no app runs, such as while the app launches, before its first frame, or while it restarts, wait in the runtime and reach the next app in order once it starts, so the link or the notification that opened the app is never lost.
- The context of a [plugin](plugins.md#the-context) registers and emits under the id of the plugin, so `[context registerHandler:@"show" ...]` of the plugin `admob` answers `admob.show`, and `[context emitRetained:@"closed" payload:nil]` sends `admob.closed`.

Swift handlers use `HaylenBridge.register(method) { (params: Params) async throws -> Result in ... }` from `source/HaylenBridgeAsync.swift` of the Apple template, which decodes the parameters with `JSONDecoder`, encodes the result with `JSONEncoder`, fails the call with the code and data of a thrown `HaylenFailure` or with the code `exception` for any other error, and cancels the task of the call when the app gives it up. `try HaylenBridge.emit(event, payload, retain: true)` sends an event with an `Encodable` payload, and plugin contexts have the same helpers, `context.register` and `context.emit`, as the [plugin guide](plugins.md#swift-helpers) describes. The template ships `HaylenBridgeAsync.swift` as a source file, because the engine artifact is a static library of C, C++ and Objective-C whose headers Swift reaches through `source/HaylenBridging.h`, and a Swift module in the artifact would have to match the Swift compiler of each app. The target names its module `HaylenApp`, so Objective-C++ code such as `main.mm` calls Swift classes through `#import "HaylenApp-Swift.h"`.

The handler table exists from the first registration, so native code may register at any time. Apps made from the Apple template register in the `main` function of `source/main.mm`, before it calls `haylen_main`, with their Objective-C, C++ and Swift files next to it in `platform/apple/source/` of the app, whose files all build into every target, as the [distribution guide](distribution.md#platform-overrides) describes. Apps built with `haylen_add_app` add the Objective-C file with its `SOURCES` argument, compile it with `-fobjc-arc` as the engine compiles its Apple sources, and register from `+load` or once the app has launched. The [reference](lua-api/platform.md#apple-handlers) has a complete handler.

## Web

The page side of the bridge lives in `engine/platform/web/haylen-runtime.js`, which every web target links with `--pre-js` and which fills `Module.haylen`:

| Function | Meaning |
| --- | --- |
| `Module.haylen.register(method, handler)` | Adds or replaces the handler of a method. |
| `Module.haylen.unregister(method)` | Removes a handler. |
| `Module.haylen.emit(event, payload, options)` | Sends an event to the app. The payload is converted with `JSON.stringify`, with `ArrayBuffer` and `Uint8Array` values as bytes, `options.retain` keeps the event for the first listener of its name, and `options.batched` sends it in the list of its frame. Events sent before the first app starts reach it once it starts. |
| `Module.haylen.createPluginContext(id, config)` | Makes the context that the web module of a plugin receives, whose `register` and `emit` put the id of the plugin in front of the name, as the [plugin guide](plugins.md#web-modules) describes. |

The runtime calls the handler after the frame that made the call, with the parsed parameters, where bytes of the app arrive as `Uint8Array` values, and a context with the `call` id and a `signal`, an `AbortSignal` that aborts when the app cancels the call or its timeout passes. A call cancelled in the frame that made it never runs its handler. The handler returns the result or a promise for it, whose `ArrayBuffer` and `Uint8Array` values cross as bytes, and a thrown error or a rejected promise fails the call with the error message and the `code` and `data` properties of the error, or with the code `exception` and the `name` of the error in `data.type` when the error has no code. The first answer counts, and an answer after a cancel is dropped. A method without a handler fails with the code `noHandler` and `No page handler is registered for <method>.`

The web build is single-threaded, so handlers run on the browser's main thread between frames. An asynchronous handler, such as one that waits for `fetch` or for a sign-in popup, never blocks the app while it waits.

`register` exists once the runtime script has run, so a page registers its handlers in `Module.preRun`. The web template runs the `app.js` of an app for that, and Tiny Island's `samples/games/tiny-island/platform/web/app.js` registers `auth.google.signIn` there. It loads Google Identity Services only when the player first signs in, reads the client id from a constant at the top of the file, and resolves with `idToken`, `email`, `name` and `picture` decoded from the returned credential. The Google script needs `make.py run --platform web --coep off`, as the [distribution guide](distribution.md#serve) explains.

## Desktop

macOS uses the Apple registry above. Windows and Linux have no registry in the language of the platform, so their services fail every call that reaches them with the code `noHandler`, synchronously on the frame thread, and the failure still reaches the app at the start of the next frame. Apps add methods on these platforms with handlers of native libraries, which a Lua app loads without compiling the engine, or with C++ or Lua handlers.

## Native library handlers

A native library answers methods in C on every platform that loads native libraries. `native.load(name, {init = 'symbol'})` hands its init function the `HaylenNativeApi` of `haylen/platform/native/HaylenNative.h`, whose `registerHandler` adds a handler with an optional cancel function, which receives the parameters with their byte buffers, `resolve` answers a call from any thread with buffers, `emit` sends an event with buffers from any thread, retained or batched, `log` writes to the engine log, `registerPlugin` declares the library the native part of a plugin, `registerErrorHandler` hands it the errors that stop the app, `openVideoStream`, `pushVideoFrame`, `openAudioStream` and `pushAudioFrames` feed the [streams](plugins.md#streams) of a plugin, `registerScreen` and `finishScreen` open and end the [screens](plugins.md#desktop-screens) of a plugin, `getWindow` hands the library the window of the app, and `coverApp` and `uncoverApp` cover the app. The handlers belong to the process and answer after the engine handlers and before the platform handlers. The [native code guide](native.md#libraries-that-talk-to-the-app) shows a library, and the [reference](lua-api/native.md#library-handlers) lists the entries.

## C++ handlers and calls

`haylen::platform::Bridge` in `engine/include/haylen/platform/Bridge.hpp` is the whole bridge, and `engine.getPlatform()` returns the running one:

| Member | Meaning |
| --- | --- |
| `registerHandler(std::string method, Bridge::Handler handler)` | Answers a method inside the engine. The handler receives the `const Bridge::Payload& params`, JSON with its `std::vector<std::byte>` buffers, and a `Bridge::Reply`, which takes a `Bridge::Result{ok, value, error}` whose `value` is a `Bridge::Payload` and whose `Bridge::Error` has a `message`, a `code` and `data`. |
| `hasHandler(std::string_view method)` | Whether an engine handler answers the method. |
| `call(std::string_view method, const Bridge::Payload& params, Bridge::Callback callback, timeout)` | Calls a method and returns the call id. The callback receives the `Bridge::Result` on the frame thread, and an optional `std::chrono::steady_clock::duration` fails the call with the code `timeout` once it passes. |
| `cancel(std::uint64_t id)` | Fails a pending call with the code `cancelled` at the next pump and tells native code, and returns whether the call was pending. |
| `getMailbox()` | A `Bridge::Mailbox` whose `post` queues work for the frame thread from any thread, until the bridge is gone. Native callbacks deliver their calls through it. |
| `on(const std::string& event, std::function<void(const Bridge::Payload&)> listener)` | Listens to native events and returns a `Connection`. |
| `send(std::string_view method, const Bridge::Payload& params)` | Calls a method whose answer nobody needs, without a pending call. |
| `resolve(std::uint64_t id, bool ok, std::string_view resultJson, buffers = {})` | Answers a call with the byte buffers of its JSON. Safe from any thread. |
| `emit(std::string_view event, std::string_view payloadJson, buffers = {}, const Bridge::EmitOptions& options = {})` | Sends an event with the byte buffers of its JSON, which waits for the first listener of its name when `options.retain` is set and arrives in the list of its frame when `options.batched` is set. Safe from any thread. |
| `getPendingCallCount()` | Calls still waiting for an answer. |

A plugin is the usual home for C++ handlers, because its `start` runs with every new engine:

```cpp
class CloudSavePlugin final : public haylen::plugins::Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "cloudSave";
    }

    void start(haylen::core::Engine& engine) override {
        engine.getPlatform().registerHandler("save.cloudSync", [](const haylen::platform::Bridge::Payload& params, haylen::platform::Bridge::Reply reply) {
            reply({.ok = true, .value = {.json = {{"synced", params.json.value("slot", 0)}}}});
        });
    }
};
```

A C++ application adds it with `engine.addPlugin(std::make_unique<CloudSavePlugin>())`, and Lua then calls `platform.call('save.cloudSync', {slot = 2})`. A handler may keep the reply and answer later from another thread, but it must answer before the engine stops, because the reply belongs to that engine's bridge. C++ code calls methods the same way Lua does:

```cpp
engine.getPlatform().call("profile.load", {.json = {{"id", "me"}}}, [](haylen::platform::Bridge::Result result) {
    if (!result.ok) {
        haylen::core::Log::warning("profile.load failed: {}", result.error.message);
        return;
    }
    haylen::core::Log::info("Playing as {}", result.value.json.value("name", std::string("a guest")));
});
```

Bytes travel next to the JSON, which refers to each buffer with `haylen::core::JsonBytes::makeReference(index)`, and `JsonBytes::findReference` reads a reference back:

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

This example adds `accessibility.reducedMotion`, which tells the app whether the player asked the system for less motion, and an `accessibility.reducedMotionChanged` event, implemented natively on Android and on the web. The app turns its camera shake off when the answer says so and uses a Lua stand-in on the other platforms.

### Android

`app/src/main/java/com/example/myapp/MotionPlugin.java`:

```java
package com.example.myapp;

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

`app/src/main/java/com/example/myapp/MyAppApplication.java`:

```java
package com.example.myapp;

import android.app.Application;

public final class MyAppApplication extends Application {
    @Override
    public void onCreate() {
        super.onCreate();
        MotionPlugin.register(getContentResolver());
    }
}
```

`app/app.gradle` names the application class through the `haylenApplication` manifest placeholder of the [Android template](distribution.md#platform-overrides):

```groovy
android {
    defaultConfig {
        manifestPlaceholders.haylenApplication = "com.example.myapp.MyAppApplication"
    }
}
```

The handler reads a setting and touches no view, so it runs on the background thread of the bridge. The observer and `Collections.singletonMap` work from API 27, the engine's minimum, and the event reaches the app even when the player changes the setting while the app is in the background.

### Web

The app's shell, passed to `haylen_add_app` as `WEB_SHELL`, registers the handler in `preRun`:

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

`source/main.lua`:

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
        print('accessibility.reducedMotion failed: ' .. err)
    end
end)

platform.on('accessibility.reducedMotionChanged', applyMotion)
```

## Testing bridge code

The headless host used by the engine tests records every call that reaches native code, and a test answers it through the bridge:

```cpp
ASSERT_TRUE(fixture.frameUntil([&] { return fixture.host().getPlatformCalls().size() == 1; }));
fixture.engine().getPlatform().resolve(fixture.host().getPlatformCalls()[0].id, true, R"({"model": "phone"})");
```

`platform::HeadlessHost::getPlatformCalls()` returns the id, method, JSON parameters and byte buffers of each call, and `getCancelledCalls()` the ids of the calls the bridge gave up through a timeout or a cancel. A test answers with buffers the way native code does, such as `resolve(id, true, R"({"png": {"$bytes": 0}})", {pngBytes})`, and sends batched events with `emit(event, json, {}, {.batched = true})`. Lua code under test can also stand in for native methods with `platform.registerHandler`, and the native interop tests load `engine/tests/native/NativeTest.c`, whose init function registers handlers through `HaylenNativeApi`. See the [testing guide](testing.md).
