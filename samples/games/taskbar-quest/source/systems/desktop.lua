-- The two ways the quest sits on the desktop: a frameless transparent strip above the taskbar that lets clicks through its empty parts, or a normal opaque window.
local events = require('haylen.events')
local haylen = require('haylen')
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local config = require('config')

local desktop = {}
desktop.__index = desktop

-- The strip mode comes from `app.json`, so the mode the app starts in is whatever the window has. The strip follows the taskbar and the monitors while the owner lives.
function desktop.new(owner)
    local self = setmetatable({strip = not window.decorated()}, desktop)
    events.on('windowMonitorsChanged', function()
        if self.strip then
            window.place({anchor = 'bottom', fill = 'width'})
        end
    end, {owner = owner})
    return self
end

function desktop:toggle()
    if self.strip then
        self:showWindow()
    else
        self:showStrip()
    end
end

function desktop:showWindow()
    self.strip = false
    window.setTransparent(false)
    window.setDecorated(true)
    window.setAlwaysOnTop(false)
    window.setFocusable(true)
    window.setResizable(true)
    window.setMousePassthrough(false)
    local frame = window.frame()
    window.setFrame(frame.x, frame.y, config.window.width, config.window.height)
    window.place('center')
    viewport.setDesignSize(config.window.designWidth, config.window.designHeight)
end

function desktop:showStrip()
    self.strip = true
    window.setResizable(false)
    window.setDecorated(false)
    window.setTransparent(true)
    window.setAlwaysOnTop(true)
    window.setFocusable(false)
    local frame = window.frame()
    window.setFrame(frame.x, frame.y, frame.width, haylen.config.window.height)
    window.place({anchor = 'bottom', fill = 'width'})
    viewport.setDesignSize(haylen.config.design.width, haylen.config.design.height)
end

-- Gives the strip the regions that keep the mouse this frame, so a click anywhere else reaches the desktop behind it.
function desktop:keepMouse(regions)
    if self.strip then
        window.setMousePassthrough(regions)
    end
end

return desktop
