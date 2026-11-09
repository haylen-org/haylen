-- Lua API of the plugin of the requirement check, which asks the web part what the page lacks of the features of the permissions policy that it names.
local platform = require('haylen.platform')

local handle = platform.plugin('page-needs')
local needs = {}

-- Returns a platform call, whose `await` gives `{missing}` with the features that the page lacks.
function needs.check(features)
    return handle:call('check', {features = features})
end

return needs
