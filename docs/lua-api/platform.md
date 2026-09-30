# haylen.platform

`haylen.platform` is the bridge between the app and native code. An app calls named methods with JSON parameters and awaits their JSON result, which may fail with a typed error, time out or be cancelled, and it listens to named events that native code sends. Parameters, results and events carry bytes next to their JSON, such as images and audio, and the native parts of plugins feed the app video and audio streams. Use it for everything the engine does not wrap, such as sign-in, purchases, sharing or deep links, with the handlers living in Java or Kotlin on Android, Objective-C or Swift on Apple platforms, JavaScript on the web, C in native libraries, or C++ anywhere. The engine wraps what the device is, opening urls and vibrating in [haylen.system](system.md), and native message boxes and file pickers in [haylen.dialogs](dialogs.md). The Lua modules of [plugins](../plugins.md) reach their native parts through the [handle of their plugin](#plugin-handles), which also opens the [screens](#screens) of the plugin, native UI that takes over the app until it ends with one result. The [native code guide](../native.md) compares the bridge with the other ways to reach native code.

```lua
local platform = require('haylen.platform')
```

## How calls travel

`platform.call` looks for a handler in this order:

1. A handler registered in the engine, either from Lua with `platform.registerHandler` or from C++.
2. A handler that a native library registered through the `HaylenNativeApi` of the engine, as [haylen.native](native.md#library-handlers) describes.
3. The native handler of the running platform: `HaylenBridge` on Android and Apple platforms and `Module.haylen` on the web. Windows and Linux have no handler registry of their own, so handlers of native libraries and C++ handlers answer there.

A method that no handler answers fails with the code `noHandler` and `No native handler is registered for <method>.`, or `No page handler is registered for <method>.` on the web.

Parameters and results cross the bridge as JSON. Lua tables with only consecutive integer keys starting at 1 become arrays, other tables become objects, and an empty table becomes an empty object. Functions, userdata and other values that JSON cannot hold raise `A <type> cannot be converted to JSON.`. JSON `null` arrives in Lua as `nil`.

Strings cross as text, so a string must hold UTF-8 to reach native code. Bytes, such as an image, a recording or any binary data, cross as byte buffers next to the JSON instead: a string that [`platform.bytes`](#platformbytesdata) marks becomes a buffer in the place of the string, and every buffer that native code sends arrives as a Lua string in its place, in results and in events alike. The [platform bridge guide](../platform_bridge.md#byte-buffers) shows how each language sends and receives them.

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

Calls `listener(payload)` every time native code sends the event named `event`, with the payload converted from JSON and its bytes as strings. Native code that marks its events batched, such as the readings of a sensor, calls the listener once per frame with the list of the payloads of that frame, in order. Returns a `haylen.Connection`. Its `disconnect()` method stops the listener, and its `connected` property is `true` until then. A listener stays connected when the connection object is garbage collected. An error raised inside a listener stops the app and shows the error screen with the message and its stack trace. Events that no listener waits for are dropped, except retained ones.

Native code retains an event that may come before the app listens, such as the deep link or the notification that opened the app, or purchases that finished while it was closed. A retained event that nothing listens to waits, up to 32 per name with the oldest dropped first, and the first listener of its name receives the waiting events in order at the start of the next frame, before any newer event of that name. A retained batch waits as the list of its frame. Events wait in the running app, so a restarted app starts with none.

The engine sends no events of its own, so event names are a contract between the app and its native code. Plugins name their events after their id, such as `admob.closed`.

```lua
local platform = require('haylen.platform')

local links = platform.on('app.link', function(payload)
    print('opened from ' .. payload.url)
end)

local function leaveMenu()
    links:disconnect()
end

-- A sensor sends its readings batched, so the listener runs once per frame with every reading of the frame.
platform.on('motion.reading', function(readings)
    local last = readings[#readings]
    print(#readings .. ' readings, the last at ' .. last.x .. ', ' .. last.y)
end)
```

### platform.registerHandler(method, handler)

Answers the method named `method` with the Lua function `handler(params)`, which returns the result. The function runs during the `platform.call` that asks for it and must return at once, and its result must convert to JSON. Bytes of the parameters arrive as strings, and strings of the result that `platform.bytes` marks cross as bytes. An error raised inside it fails the call with the error message and its stack trace instead of stopping the app. A registered handler replaces any earlier engine handler of the same method, and it takes precedence over native handlers. Lua handlers suit desktop builds and tests that stand in for mobile services.

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

Returns `true` when an engine handler answers `method`, which covers methods registered with `platform.registerHandler` and methods C++ code registered. It does not see native handlers, so it returns `false` for a method that only the platform answers.

```lua
local platform = require('haylen.platform')

platform.registerHandler('score.best', function() return 1200 end)
print(platform.hasHandler('score.best'))
print(platform.hasHandler('store.buy'))
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

### platform.screenShowing()

Returns `true` while the [screen](#screens) of a plugin shows over the app, whichever app of the process opened it. An app that restarted under a screen starts covered, and this tells it that a screen covers it, whose end arrives as `screenRestored`.

```lua
local haylen = require('haylen')
local platform = require('haylen.platform')

if platform.screenShowing() then
    print('a screen of a plugin still shows, covered ' .. tostring(haylen.appCovered()))
end
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

Sends the event `event` with `payload` the way native code does, so `platform.on` listeners receive it at the start of the next frame. With `options.retain` set to `true`, the event waits for the first listener of its name like a [retained event](#platformonevent-listener) of native code, and with `options.batched` set to `true` it reaches the listeners in the list of its frame like a batched event of native code. It suits tests and desktop builds that stand in for native events. A payload that JSON cannot hold raises `A <type> cannot be converted to JSON.`, and an unknown option raises an error that names it.

```lua
local platform = require('haylen.platform')

platform.emit('app.link', {url = 'island://beach'}, {retain = true})

-- The link was retained, so this listener receives it at the start of the next frame.
platform.on('app.link', function(payload)
    print('opened from ' .. payload.url)
end)

-- The three readings reach the listener as one list at the start of the next frame.
platform.on('motion.reading', function(readings) print(#readings) end)
for index = 1, 3 do
    platform.emit('motion.reading', {x = index, y = 0}, {batched = true})
end
```

### platform.bytes(data)

Returns a `haylen.Bytes` that marks the string `data` to cross the bridge as a byte buffer, in the parameters of a call or of a send, in the result of a Lua handler and in an event. Plain strings cross as text, which must be UTF-8, so marking keeps binary data such as images or recordings exact and never turns it into text. The mark holds a copy of the bytes, and its read-only `size` property tells how many. Only the bridge takes it, so any other conversion to JSON raises `A userdata cannot be converted to JSON.`. A value that is not a string raises a bad argument error.

```lua
local assets = require('haylen.assets')
local async = require('async')
local platform = require('haylen.platform')

local photo = assets.bytes('photos/beach.png')
local marked = platform.bytes(photo)
print(marked.size == #photo)

async.spawn(function()
    -- The photo crosses as bytes, while the name stays text, and the thumbnail comes back as a Lua string of bytes.
    local saved, err = platform.call('gallery.save', {image = marked, name = 'beach.png'}):await()
    if saved then
        print('the thumbnail takes ' .. #saved.thumbnail .. ' bytes')
    else
        print('gallery.save failed: ' .. err)
    end
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
    local both = async.all({platform.call('profile.load', {id = 'me'}).promise, platform.call('store.products').promise}):await()
    print(both[1].name .. ' sees ' .. #both[2] .. ' products')
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
| `handle:videoStream(name)` | The [video stream](#video-streams) `name` that the native part of the plugin opened, or `nil` until it opens it. The engine keeps its texture current from then on, until the app stops. |
| `handle:audioStream(name)` | The [audio stream](#audio-streams) `name` that the native part of the plugin opened, or `nil` until it opens it. |
| `handle:openScreen(name, params, options)` | Opens the [screen](#screens) `name` of the plugin, which covers the app until it ends, and returns a call that the result of the screen settles. |

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

## Screens

A screen is native UI of a plugin that takes over the app until it ends with one result, such as a paywall, a sign-in flow, a payment page, the activity of an SDK, a popup page on the web or a window over the window of the app on the desktops. `handle:openScreen(name, params, options)` of the [plugin handle](#plugin-handles) opens the screen `name` of the plugin with `params`, which default to an empty object and cross like the parameters of a call, bytes included, and returns a [call](#calls) at once, whose `await` returns the result of the screen or `nil` and an [error](#errors).

| Option | Type | Meaning |
| --- | --- | --- |
| `state` | any JSON value without bytes | A value that the platform keeps with the screen, where it survives the end of the process, and hands back with a restored end. |
| `opaque` | boolean | Whether the screen hides the app completely, `true` by default. The engine draws nothing under an opaque screen and keeps the last frame on screen, while it keeps drawing the halted app under a screen that lets the app show through, such as a sheet. |
| `timeout` | number | Seconds of real time after which the call fails with the code `timeout` and the platform dismisses the screen. Screens have no timeout otherwise. |

- The engine covers the app at the start of the next frame and only then hands the screen to the platform, so the app is `'inactive'`, halted and muted before the screen shows, and `haylen.appCovered()` returns `true` until the screen ends, however it ends.
- One screen shows at a time in the whole process, and a screen opens only while the app is `'active'`.
- `call:cancel()` and the timeout fail the call at once and ask the platform to dismiss the screen, and the app stays covered until the screen is gone.
- A screen outlives the app that opened it. When the app restarts under it or the process ends while it shows, such as a web page that left for a redirect and loaded again, its end reaches the next app as the retained event `screenRestored` of the plugin, which `handle:on('screenRestored', listener)` receives with `screen`, the name of the screen, `state`, the value of `options.state`, and `result`, or `error` with `message`, `code` and `data` when the screen failed. The event waits for the first listener of its name, like every [retained event](#platformonevent-listener).

A screen fails with the codes of [errors](#errors) and these.

| Code | When |
| --- | --- |
| `busy` | Another screen shows, which an earlier app of the process may have opened. |
| `notActive` | The app is not active, because it lost the focus, it is in the background or native UI covers it, or the platform cannot present the screen at that moment. |
| `cancelled` | `call:cancel()` gave the screen up, or the person closed the screen. |
| `noHandler` | No screen of that name is registered: no page screen on the web and no screen of a native library on Windows and Linux. |
| `unsupported` | The platform opens no screens of plugins yet, which is the case on Apple platforms and Android for screens that no native library opens. |
| `popupBlocked` | The browser blocked the popup of a web screen, which happens when the tap that asked for it is too long ago. |

An empty name raises `A screen of the plugin <id> needs a name.`, a timeout that is not a positive number raises `The timeout of a screen is a positive number of seconds.`, a state with bytes raises `The state of a screen is JSON without bytes.`, and an unknown option raises an error that names it. The [plugin guide](../plugins.md#plugin-screens) describes how the native parts of plugins open screens on each platform.

```lua
local async = require('async')
local platform = require('haylen.platform')

local paywall = platform.plugin('paywall')

-- The app restarted while the paywall showed, or the page loaded again after a redirect, so the end arrives with the state the earlier app gave.
paywall:on('screenRestored', function(ending)
    if ending.result then
        print('bought ' .. ending.result.product .. ' on level ' .. ending.state.level)
    end
end)

async.spawn(function()
    local purchase, err = paywall:openScreen('offer', {offering = 'gold'}, {state = {level = 12}, timeout = 300}):await()
    if purchase then
        print('bought ' .. purchase.product)
    elseif err.code == 'cancelled' then
        print('the player closed the paywall')
    else
        print('the paywall failed: ' .. err)
    end
end)
```

## Video streams

The native part of a plugin, such as a camera or a video decoder, pushes frames into a video stream from any thread, and `handle:videoStream(name)` returns it as a `haylen.VideoStream`. The engine keeps only the newest frame and uploads it into the texture of the stream at the start of a frame when it is new, so the texture changes once per frame at most, however fast the frames come. A frame of another size resizes the texture in place, so a sprite or a material that holds the texture keeps showing the current frame. The texture samples linearly. Two values of the same stream compare equal with `==`.

| Member | Meaning |
| --- | --- |
| `stream.texture` | The `Texture` of the stream, which holds transparent pixels of the size the native part opened the stream with, or a single one, until the first frame arrives. |
| `stream.width`, `stream.height` | The size of the frame the texture shows, or the size the stream opened with before the first frame. |
| `stream.frameCount` | How many frames the texture received, which leaves out the frames that newer ones replaced before a frame of the app. |
| `stream.timestamp` | The timestamp of the frame the texture shows, in the seconds of the native part. |
| `stream:on('frame', listener, options)` | Calls `listener(timestamp)` whenever the texture receives a frame, and returns a connection. `options.owner` ties the listener to an owner, like the listeners of [haylen.events](events.md#owners). Another event name raises `Unknown video stream event '<name>'. Video streams report frame.`. |

Streams belong to the process, like the native code that pushes into them, so a stream outlives an app that restarts, and the next app receives the newest frame again. The [plugin guide](../plugins.md#streams) describes how native code pushes frames on each platform.

```lua
local graphics2d = require('haylen.graphics2d')
local platform = require('haylen.platform')
local scene = require('haylen.scene')

local camera = platform.plugin('camera-kit')

scene.push({
    enter = function(self)
        scene.spawn(self, function()
            camera:call('start', {facing = 'back'}):await()
            self.preview = camera:videoStream('preview')
            self.preview:on('frame', function(timestamp) self.lastFrame = timestamp end, {owner = self})
        end)
    end,
    render = function(self)
        if self.preview then
            graphics2d.beginScreen()
            graphics2d.draw(self.preview.texture, 40, 40, {width = 640, height = 640 * self.preview.height / math.max(self.preview.width, 1), pivotX = 0, pivotY = 0})
        end
    end,
})
```

## Audio streams

The native part of a plugin, such as a microphone, a synthesized voice or decoded network audio, pushes samples into the ring of an audio stream from any thread, and `handle:audioStream(name)` returns it as a `haylen.AudioStream`, which plays as a voice of [haylen.audio](audio.md). The voice resamples the stream to the rate of the mixer, plays silence where samples have not arrived and counts each read that found the ring short as an underrun, and it never ends by itself, so `audio.stop` ends it. Two values of the same stream compare equal with `==`.

| Member | Meaning |
| --- | --- |
| `stream:play(options)` | Starts a voice that plays the stream and returns its id, which every voice function of `haylen.audio` takes. `options` takes the options of [`audio.play`](audio.md#audioplaysound-options) that apply to a live stream, `bus`, `volume`, `pan`, `fadeIn`, `x`, `y`, `processMode` and `effects`, and any other key raises `Unknown option '<key>'.`. A new voice takes the stream over from the voice that played it before, which plays silence from then on. |
| `stream:read(buffer)` | Copies the newest samples that native code pushed into a float buffer of [haylen.collections](collections.md#float-buffers), interleaved by channel, in whole frames and oldest first from the first value, and returns how many values it wrote, fewer while less has arrived. It suits level meters and waveforms, whether a voice plays the stream or not. |
| `stream.sampleRate` | The sample rate of the stream in hertz. |
| `stream.channels` | The channels of the stream. |
| `stream.underruns` | How many reads of its voices found fewer samples than they needed, since samples started to arrive. |

```lua
local audio = require('haylen.audio')
local collections = require('haylen.collections')
local platform = require('haylen.platform')
local scene = require('haylen.scene')

local microphone = platform.plugin('mic-kit')
local samples = collections.newFloatBuffer(1024)

scene.push({
    enter = function(self)
        scene.spawn(self, function()
            microphone:call('start'):await()
            self.stream = microphone:audioStream('input')
            self.voice = self.stream:play({bus = 'sfx', volume = 0.8})
        end)
    end,
    update = function(self, dt)
        if self.stream then
            local count = self.stream:read(samples)
            local peak = 0
            for index = 1, count do
                peak = math.max(peak, math.abs(samples[index]))
            end
            self.level = peak
        end
    end,
    exit = function(self)
        if self.voice then
            audio.stop(self.voice)
        end
    end,
})
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
| `invalidBytes` | Native code answered with JSON that refers to a byte buffer it lacks, with a message such as `The JSON refers to byte buffer 2, but only 1 came with it.`. |
| `exception` | A Java, Kotlin, Swift or JavaScript handler threw an error without a code of its own instead of answering. `data.type` names the class of the exception or the type of the error. |
| `unsupported` | The platform cannot serve the call, or the project of the app lacks what the call needs, such as a permission that the Android manifest does not declare. In the second case `data.missing` lists each missing requirement as `{kind, name, file, snippet}`, as the [plugin guide](../plugins.md#requirements) describes. |

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

## Native handlers

Native handlers receive the parameters as parsed JSON, with the bytes of the app in the types of their language, and answer once, with success and a JSON value with bytes or with failure and a message, a code and data. They may answer later, from any thread, and the answer reaches the app on the frame thread. A second answer, and an answer that comes after the app cancelled the call or its timeout passed, is dropped. Native code sends events the same way, and `platform.on` receives them.

## Android handlers

`dev.haylen.HaylenBridge` in the engine Android library holds the handlers. `HaylenBridge.register(method, handler)` adds a handler, `HaylenBridge.unregister(method)` removes it and `HaylenBridge.emit(event, payload)` sends an event, which `HaylenBridge.emit(event, payload, true)` sends retained and `HaylenBridge.emit(event, payload, retain, true)` batched. Handlers run on the main thread, and `HaylenBridge.register(method, handler, HaylenBridge.Threading.BACKGROUND)` runs one on the background thread that such handlers share, for work that does not touch the UI. `params` is what `org.json.JSONTokener` reads from the parameters on the thread of the handler, usually a `JSONObject`, where bytes of the app are `byte[]` values. Plugins register through their context, as the [plugin guide](../plugins.md#the-android-part) describes. The reply of a call has these members:

| Member | Meaning |
| --- | --- |
| `success(value)` | Answers with `null`, a string, a number, a boolean, a `JSONObject`, a `JSONArray`, a map, a collection or an array, with `byte[]` and `ByteBuffer` values anywhere inside, which cross as bytes. |
| `failure(message)` | Fails the call with a message. |
| `failure(message, code, data)` | Fails the call with a code and data, both optional. |
| `failure(throwable)` | Fails the call with a thrown error: a `HaylenBridge.Failure` keeps its code and data, and any other error fails with the code `exception`. |
| `isCancelled()` | Whether the app cancelled the call or its timeout passed. |
| `onCancel(runnable)` | Runs on the main thread when the app cancels the call or its timeout passes, at once when that already happened. |

A handler that throws fails its call through `failure(throwable)` instead of crashing the app, so `throw new HaylenBridge.Failure(message, code, data)` fails a call with a code. `HaylenBridge.activity()` returns the running activity.

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

Kotlin handlers written as suspending functions register with `HaylenCoroutines.register(method) { params -> result }`. Each call runs in a coroutine on the main thread, its result answers the call, a thrown exception fails it like `failure(throwable)`, and the coroutine is cancelled when the app cancels the call or its timeout passes. `HaylenCoroutines` comes with the `dev.haylen:haylen-coroutines` library, which brings `kotlinx-coroutines-android` with it, so an app with such handlers depends on it in `platform/android/app/app.gradle`, as the [native sample](../../samples/system/native) does.

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

`HaylenBridge` in `haylen/platform/apple/HaylenBridge.h` holds the handlers on iOS, tvOS and macOS. `+registerHandler:handler:` adds a handler, `+registerCancellableHandler:handler:` adds one that returns a block to run when the app cancels the call or its timeout passes, `+removeHandler:` removes a handler, `+emit:payload:` sends an event, `+emit:payload:retain:` sends one that waits for the first listener of its name when `retain` is `YES`, and `+emit:payload:retain:batched:` sends one in the list of its frame when `batched` is `YES`. Handlers run on the main queue, and so does the cancel block. `params` is the parsed JSON, an `NSDictionary` for object parameters, where bytes of the app are `NSData` values, and the handler answers with `reply(YES, result)` or `reply(NO, failure)`. `result` is any value `NSJSONSerialization` accepts, with `NSData` values anywhere inside, which cross as bytes, or `nil`, and a success value it rejects fails the call with `The native handler for <method> returned a value that is not JSON.`. A failure passes a message string or a dictionary with a `message` string and optional `code` and `data`, and anything else fails with `The native handler for <method> failed.`. Handlers can be registered at any time, even before the app starts. An event payload that is not JSON is logged as an error and dropped.

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
// The link that opened the app arrives before the app listens, so it waits for the first listener of app.link.
[HaylenBridge emit:@"app.link" payload:@{@"url" : url.absoluteString} retain:YES];
```

Swift handlers written as async functions register with `HaylenBridge.register(method) { (params: Params) async throws -> Result in ... }` from `HaylenBridgeAsync.swift` of the Apple template, where `Params` is `Decodable` and `Result` is `Encodable`. A handler runs on the main actor, a thrown `HaylenFailure(message, code:, data:)` fails the call with its code and data, any other error fails it with the code `exception`, and the task is cancelled when the app cancels the call or its timeout passes. `try HaylenBridge.emit(event, payload, retain: false)` sends an event with an `Encodable` payload. The native parts of plugins register and emit through the context of their plugin instead, which puts the id of the plugin in front of the names, as the [plugin guide](../plugins.md#the-apple-part) describes.

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

`Module.haylen` in the page holds the handlers. `Module.haylen.register(method, handler)` adds or replaces a handler, `Module.haylen.unregister(method)` removes it and `Module.haylen.emit(event, payload, {retain, batched})` sends an event, which waits for the first listener of its name when `retain` is `true` and arrives in the list of its frame when `batched` is `true`. Events sent before the first app starts reach it once it starts. A handler runs after the frame that made the call and receives the parsed parameters, where bytes of the app are `Uint8Array` values, and a context with the `call` id and a `signal`, an `AbortSignal` that aborts when the app cancels the call or its timeout passes, and it returns the result or a promise for it, whose `ArrayBuffer` and `Uint8Array` values cross as bytes. A call cancelled in the frame that made it never runs its handler. A thrown error or a rejected promise fails the call with the error message and the `code` and `data` properties of the error, or with the code `exception` and the `name` of the error in `data.type` when the error has no code. The page registers its handlers before the runtime starts, for example in `Module.preRun`.

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

A native library registers handlers written in C through the `HaylenNativeApi` that `native.load` hands to its init function, on every platform that loads native libraries, answers them and sends events with byte buffers, retained or batched, pushes the video and audio streams of a plugin, and declares itself the native part of a plugin. The [haylen.native reference](native.md#library-handlers) describes them.

## C++ handlers

C++ code registers handlers that run inside the engine with `engine.getPlatform().registerHandler(method, handler)`, where the handler receives the parameters as a `haylen::platform::Bridge::Payload`, JSON with its byte buffers, and a reply function that takes a `haylen::platform::Bridge::Result` with `ok`, a `value` payload and an `error` of `message`, `code` and `data`. `engine.getPlatform().emit(event, payloadJson, buffers, {.retain = true, .batched = true})` sends an event from any thread, `engine.getPlatform().send(method, params)` calls a method whose answer nobody needs, and `engine.getAppPlugins()` returns the plugins of `app.json` as `haylen::platform::AppPlugin` records with `id`, `version`, `config` and `native`. `engine.getScreens()` returns the `haylen::platform::Screens` that open the screens of plugins, as the [plugin guide](../plugins.md#c) describes.

```cpp
engine.getPlatform().registerHandler("save.cloudSync", [](const haylen::platform::Bridge::Payload& params, haylen::platform::Bridge::Reply reply) {
    if (!params.json.contains("slot")) {
        reply({.error = {.message = "save.cloudSync needs a slot.", .code = "missingSlot"}});
        return;
    }
    reply({.ok = true, .value = {.json = {{"synced", params.json.at("slot")}}}});
});
engine.getPlatform().emit("app.link", R"({"url": "island://beach"})");
engine.getPlatform().emit("motion.reading", R"({"x": 1, "y": 2})", {}, {.batched = true});
```
