-- The app of the plugin check of the web: its plugin requires a feature that the permissions policy of the page refuses and one that the policy does not govern, and fails a call with an error of the page, while `haylen.py test --platform web` reads what the page answered.
local async = require('async')
local log = require('haylen.log')
local plugin = require('page-plugin')

async.spawn(function()
    local answer = plugin.check({'camera', 'web-share'}):await()
    log.info('The plugin check found these features missing: ' .. table.concat(answer.missing, ', ') .. '.')

    local _, failure = plugin.fail():await()
    log.info(string.format('The error of the page failed with the code "%s" and the type "%s".', tostring(failure.code), tostring(failure.data and failure.data.type)))
end)
