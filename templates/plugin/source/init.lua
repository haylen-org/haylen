-- Lua API of the {{TITLE}} plugin, which an app loads with `require('{{ID}}')`. The native part of each platform answers its calls and sends its events.
local platform = require('haylen.platform')

local handle = platform.plugin('{{ID}}')
local plugin = {}

-- Sends a message to the native part, which answers with the same message. Returns a platform call, whose `await` gives `{message = message}`.
function plugin.echo(message)
    return handle:call('echo', {message = message})
end

-- Calls `listener` with `{message = message}` after every echo, and returns the connection that stops it.
function plugin.onEchoed(listener)
    return handle:on('echoed', listener)
end

return plugin
