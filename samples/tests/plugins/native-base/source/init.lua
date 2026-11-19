-- Lua API of the plugin `native-base`, which other plugins depend on and which an app loads with `require('native-base')`. Its native part offers the helper `describe`, which the native parts of the plugins that depend on it call directly, and its method answers the app with the same text.
local platform = require('haylen.platform')

local handle = platform.plugin('native-base')
local base = {}

-- Whether the native part of the plugin runs on this platform, which Apple platforms, Android and the web load.
function base.available()
    return handle.native
end

-- Answers `{text, language}`, where `text` is the text with the words of the native part after it, such as "Hello, as the native part of native-base wrote it on Android.".
function base.describe(text)
    return handle:call('describe', {text = text})
end

return base
