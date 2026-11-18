-- The app of the plugin check of the web: its plugin requires a feature that the permissions policy of the page refuses and one that the policy does not govern, fails a call with an error of the page, requires a permission whose request ends without a prompt and one that it never registered, and sends events before the app starts, while `haylen test --platform web` reads what the page answered.
local async = require('async')
local log = require('haylen.log')
local plugin = require('page-plugin')
local platform = require('haylen.platform')

local samples = {}
platform.on('page-plugin.sample', function(payload) samples[#samples + 1] = payload.index end)

async.spawn(function()
    local answer = plugin.check({'camera', 'web-share'}):await()
    log.info('The plugin check found these features missing: ' .. table.concat(answer.missing, ', ') .. '.')

    local _, failure = plugin.fail():await()
    log.info(string.format('The error of the page failed with the code "%s" and the type "%s".', tostring(failure.code), tostring(failure.data and failure.data.type)))

    local _, refusal = plugin.useUnprompted():await()
    log.info(string.format('The use without a prompt failed with the code "%s": %s', tostring(refusal.code), refusal.message))

    local _, unregistered = plugin.useUnregistered():await()
    log.info(string.format('The use of an unregistered permission failed with the code "%s": %s', tostring(unregistered.code), unregistered.message))
    log.info(string.format('The app heard %d samples sent before it started, from %d to %d.', #samples, samples[1] or -1, samples[#samples] or -1))
end)
