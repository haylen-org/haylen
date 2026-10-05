-- Lua API of the native code of the plugin `platform-sample`, which Objective-C answers on Apple platforms, Java on Android and JavaScript on the web. The tests load it with `require('platform-sample')`.
local platform = require('haylen.platform')

local handle = platform.plugin('platform-sample')
local platformSample = {}

-- Sends a text to the native code, which answers with `{echo, characters, language, system}`, or fails when the text is empty.
function platformSample.echo(text)
    return handle:call('echo', {text = text})
end

-- Asks the native code for `count` events `tick`, `interval` milliseconds apart, and answers with `{started, count, interval}`.
function platformSample.ticker(count, interval)
    return handle:call('ticker', {count = count, interval = interval})
end

-- Calls `listener` with `{count, total, source}` for every event `tick`.
function platformSample.onTick(listener)
    return handle:on('tick', listener)
end

-- Calls `listener` with `{state, source}` whenever the native code reports that the app became active or inactive.
function platformSample.onActivity(listener)
    return handle:on('activity', listener)
end

return platformSample
