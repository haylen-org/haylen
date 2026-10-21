-- The base of every screen: it paints the background of the theme, mounts the GUI of the screen, shows the count of its bag buttons and follows the width of the window.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local Shell = require('shell')

local Page = haylen.class('Page', scene.Scene)

function Page:init(params)
    self.params = params or {}
end

-- Mounts the tree of the screen. Every screen shares the focus with the GUIs of the app, so a remote reaches the notices and back.
function Page:mountGui(tree)
    self.gui = ui.mount(tree, {owner = self, sharedFocus = true})
    self.wide = nil
    Shell.showBag(self)
    Shell.adapt(self)
end

function Page:update(dt)
    Shell.adapt(self)
end

function Page:render()
    graphics2d.beginScreen()
    graphics2d.drawRect(viewport.visibleRect(), ui.themeColor('window'))
end

return Page
