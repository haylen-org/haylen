-- The base of every screen: it builds the tree of the screen in the safe area, over the background of the theme for a page or over the map for a step of a ride, runs the start of the screen the first time it shows and tells the screen when the window crosses the wide width.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Shell = require('shell')

local Page = haylen.class('Page', ui.Screen)

Page.background = {colors = {panel = 'window'}, metrics = {panelRadius = 0}}

function Page:init(params)
    self.params = params or {}
end

-- A step of a ride, which sets `onMap`, draws the map behind its GUI, and a page covers the screen with the background of the theme.
function Page:build()
    local tree = ui.safeArea{self:tree()}
    if self.onMap then
        return tree
    end
    return ui.stack{align = 'stretch', ui.panel{padding = 0, align = 'stretch', style = Page.background}, tree}
end

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
