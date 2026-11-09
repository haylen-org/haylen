-- The app of the plugin check of the web: its plugin requires a feature that the permissions policy of the page refuses and one that the policy does not govern, fails a call with an error of the page, requires a permission whose request ends without a prompt and one that it never registered, while `haylen.py test --platform web` reads what the page answered.
local async = require('async')
local log = require('haylen.log')
local plugin = require('page-plugin')

async.spawn(function()
    local answer = plugin.check({'camera', 'web-share'}):await()
    log.info('The plugin check found these features missing: ' .. table.concat(answer.missing, ', ') .. '.')

    local _, failure = plugin.fail():await()
    log.info(string.format('The error of the page failed with the code "%s" and the type "%s".', tostring(failure.code), tostring(failure.data and failure.data.type)))

    local _, refusal = plugin.useUnprompted():await()
    log.info(string.format('The use without a prompt failed with the code "%s": %s', tostring(refusal.code), refusal.message))

    local _, unregistered = plugin.useUnregistered():await()
    log.info(string.format('The use of an unregistered permission failed with the code "%s": %s', tostring(unregistered.code), unregistered.message))
end)
