-- Lua API of the plugin of the plugin check, which asks the web part what the context of a web module gives a plugin.
local platform = require('haylen.platform')

local handle = platform.plugin('page-plugin')
local plugin = {}

-- Returns a platform call, whose `await` gives `{missing}` with the features of the permissions policy that the page lacks.
function plugin.check(features)
    return handle:call('check', {features = features})
end

-- Returns a platform call that fails with the error of the page that the web part throws.
function plugin.fail()
    return handle:call('fail')
end

-- Returns a platform call that fails, because the web part requires the permission `unprompted`, whose request ends without a prompt.
function plugin.useUnprompted()
    return handle:call('useUnprompted')
end

-- Returns a platform call that fails, because the web part requires the permission `unregistered`, which it never registered.
function plugin.useUnregistered()
    return handle:call('useUnregistered')
end

return plugin
