-- Ride Hailing: rides across a made-up city whose day map fills the screen, with live traffic, the search of a destination, six kinds of ride, the whole trip from the search for a driver to the rating, the past trips, the wallet and the account behind a menu, in a sunny look of white sheets, ink and yellow.
local events = require('haylen.events')
local haylen = require('haylen')
local system = require('haylen.system')
local ui = require('haylen.ui')

local Style = require('style')

Style.register()

local Shell = require('shell')
local Tour = require('tour')

-- The interface keeps its size in points on every screen, and a television, seen from the couch, draws it half as large again.
if system.info().deviceKind == 'tv' then
    ui.setScale(1.5)
end
ui.setTheme('rides')

events.on('notice', function(text)
    Shell.notify(text)
end)

Shell.start()

if haylen.platform == 'headless' then
    Tour.start()
end
