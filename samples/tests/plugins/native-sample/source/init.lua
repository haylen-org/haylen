-- Lua API of the platform handlers of the plugin `native-sample`, which suspending Kotlin and a throwing Java handler answer on Android, async Swift on Apple platforms and JavaScript on the web. The tests load it with `require('native-sample')`.
local platform = require('haylen.platform')

local handle = platform.plugin('native-sample')
local nativeSample = {}

-- Answers `{greeting, language}` a moment later.
function nativeSample.greet(name)
    return handle:call('greet', {name = name})
end

-- Fails with the code `refused` and the data `{reason = 'requested'}`.
function nativeSample.refuse()
    return handle:call('refuse')
end

-- Throws in the handler, which fails the call with the code `exception` instead of crashing the app.
function nativeSample.explode()
    return handle:call('explode')
end

-- Never answers by itself. When the app cancels the call or its timeout passes, the handler stops and sends `cancelled`.
function nativeSample.slow(options)
    return handle:call('slow', nil, options)
end

-- Calls `listener` with `{language}` whenever a slow handler stopped.
function nativeSample.onCancelled(listener)
    return handle:on('cancelled', listener)
end

return nativeSample
