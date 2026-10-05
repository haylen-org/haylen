-- The base of the events tests: while one of them shows, the player data autoload draws its coin counter in a corner of the screen.
local haylen = require('haylen')

local Test = require('harness.test')

local EventsTest = haylen.class('EventsTest', Test)

function EventsTest:frame(options)
    EventsTest.super.frame(self, options)
    haylen.autoloads.playerData.counterShown = true
end

function EventsTest:exit()
    EventsTest.super.exit(self)
    haylen.autoloads.playerData.counterShown = false
end

return EventsTest
