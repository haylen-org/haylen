-- The base of every screen: it paints the night of the app and, when a screen names a genre, the glow of its color at the top, mounts the GUI of the screen, follows the width of the window and keeps the player of the frame current.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local Covers = require('covers')
local Shell = require('shell')

local Page = haylen.class('Page', scene.Scene)

-- The share of the height of the screen the glow of a genre covers.
Page.glowHeight = 0.7

function Page:init(params)
    self.params = params or {}
end

-- Mounts the tree of the screen. Every screen shares the focus with the GUIs of the app, so a remote reaches the notices and back.
function Page:mountGui(tree)
    self.gui = ui.mount(tree, {owner = self, sharedFocus = true})
    self.wide = nil
    Shell.adapt(self)
end

function Page:update(dt)
    Shell.adapt(self)
    Shell.showPlayer(self)
end

function Page:render()
    graphics2d.beginScreen()
    local visible = viewport.visibleRect()
    graphics2d.drawRect(visible, ui.themeColor('window'))
    if self.glow then
        graphics2d.draw(Covers.backdrop(self.glow), visible.x, visible.y, {width = visible.width, height = visible.height * Page.glowHeight, pivotX = 0, pivotY = 0})
    end
end

return Page
