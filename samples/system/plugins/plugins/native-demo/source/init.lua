-- Lua API of the Native Demo plugin, which an app loads with require('native-demo'). Swift on Apple platforms, Kotlin on Android, JavaScript on the web and a C library on the desktops answer its calls and send its events, with the APIs of each platform alone.
local haylen = require('haylen')
local native = require('haylen.native')
local platform = require('haylen.platform')

local handle = platform.plugin('native-demo')
local demo = {}

local kDesktops = {macos = true, windows = true, linux = true}

-- Apple platforms, Android and the web load their native parts before any Lua runs. The desktop player and the Windows and Linux apps run the C library of native/, which make.py places next to them and which declares itself the native part of the plugin in its init function, once per process.
if not handle.native and kDesktops[haylen.platform] then
    native.load('native_demo', {init = 'native_demo_haylen_init'})
end

-- Every app that loads the module tells the native part that it started, which then stops what an earlier app of the process left running and sends lastError when that app stopped with an error.
if handle.native then
    handle:send('start')
end

demo.id = handle.id
demo.version = handle.version

-- Whether the native part of the plugin runs on this platform. Every other function needs it, and calls without it fail with the code noHandler.
function demo.available()
    return handle.native
end

-- The parameters of the plugin: the values of app.json over the defaults of plugin.json.
function demo.config()
    return handle.config
end

-- Sends any JSON value to the native part, which answers on the main thread with {echo, thread, language}.
function demo.echo(value)
    return handle:call('echo', {value = value})
end

-- Sends the bytes of a string to the native part as a byte buffer, and the native part answers with {data, size, thread, language}, where data holds the same bytes.
function demo.echoBytes(data)
    return handle:call('echoBytes', {data = platform.bytes(data)})
end

-- Draws an image of width by height pixels natively and answers with {png, width, height, drawnWith, language}, where png holds the bytes of a PNG file.
function demo.generatedImage(width, height)
    return handle:call('generatedImage', {width = width, height = height})
end

-- Counts the primes below limit on a background thread and answers with {primes, thread, detail, language}.
function demo.compute(limit)
    return handle:call('compute', {limit = limit})
end

-- Always fails with the code demoFailure and data {reason, language}.
function demo.fail()
    return handle:call('fail')
end

-- Never answers by itself. When the app cancels the call or its timeout passes, the native part hears it and sends waitCancelled with the token.
function demo.wait(token, options)
    return handle:call('wait', {token = token}, options)
end

function demo.onWaitCancelled(listener)
    return handle:on('waitCancelled', listener)
end

-- Starts or stops the native timer that sends tick every tickInterval seconds, and answers with {enabled, interval}. The Lua API passes the interval, since the desktop part receives no parameters.
function demo.setTicking(enabled)
    return handle:call('ticks', {enabled = enabled, interval = handle.config.tickInterval})
end

-- Calls listener with {count, thread, language} for every tick of the native timer.
function demo.onTick(listener)
    return handle:on('tick', listener)
end

-- Sends count batched events of the name burst 30 times per second for ticks ticks, and then burstDone with {events, ticks, language}. The events of one frame reach the listener of onBurst as one list of {tick, index, language}.
function demo.burst(count, ticks)
    return handle:call('burst', {count = count, ticks = ticks})
end

function demo.onBurst(listener)
    return handle:on('burst', listener)
end

function demo.onBurstDone(listener)
    return handle:on('burstDone', listener)
end

-- Starts the animated pattern that the native part draws 30 times per second into the video stream pattern, and answers with {width, height, fps, format, language}.
function demo.startVideo()
    return handle:call('startVideo')
end

function demo.stopVideo()
    return handle:call('stopVideo')
end

-- The video stream of the pattern, or nil until the native part opened it.
function demo.videoStream()
    return handle:videoStream('pattern')
end

-- Starts a sine wave of the frequency in hertz that the native part synthesizes into the audio stream tone, and answers with {frequency, sampleRate, channels, format, language}.
function demo.startTone(frequency)
    return handle:call('startTone', {frequency = frequency})
end

function demo.stopTone()
    return handle:call('stopTone')
end

-- The audio stream of the tone, or nil until the native part opened it.
function demo.audioStream()
    return handle:audioStream('tone')
end

-- The native part sends loaded retained once it loads, so the first listener receives {language, platform} however late it connects.
function demo.onLoaded(listener)
    return handle:on('loaded', listener)
end

