-- The app of the requirement check of the web: its plugin requires a feature that the permissions policy of the page refuses and one that the policy does not govern, while `haylen.py test --platform web` reads which of them the page lacks.
local async = require('async')
local log = require('haylen.log')
local needs = require('page-needs')

async.spawn(function()
    local answer = needs.check({'camera', 'web-share'}):await()
    log.info('The requirement check found these features missing: ' .. table.concat(answer.missing, ', ') .. '.')
end)
