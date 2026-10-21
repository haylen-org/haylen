# 📖 `haylen.platform`

The module `haylen.platform` connects Lua to native code. Use it to call a method that native code answers, to listen to events that native code sends and to reach the native part of a plugin. Use it for what the engine does not wrap, such as sign-in, purchases, sharing or deep links. The engine wraps some services itself: what the device is, opening urls and vibrating live in [`haylen.system`](system.md), and native message boxes and file pickers live in [`haylen.dialogs`](dialogs.md). The [platform bridge guide](../platform/platform-bridge.md) explains how a call travels and how each platform answers it, the [plugin guide](../platform/plugins.md) shows how plugins use this module, and the [native code guide](../platform/native.md) compares the bridge with the other ways to reach native code.

```lua
local platform = require('haylen.platform')
```

## Values that cross the bridge

Parameters, results and event payloads cross the bridge as JSON. Lua values become JSON by these rules:

- A table whose keys are the integers from `1` up, with no gaps, becomes an array. Any other table becomes an object, and an empty table becomes an empty object.
- The keys of an object are strings. A number key becomes its text, and a key of another type raises `A <type> key cannot be converted to JSON.`.
- A function, a userdata or another value that JSON cannot hold raises `A <type> cannot be converted to JSON.`. The one userdata that crosses is the [`Bytes`](#bytes) mark.
- A table nested deeper than 128 levels raises `Value is nested too deeply to convert to JSON.`.

JSON comes back to Lua by the same rules in reverse. A JSON `null` arrives as `nil`, and an integer too large for a Lua integer arrives as a float.

A string crosses as text, so it must hold UTF-8 to reach native code. Binary data, such as an image or a recording, crosses as a byte buffer next to the JSON instead. Mark the string with [`platform.bytes(data)`](#platformbytesdata), and it crosses as a buffer in its place. Every buffer that native code sends arrives as a Lua string in its place, in results and in events alike.

Results and events reach Lua on the frame thread, at the start of a frame, before the app updates. The [platform bridge guide](../platform/platform-bridge.md#how-a-call-travels) shows the whole road.

## Calls

### platform.call(method, params, options)

Calls the method named `method` and returns a [`PlatformCall`](#platformcall) at once. The table `params` holds the parameters, and they default to an empty object. Inside a coroutine, such as one that `async.spawn` or `scene.spawn` starts, `call:await()` returns the result, or `nil` and a [`PlatformError`](#platformerror).

The result never arrives during `platform.call` itself, even when a Lua handler answers at once. It arrives at the start of a later frame. Every call gets an id that is unique in the whole process. So an answer that comes after the app restarted, such as after a hot reload, never answers a call of the new app, and the engine drops it.

The table `options` is optional:

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `timeout` | number | none | Seconds of real time after which the call fails with the code `timeout`. The native handler hears that the app gave the call up. The time counts while the app is paused too, because the native side keeps working. |

An empty method name raises `A platform call needs a method name.`. A timeout that is not a positive number raises `The timeout of a platform call is a positive number of seconds.`, and an unknown option raises `Unknown option "<key>".`.

```lua
local async = require('async')
local platform = require('haylen.platform')

async.spawn(function()
    local account, err = platform.call('auth.signIn', {prompt = true}, {timeout = 60}):await()
    if account then
        print('Signed in as ' .. account.email .. '.')
    elseif err.code == 'timeout' then
        print('The sign-in took too long.')
    else
        print('The sign-in failed: ' .. err)
    end
end)
```

### platform.send(method, params)

Calls the method named `method` when nothing needs its answer, such as an analytics event. The table `params` defaults to an empty object. The call goes to the same handler as `platform.call`. It creates no call object, never counts in `platform.pendingCallCount()` and drops the answer, a failure included. An empty method name raises `A platform call needs a method name.`.

```lua
local platform = require('haylen.platform')

platform.send('analytics.logEvent', {name = 'levelStart', level = 3})
```

### platform.pendingCallCount()

Returns how many calls still wait for their result. A call whose answer arrived but has not reached Lua at the start of a frame yet still counts.

```lua
local platform = require('haylen.platform')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        self.busy = platform.pendingCallCount() > 0
    end,
})
```

## Events

### platform.on(event, listener, options)

Calls `listener(payload)` every time native code sends the event named `event`. The payload is the JSON of the event as a Lua value, with its bytes as strings. The function returns a [`Connection`](signal.md#connection) of `haylen.signal`. Its method `disconnect()` stops the listener, and its property `connected` is `true` until then. The listener stays connected when Lua drops the connection object.

The table `options` may hold an `owner`, such as a scene, and the listener ends with it, like the listeners of [`haylen.events`](events.md#owners). A listener without an owner, which a module adds at its top level in development, belongs to that module. So a [hot reload](hotReload.md) of the module ends it. An unknown option raises `Unknown option "<key>".`. An error raised inside a listener stops the app and shows the error screen with the message and its stack trace.

Events that no listener waits for are dropped, unless they are retained or batched as below. The engine sends no events of its own, so the names of events are a contract between the app and its native code. Plugins name their events after their id, such as `ads.closed`.

- **Retained events.** Native code retains an event that may come before the app listens, such as the deep link or the notification that opened the app. A retained event that nothing listens to waits, up to 32 per name, and the oldest is dropped first. The first listener of the name receives the waiting events in order at the start of the next frame, before any newer event of that name. Events wait in the running app, so an app that restarts starts with none.
- **Batched events.** Native code marks an event batched when it sends many per frame, such as the readings of a sensor. The listener then runs once per frame with a list of the payloads of that frame, in order. A retained batch waits as the list of its frame.

The [platform bridge guide](../platform/platform-bridge.md#retained-events) shows how each platform sends retained and batched events.

```lua
local platform = require('haylen.platform')

local links = platform.on('app.link', function(payload)
    print('Opened from ' .. payload.url .. '.')
end)

local function leaveMenu()
    links:disconnect()
end

-- A sensor sends its readings batched, so the listener runs once per frame with every reading of the frame.
platform.on('motion.reading', function(readings)
    local last = readings[#readings]
    print(#readings .. ' readings, the last at ' .. last.x .. ', ' .. last.y .. '.')
end)
```

## Stand-ins for native code

Desktop builds and tests often have no native code for a method. These functions let Lua play the part of native code.

### platform.registerHandler(method, handler)

Answers the method named `method` with the Lua function `handler(params)`, which returns the result. The function runs during the `platform.call` that asks for it, so it must return at once. Its result must convert to JSON. Bytes of the parameters arrive as strings, and strings of the result that `platform.bytes` marks cross as bytes.

An error raised inside the handler fails the call with the error message and its stack trace. It does not stop the app. A new handler replaces an earlier engine handler of the same method, and an engine handler goes before every native handler. An empty method name raises `A platform handler needs a method name and a function.`, and a `handler` that is not a function raises a bad argument error.

```lua
local async = require('async')
local haylen = require('haylen')
local platform = require('haylen.platform')

-- Desktop builds and tests have no store, so a Lua stand-in answers there. Phones keep their native handler, which hasHandler cannot see.
local standIn = {windows = true, linux = true, headless = true}
if standIn[haylen.platform] then
    platform.registerHandler('store.buy', function(params)
        if params.item == nil then
            error('The method store.buy needs an item.')
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

Returns `true` when an engine handler answers `method`. That covers methods that `platform.registerHandler` registered and methods that C++ code registered. It does not see native handlers, so it returns `false` for a method that only the platform answers.

```lua
local platform = require('haylen.platform')

platform.registerHandler('score.best', function() return 1200 end)
print(platform.hasHandler('score.best')) -- true
print(platform.hasHandler('store.buy')) -- false
```

### platform.resolve(id, ok, result)

Answers the pending call `id` the way native code does. With `ok` set to `true` the call resolves with `result`. With `ok` set to `false` it fails with `result`, which is a message string or a table with `message`, `code` and `data`. The answer reaches Lua at the start of the next frame, and an id that no pending call has is dropped then. A negative id raises a bad argument error with `expected a non-negative integer`, and a result that JSON cannot hold raises `A <type> cannot be converted to JSON.`.

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

Sends the event `event` with `payload` the way native code does, so `platform.on` listeners receive it at the start of the next frame. The table `options` is optional:

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `retain` | boolean | `false` | The event waits for the first listener of its name, like a retained event of native code. |
| `batched` | boolean | `false` | The event reaches the listeners in the list of its frame, like a batched event of native code. |

A payload that JSON cannot hold raises `A <type> cannot be converted to JSON.`, and an unknown option raises `Unknown option "<key>".`.

```lua
local platform = require('haylen.platform')

platform.emit('app.link', {url = 'island://beach'}, {retain = true})

-- The link was retained, so this listener receives it at the start of the next frame.
platform.on('app.link', function(payload)
    print('Opened from ' .. payload.url .. '.')
end)

-- The three readings reach the listener as one list at the start of the next frame.
platform.on('motion.reading', function(readings) print(#readings) end)
for index = 1, 3 do
    platform.emit('motion.reading', {x = index, y = 0}, {batched = true})
end
```

## Byte buffers

### platform.bytes(data)

Returns a [`Bytes`](#bytes) value that marks the string `data` to cross the bridge as a byte buffer. Use it in the parameters of a call or of a send, in the result of a Lua handler and in an event. Plain strings cross as text, which must be UTF-8, so marking keeps binary data exact. A value that is not a string raises a bad argument error.

```lua
local assets = require('haylen.assets')
local async = require('async')
local platform = require('haylen.platform')

local photo = assets.bytes('photos/beach.png')

async.spawn(function()
    -- The photo crosses as bytes, the name stays text, and the thumbnail comes back as a Lua string of bytes.
    local saved, err = platform.call('gallery.save', {image = platform.bytes(photo), name = 'beach.png'}):await()
    if saved then
        print('The thumbnail takes ' .. #saved.thumbnail .. ' bytes.')
    else
        print('The method gallery.save failed: ' .. err)
    end
end)
```

## Plugins

### platform.plugins()

Returns a list with one table per plugin that `app.json` lists, in the order of their ids. Each table has these fields:

| Field | Type | Meaning |
| --- | --- | --- |
| `id` | string | The id of the plugin. |
| `version` | string | The `version` of its `plugin.json`. |
| `native` | boolean | Whether the native part of the plugin runs on this platform. It is `true` when the platform loaded the native part, or when a native library of the app declared itself that part with [`registerPlugin`](native.md#the-c-interface-of-the-engine). |

A `plugin.json` in the package that is not a JSON object raises `The file "<path>" of the package is not a JSON object.`, and one without a version raises `The file "<path>" of the package has no version.`.

```lua
local platform = require('haylen.platform')

for _, plugin in ipairs(platform.plugins()) do
    print(plugin.id .. ' ' .. plugin.version .. (plugin.native and '' or ' without its native part'))
end
```

### platform.plugin(id)

Returns the [`AppPlugin`](#appplugin) handle of the plugin `id` that `app.json` lists. The Lua modules of a plugin use it to reach its native part. An id that `app.json` does not list raises `The plugin "<id>" is not among the plugins of "app.json".`.

```lua
local platform = require('haylen.platform')

local ads = platform.plugin('ads')
print(ads.id, ads.version, ads.native, ads.config.testMode)
```

### platform.screenShowing()

Returns `true` while the [screen](#screens) of a plugin shows over the app, whichever app of the process opened it. It is `true` from the moment the app asked for the screen. A screen that waits for the app to be `'active'` does not show yet, so it does not count. An app that restarted under a screen starts covered, and this function tells it why. The end of that screen arrives as `screenRestored`.

```lua
local haylen = require('haylen')
local platform = require('haylen.platform')

if platform.screenShowing() then
    print('A screen of a plugin still shows. Covered: ' .. tostring(haylen.appCovered()) .. '.')
end
```

## PlatformCall

A `PlatformCall`, named `haylen.PlatformCall` in messages, is a call that has not settled yet or that settled. The functions `platform.call` and `handle:openScreen` return one, and so does every function of [`haylen.dialogs`](dialogs.md).

### call:await()

Waits inside a coroutine and returns the result, or `nil` and a [`PlatformError`](#platformerror). Awaiting a call that already settled returns at once.

```lua
local async = require('async')
local platform = require('haylen.platform')

async.spawn(function()
    local levels, err = platform.call('store.downloadLevels', {pack = 2}):await()
    print(levels and #levels or err.code)
end)
```

### call:cancel()

Gives the call up. It returns `true` when the call was still pending, and `false` when it had already settled, which leaves the call as it was. A cancelled call fails with the code `cancelled` at the start of the next frame, and the native handler hears it, so it may stop its work. A cancelled call never stops the app when nothing awaits it. A call that fails any other way while nothing awaits it stops the app, as an [unobserved rejection](../core/lua.md#asynchronous-code) does.

```lua
local platform = require('haylen.platform')

local download = platform.call('store.downloadLevels', {pack = 2})

local function leaveShop()
    download:cancel()
end
```

### Properties

| Property | Type | Meaning |
| --- | --- | --- |
| `id` | integer | The id of the call, unique in the process, which `platform.resolve` takes. |
| `done` | boolean | Whether the call has settled. |
| `promise` | promise | The Varn promise of the call, for the combinators of `async` such as `async.all` and `async.race`. A failure of the promise is only the message. |

The properties are read-only.

```lua
local async = require('async')
local platform = require('haylen.platform')

local profile = platform.call('profile.load', {id = 'me'})
print(profile.id, profile.done)

async.spawn(function()
    local both = async.all({profile.promise, platform.call('store.products').promise}):await()
    print(both[1].name .. ' sees ' .. #both[2] .. ' products.')
end)
```

## PlatformError

A `PlatformError`, named `haylen.PlatformError` in messages, is the error of a call that failed, which `call:await()` returns after `nil`. It is a table with three fields. It reads as its message in `tostring` and in string concatenation, so `'Failed: ' .. err` shows the message.

| Field | Type | Meaning |
| --- | --- | --- |
| `message` | string | What went wrong, for people. |
| `code` | any JSON value or `nil` | What went wrong, for code: the code that the native handler gave, or one of the codes below. |
| `data` | any JSON value or `nil` | Details that the native handler added, such as a retry delay. |

The engine and the platforms fail calls with these codes:

| Code | When |
| --- | --- |
| `timeout` | The `timeout` of the call passed first. |
| `cancelled` | The method `call:cancel()` gave the call up. |
| `noHandler` | No handler answers the method. The message is `No native handler is registered for "<method>".`, or `No page handler is registered for "<method>".` on the web. |
| `invalidJson` | Native code answered with text that is not JSON. The message is `The platform returned invalid JSON.`. |
| `invalidBytes` | Native code answered with JSON that refers to a byte buffer it did not send, with a message such as `The JSON refers to byte buffer 2, but only 1 came with it.`. |
| `exception` | A Java, Kotlin, Swift or JavaScript handler threw an error without a code of its own. The field `data.type` names the class of the exception or the type of the error. |
| `unsupported` | The platform cannot serve the call, or the project of the app lacks what the call needs. That may be a permission of the Android manifest, a usage description of the `Info.plist` or an API of the browser. In the second case `data.missing` lists each missing requirement as `{kind, name, file, snippet}`, as the [plugin guide](../platform/plugins.md#requirements) describes. |

A native failure that is a string becomes the message. A failure object gives its `message`, `code` and `data`. A failure without a string `message`, such as `null` or a number, fails with `The native platform call failed without a message.`. A Lua handler of `platform.registerHandler` fails with the error it raised and its stack trace as the message.

```lua
local async = require('async')
local platform = require('haylen.platform')

async.spawn(function()
    local _, err = platform.call('store.buy', {item = 'coins_100'}):await()
    if err and err.code == 'declined' then
        print('Try another card: ' .. tostring(err))
    elseif err then
        print('The method store.buy failed with ' .. tostring(err.code) .. ': ' .. err.message)
    end
end)
```

## AppPlugin

An `AppPlugin`, named `haylen.AppPlugin` in messages, is the handle of a plugin that [`platform.plugin(id)`](#platformpluginid) returns. The Lua API of a plugin talks to its native part through it on every platform. Its methods put the id of the plugin and a dot in front of the names they take, which is how the native parts of plugins register them. The [plugin guide](../platform/plugins.md#the-lua-api-of-a-plugin) shows the Lua API of a whole plugin.

A platform without the native part of the plugin answers its calls with the code `noHandler`. So a plugin checks `handle.native` when it can do without its native part. An empty method or event name raises `A method or event of the plugin "<id>" needs a name.`.

### Properties

| Property | Type | Meaning |
| --- | --- | --- |
| `id` | string | The id of the plugin, such as `'ads'`. |
| `version` | string | The `version` of its `plugin.json`. |
| `config` | table | The parameter values of the plugin in `app.json`, over the `default` of every parameter of its `plugin.json`. The native parts receive the same values. |
| `native` | boolean | Whether the native part of the plugin runs on this platform. The handle reads it anew every time, since a native library may declare the native part later. |

The properties are read-only.

### handle:call(method, params, options)

Works like `platform.call(id .. '.' .. method, params, options)` and returns a [`PlatformCall`](#platformcall).

### handle:send(method, params)

Works like `platform.send(id .. '.' .. method, params)`.

### handle:on(event, listener, options)

Works like `platform.on(id .. '.' .. event, listener, options)` and returns a connection.

```lua
-- plugins/ads/source/init.lua
local platform = require('haylen.platform')

local handle = platform.plugin('ads')
local ads = {}

function ads.showBanner(placement)
    return handle:call('showBanner', {placement = placement or handle.config.placement})
end

function ads.logImpression(name)
    handle:send('logImpression', {name = name})
end

function ads.onClosed(listener)
    return handle:on('closed', listener)
end

return ads
```

### handle:videoStream(name)

Returns the [`VideoStream`](#videostream) `name` that the native part of the plugin opened, or `nil` until the native part opens it. From the first call that returns it, the engine keeps its texture current until the app stops.

### handle:audioStream(name)

Returns the [`AudioStream`](#audiostream) `name` that the native part of the plugin opened, or `nil` until the native part opens it.

### handle:openScreen(name, params, options)

Opens the screen `name` of the plugin, which covers the app until it ends, and returns a [`PlatformCall`](#platformcall) that the result of the screen settles. The [screens](#screens) section lists its options and codes.

## Screens

A screen is native UI of a plugin that takes over the app until it ends with one result, such as a paywall, a sign-in flow or a payment page. The [plugin guide](../platform/plugins.md#plugin-screens) explains how a screen covers the app, waits for it to be active and survives a restart. This section lists the API.

The method `handle:openScreen(name, params, options)` opens the screen `name` with `params`. The parameters default to an empty object and cross like the parameters of a call, bytes included. The method returns a [`PlatformCall`](#platformcall) at once. Its `await` returns the result of the screen, or `nil` and a [`PlatformError`](#platformerror).

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `state` | any JSON value without bytes | none | A value that the platform keeps with the screen, where it survives the end of the process. The platform hands it back with a restored end. |
| `opaque` | boolean | `true` | Whether the screen hides the app completely. The engine draws nothing under an opaque screen and keeps the last frame on screen. Under a screen that lets the app show through, such as a sheet, it draws the halted app once and keeps that frame. On Apple platforms a controller with the automatic presentation style presents full screen for an opaque screen and as a sheet otherwise. |
| `timeout` | number | none | Seconds of real time after which the call fails with the code `timeout`, and the platform dismisses the screen. Without it a screen has no time limit. |

A screen fails with the codes of [`PlatformError`](#platformerror) and with these:

| Code | When |
| --- | --- |
| `busy` | Another screen shows or waits to show. An earlier app of the process may have opened it. |
| `notActive` | The app is in the background, went there while the screen waited, or the platform cannot present the screen at that moment. |
| `cancelled` | The method `call:cancel()` gave the screen up, or the person closed the screen. |
| `noHandler` | No screen of that name is registered. That means no page screen on the web, no screen of a plugin class on Apple platforms and Android, and no screen of a native library on Windows and Linux. |
| `unsupported` | The device cannot show the screen the way the plugin asks, such as a window of its own on an iPhone. |
| `popupBlocked` | The browser blocked the popup of a web screen. This happens when the tap that asked for it is too long ago. |

When the app restarts under a screen, or the process ends while it shows, the end of the screen reaches the next app as the retained event `screenRestored` of the plugin. Listen to it with `handle:on('screenRestored', listener)`. The payload has these fields:

| Field | Meaning |
| --- | --- |
| `screen` | The name of the screen. |
| `state` | The value of `options.state`. |
| `result` | The result of the screen, when it ended well. |
| `error` | A table with `message`, `code` and `data`, when the screen failed. |

An empty name raises `A screen of the plugin "<id>" needs a name.`. A timeout that is not a positive number raises `The timeout of a screen is a positive number of seconds.`. A state with bytes raises `The state of a screen is JSON without bytes.`, and an unknown option raises `Unknown option "<key>".`.

```lua
local async = require('async')
local platform = require('haylen.platform')

local paywall = platform.plugin('paywall')

-- The app restarted while the paywall showed, so the end arrives with the state that the earlier app gave.
paywall:on('screenRestored', function(ending)
    if ending.result then
        print('Bought ' .. ending.result.product .. ' on level ' .. ending.state.level .. '.')
    end
end)

async.spawn(function()
    local purchase, err = paywall:openScreen('offer', {offering = 'gold'}, {state = {level = 12}, timeout = 300}):await()
    if purchase then
        print('Bought ' .. purchase.product .. '.')
    elseif err.code == 'cancelled' then
        print('The player closed the paywall.')
    else
        print('The paywall failed: ' .. err)
    end
end)
```

A screen may open right in the answer of a dialog. That answer arrives while the app is still `'inactive'` on some platforms, so the screen then waits until the app is `'active'`.

```lua
local async = require('async')
local dialogs = require('haylen.dialogs')
local platform = require('haylen.platform')

local paywall = platform.plugin('paywall')

async.spawn(function()
    local button = dialogs.message({text = 'Unlock every level?', buttons = {'See the offer', 'Later'}}):await()
    if button == 1 then
        local purchase, err = paywall:openScreen('offer', {offering = 'levels'}, {timeout = 300}):await()
        print(purchase and purchase.product or err.code)
    end
end)
```

## VideoStream

A `VideoStream`, named `haylen.VideoStream` in messages, shows the frames that the native part of a plugin pushes, such as a camera or a video decoder. Get one with [`handle:videoStream(name)`](#handlevideostreamname). The native part pushes frames from any thread. The engine keeps only the newest frame and uploads it into the texture of the stream at the start of a frame when it is new. So the texture changes at most once per frame, however fast the frames come. A frame of another size resizes the texture in place, so a sprite or a material that holds the texture keeps showing the current frame. The texture samples linearly.

A stream belongs to the process, like the native code that pushes into it. So a stream outlives an app that restarts, and the next app receives the newest frame again. Two values of the same stream are equal with `==`. The [plugin guide](../platform/plugins.md#streams) shows how native code pushes frames on each platform.

### Properties

| Property | Type | Meaning |
| --- | --- | --- |
| `texture` | `Texture` | The texture of the stream. Until the first frame arrives it holds transparent pixels of the size the native part opened the stream with, or a single pixel. |
| `width`, `height` | integer | The size of the frame that the texture shows, or the size the stream opened with before the first frame. |
| `frameCount` | integer | How many frames the texture received. Frames that newer ones replaced before a frame of the app do not count. |
| `timestamp` | number | The timestamp of the frame that the texture shows, in the seconds of the native part. |

The properties are read-only.

### stream:on(event, listener, options)

Calls `listener(timestamp)` whenever the texture receives a frame, and returns a connection. The only event is `'frame'`, and any other name raises `Unknown video stream event "<name>". Video streams report "frame".`. The table `options` may hold an `owner`, like the listeners of [`haylen.events`](events.md#owners). A listener without one belongs to the module that adds it at its top level.

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
            print(self.preview.frameCount, self.preview.timestamp)
        end
    end,
})
```

## AudioStream

An `AudioStream`, named `haylen.AudioStream` in messages, plays the samples that the native part of a plugin pushes, such as a microphone, a synthesized voice or decoded network audio. Get one with [`handle:audioStream(name)`](#handleaudiostreamname). The native part pushes samples into a ring from any thread, and the stream plays as a voice of [`haylen.audio`](audio.md). The voice resamples the stream to the rate of the mixer. It plays silence where samples have not arrived, and it counts each read that found the ring short as an underrun. It never ends by itself, so `audio.stop` ends it. Two values of the same stream are equal with `==`.

### stream:play(options)

Starts a voice that plays the stream and returns its id, which every voice function of `haylen.audio` takes. The table `options` takes the options of [`audio.play`](audio.md#audioplaysound-options) that suit a live stream: `bus`, `volume`, `pan`, `fadeIn`, `x`, `y`, `processMode` and `effects`. Any other key raises `Unknown option "<key>".`. A new voice takes the stream over from the voice that played it before, which plays silence from then on.

### stream:read(buffer)

Copies the newest samples that native code pushed into a float buffer of [`haylen.collections`](collections.md#float-buffers). The samples are interleaved by channel, in whole frames, oldest first from the first value. The method returns how many values it wrote, which is fewer while less has arrived. Use it for level meters and waveforms, whether a voice plays the stream or not.

### Properties

| Property | Type | Meaning |
| --- | --- | --- |
| `sampleRate` | integer | The sample rate of the stream in hertz. |
| `channels` | integer | The number of channels of the stream. |
| `underruns` | integer | How many reads of its voices found fewer samples than they needed, since samples started to arrive. |

The properties are read-only.

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
            print(self.stream.sampleRate .. ' Hz, ' .. self.stream.channels .. ' channels.')
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
            self.dropouts = self.stream.underruns
        end
    end,
    exit = function(self)
        if self.voice then
            audio.stop(self.voice)
        end
    end,
})
```

## Bytes

A `Bytes` value, named `haylen.Bytes` in messages, is a string marked to cross the bridge as a byte buffer. Make one with [`platform.bytes(data)`](#platformbytesdata). The mark holds a copy of the bytes. Only the bridge takes it, so any other conversion to JSON of the engine raises `A userdata cannot be converted to JSON.`.

### Properties

| Property | Type | Meaning |
| --- | --- | --- |
| `size` | integer | How many bytes the mark holds. It is read-only. |

```lua
local platform = require('haylen.platform')

local marked = platform.bytes('\0\1\2\3')
print(marked.size) -- 4
```

## Errors

These messages are raised by the functions and methods of the module.

| Message | Cause |
| --- | --- |
| `A platform call needs a method name.` | The method name of `platform.call`, `platform.send` or a handle is empty. |
| `The timeout of a platform call is a positive number of seconds.` | The option `timeout` of a call is not a positive number. |
| `A platform handler needs a method name and a function.` | The method name of `platform.registerHandler` is empty. |
| `A <type> cannot be converted to JSON.` | A parameter, a result or a payload holds a value that JSON cannot hold. |
| `A <type> key cannot be converted to JSON.` | A table has a key that is neither a string nor a number. |
| `Value is nested too deeply to convert to JSON.` | A table is nested deeper than 128 levels. |
| `Unknown option "<key>".` | An options table has a key that the function does not take. |
| `The plugin "<id>" is not among the plugins of "app.json".` | The id of `platform.plugin` names no plugin of the app. |
| `The file "<path>" of the package is not a JSON object.` | A `plugin.json` of the package is not a JSON object. |
| `The file "<path>" of the package has no version.` | A `plugin.json` of the package has no `version`. |
| `A method or event of the plugin "<id>" needs a name.` | The name given to a method of a handle is empty. |
| `A screen of the plugin "<id>" needs a name.` | The name of `handle:openScreen` is empty. |
| `The timeout of a screen is a positive number of seconds.` | The option `timeout` of a screen is not a positive number. |
| `The state of a screen is JSON without bytes.` | The option `state` of a screen holds bytes. |
| `Unknown video stream event "<name>". Video streams report "frame".` | The method `stream:on` of a video stream names another event. |