-- Answers with the parameters as the native part received them from the platform.
function demo.nativeConfig()
    return handle:call('config')
end

-- Shows the native banner, or places it again, at the anchor 'top' or 'bottom', reserving its edge of the screen when reserve is true. Answers with {anchor, reserve, visible}.
function demo.showBanner(anchor, reserve)
    return handle:call('showBanner', {anchor = anchor, reserve = reserve})
end

function demo.setBannerVisible(visible)
    return handle:call('setBannerVisible', {visible = visible})
end

function demo.removeBanner()
    return handle:call('removeBanner')
end

-- Calls listener with {count} whenever the native button of the banner is tapped.
function demo.onBannerTapped(listener)
    return handle:on('bannerTapped', listener)
end

-- Shows a native screen over the whole app, which covers it until the Close button closes the screen. Answers with {seconds} once it closed.
function demo.showScreen(title)
    return handle:call('showScreen', {title = title})
end

-- Opens the confirm screen of the plugin, which covers the app while it shows: a UIKit controller on iOS, iPadOS, Mac Catalyst and tvOS, a sheet on macOS, an AndroidX activity of its own on Android, a popup page on the web and a native window over the window of the app on the desktops. Answers with `{confirmed, via, language}` once the person answers, and fails with the code `cancelled` when the person closes the screen. The table `options` takes the `state` that comes back with a restored end, `opaque` and `timeout`, like `handle:openScreen`.
function demo.openScreen(options)
    return handle:openScreen('confirm', {title = 'Native Demo', question = 'Does the app get an answer?'}, options)
end

-- Opens the same question in SwiftUI, which only Apple platforms have: over the app on iOS, iPadOS and tvOS and in a window of its own on Mac Catalyst and macOS. It answers like `openScreen`, and its Close button dismisses it through SwiftUI, which fails the call with the code `cancelled`.
function demo.openSwiftUIScreen(options)
    return handle:openScreen('swiftUI', {title = 'SwiftUI', question = 'Does the SwiftUI screen answer the app?'}, options)
end

-- Opens the confirm screen of the web by leaving the page for a page of the plugin, which comes back with the answer in its address. The page loads again, so the answer reaches the new app as screenRestored with the state.
function demo.openRedirectScreen(options)
    return handle:openScreen('redirect', {title = 'Native Demo', question = 'Does the app get an answer after the page loads again?'}, options)
end

-- Calls listener with {screen, state, result} or {screen, state, error} of a screen whose app restarted, or whose page loaded again, before it ended. The event is retained, so the first listener receives it however late it connects.
function demo.onScreenRestored(listener)
    return handle:on('screenRestored', listener)
end

-- Lets the person pick a file with the file picker of the platform and answers with {name}, or nil when the picker was cancelled.
function demo.pickFile()
    return handle:call('pickFile')
end

-- Asks the person for the permission `kind`, `camera` or `notifications`, and answers with `{kind, granted, status, language}`. The camera prompt shows the usage description of the `cameraUsage` parameter on Apple platforms.
function demo.requestPermission(kind)
    return handle:call('requestPermission', {kind = kind})
end

-- Schedules a local notification of the plugin after `seconds` and answers with `{identifier, seconds, language}`.
function demo.notify(seconds)
    return handle:call('notify', {seconds = seconds, title = 'Native Demo', body = 'Tap to open the plugins sample.'})
end

-- Calls `listener` with `{identifier, title, action, language}` when the person taps a notification of the plugin. The native part sends `notificationOpened` retained, so the tap that launched the app reaches the first listener.
function demo.onNotificationOpened(listener)
    return handle:on('notificationOpened', listener)
end

-- Calls a method whose native part needs something that the plugin leaves out of the project of the app on purpose, such as the permission `android.permission.READ_CONTACTS` on Android. It fails with the code `unsupported` and lists each missing requirement in `data.missing` as `{kind, name, file, snippet}`, and answers with `{met, language}` in an app whose project adds it.
function demo.requirementCheck()
    return handle:call('requirementCheck')
end

-- Calls listener with {url} for every URL that opens the app. The native part sends urlOpened retained, so the URL that launched the app reaches the first listener.
function demo.onUrlOpened(listener)
    return handle:on('urlOpened', listener)
end

-- Calls listener with {message, file, line, language} of the error that stopped the previous app of the process, which the native part sends retained when the next app starts.
function demo.onLastError(listener)
    return handle:on('lastError', listener)
end

return demo
