-- The base of every screen: it builds the tree of the screen, runs the start of the screen the first time it shows, and tells the screen when the window crosses the widths of the wide and roomy layouts.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Shell = require('shell')

local Page = haylen.class('Page', ui.Screen)

function Page:init(params)
    self.params = params or {}
end

function Page:build()
    return self:tree()
end

-- The screen lays out for the width of the window and starts the first time it shows.
function Page:appear()
    Shell.adapt(self)
    if self.started then
        return
    end
    self.started = true
    if self.start then
        self:start()
    end
end

function Page:update(dt)
    Shell.adapt(self)
end

return Page
