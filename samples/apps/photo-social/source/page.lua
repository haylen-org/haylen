-- The base of every screen: it paints the background of the theme, mounts the GUI of the screen in the middle of the frame and follows the width of the window.
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

-- Mounts the tree of the screen in the middle of the frame. Every screen shares the focus with the frame and the notices, so a remote moves between the screen and the tabs, and cancel goes back.
function Page:mountGui(tree)
    tree.onCancel = tree.onCancel or Shell.back
    self.gui = ui.mount(tree, {owner = self, sharedFocus = true, placement = Shell.middle()})
    self.wide, self.roomy = nil, nil
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
