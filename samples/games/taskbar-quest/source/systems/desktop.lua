-- The two ways the quest sits on the desktop: a frameless transparent strip above the taskbar that lets clicks through its empty parts, or a normal opaque window.
local events = require('haylen.events')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local config = require('config')

local desktop = {}
desktop.__index = desktop

-- Only the desktops of Windows, macOS and Linux float a frameless window above the others, as the window reference says. Everywhere else, such as on a web page, the window fills the page and the quest shows in the window mode alone.
desktop.stripPlatforms = {windows = true, macos = true, linux = true}

-- The strip mode comes from `haylen.json`, so on a desktop the app starts in whatever mode the window has. The strip follows the taskbar and the displays while the owner lives.
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

-- Returns the area the quest plays in: the visible area of a desktop window in both modes, or, on a page, a window of the shape of the window mode in the middle of the page, inside its margin.
function desktop:area()
    local visible = viewport.visibleRect()
    if self.canStrip then
        return visible
    end

    local margin = config.page.margin
    local aspect = config.window.width / config.window.height
    local width = math.min(visible.width - 2 * margin, (visible.height - 2 * margin) * aspect)
    local height = width / aspect
    return m.rect(visible.x + (visible.width - width) / 2, visible.y + (visible.height - height) / 2, width, height)
end

-- Draws the page around the framed window: a dark backdrop over the whole page and a soft shadow under the window.
function desktop:drawPage(area)
    local page = config.page
    local visible = viewport.visibleRect()
    graphics2d.drawMesh(nil, {
        {x = visible.x, y = visible.y, color = page.backdropTop},
        {x = visible:right(), y = visible.y, color = page.backdropTop},
        {x = visible:right(), y = visible:bottom(), color = page.backdropBottom},
        {x = visible.x, y = visible:bottom(), color = page.backdropBottom},
    }, {1, 2, 3, 1, 3, 4}, {layer = config.layer.backdrop})
    graphics2d.drawShape({area.x, area.y + 12, area.width, area.height}, {radius = page.radius, color = page.shadow, softness = 28, layer = config.layer.backdrop})
end

-- Draws the shape of the framed window, which masks the quest so nothing of it reaches the page.
function desktop:drawWindowShape(area)
    graphics2d.drawShape(area, {radius = config.page.radius, color = '#FFFFFFFF'})
end

-- Draws the border of the framed window over the edge of the quest.
function desktop:drawFrame(area)
    local page = config.page
    graphics2d.drawShape(area, {radius = page.radius, color = '#00000000', borderWidth = page.border, borderColor = page.frame, layer = config.layer.text + 1})
end

-- Gives the strip the regions that keep the mouse this frame, so a click anywhere else reaches the desktop behind it.
function desktop:keepMouse(regions)
    if self.strip then
        window.setMousePassthrough(regions)
    end
end

return desktop
