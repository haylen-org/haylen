-- The base of every screen: it builds the tree of the screen over the glow of the color of its genre, runs the start of the screen the first time it shows, and tells the screen when the window crosses the wide width.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Covers = require('covers')
local Shell = require('shell')

local Page = haylen.class('Page', ui.Screen)

function Page:init(params)
    self.params = params or {}
end

-- The tree of the screen lies over its glow, an image that fills the area of the screen and fades from the color of the genre into the night of the app, so the glow travels with the screen.
function Page:build()
    local tree = self:tree()
    return ui.stack{align = 'stretch',
        ui.image{id = 'glow', image = self.glow and Covers.glow(self.glow) or Shell.blankCover, visible = self.glow ~= nil, fit = 'fill', align = 'stretch', accessibilityHidden = true},
        tree,
    }
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

-- Shows the glow of another genre, such as the genre of the song that plays.
function Page:setGlow(genreId)
    if self.glow == genreId then
        return
    end
    self.glow = genreId
    self.gui:set('glow', {image = Covers.glow(genreId), visible = true})
end

return Page
