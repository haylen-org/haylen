-- Lua API of the {{TITLE}} plugin, which an app loads with `require('{{ID}}')`. The native part of each platform answers its calls and sends its events.
local haylen = require('haylen')
local native = require('haylen.native')
local platform = require('haylen.platform')

local handle = platform.plugin('{{ID}}')
local plugin = {}

local kDesktops = {macos = true, windows = true, linux = true}

-- Apple platforms, Android and the web load their native parts before any Lua runs. The desktop player and the Windows and Linux apps run the C library of `native/`, which `haylen.py` places next to them and which declares itself the native part of the plugin in its init function, once per process.
if not handle.native and kDesktops[haylen.platform] then
    native.load('{{SYMBOL}}', {init = '{{SYMBOL}}_haylen_init'})
end

-- Sends a message to the native part, which answers with the same message. Returns a platform call, whose `await` gives `{message = message}`.
function plugin.echo(message)
    return handle:call('echo', {message = message})
end

-- Calls `listener` with `{message = message}` after every echo, and returns the connection that stops it.
function plugin.onEchoed(listener)
    return handle:on('echoed', listener)
end
{{BLOCK:LUA_API}}

return plugin
