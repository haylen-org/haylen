-- The app of the memory check of the web: it downloads a large file of its content in the background, which `haylen.py test --platform web` writes into the content of a copy of this app, and then reads a small asset, while the check measures the memory of the page.
local assets = require('haylen.assets')
local content = require('haylen.content')
local log = require('haylen.log')

require('async').spawn(function()
    local done, message = content.prefetch({'large/blob.bin'}):await()
    if not done then
        log.error('The memory check could not download the large file: ' .. tostring(message))
        return
    end
    log.info('The memory check downloaded the large file and read ' .. assets.json('data/small.json').name .. '.')
end)
