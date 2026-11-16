-- The two ways the quest sits on the desktop: a frameless transparent strip above the taskbar that lets clicks through its empty parts, or a normal opaque window.
local events = require('haylen.events')
local haylen = require('haylen')
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local config = require('config')

local desktop = {}
desktop.__index = desktop

-- Only the desktops of Windows, macOS and Linux float a frameless window above the others, as the window reference says. Everywhere else, such as on a web page, the window fills the page and the quest shows in the window mode alone.
desktop.stripPlatforms = {windows = true, macos = true, linux = true}

-- The strip mode comes from `app.json`, so on a desktop the app starts in whatever mode the window has. The strip follows the taskbar and the displays while the owner lives.
function desktop.new(owner)
    local canStrip = desktop.stripPlatforms[haylen.platform] == true
    local self = setmetatable({canStrip = canStrip, strip = canStrip and not window.decorated()}, desktop)
    if not canStrip then
        viewport.setDesignSize(config.window.designWidth, config.window.designHeight)
    end
    for _, name in ipairs({'displayConnected', 'displayDisconnected', 'displayChanged'}) do
        events.on(name, function()
            if self.strip then
                window.place({anchor = 'bottom', fill = 'width'})
            end
        end, {owner = owner})
    end
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
