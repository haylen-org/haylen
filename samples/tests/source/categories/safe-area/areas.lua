-- The safe area the tests of the category show: the simulated devices of `haylen.viewport`, a phone with a dynamic island where the screen has no notch, the insets, the bands of the screen outside the safe area and a preview of the device with its safe area.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local Device = require('categories.safe-area.device')

local areas = {}

areas.devices = {
    {id = 'device', text = 'This device'},
    {id = 'iphoneNotch', text = 'An iPhone with a notch'},
    {id = 'iphoneDynamicIsland', text = 'An iPhone with a dynamic island'},
    {id = 'ipad', text = 'An iPad'},
    {id = 'androidGestureBar', text = 'An Android phone with a gesture bar'},
    {id = 'television', text = 'A TV'},
}

-- Desktop windows, browsers and the headless host have no notch.
areas.flat = {macos = true, windows = true, linux = true, web = true, headless = true}

-- Simulates a phone with a dynamic island where the screen has no notch, while phones, tablets and TVs keep their own safe area, and returns the simulation to restore when the test exits.
function areas.simulate()
    local previous = viewport.safeAreaSimulation()
    if areas.flat[haylen.platform] then
        viewport.setSafeAreaSimulation('iphoneDynamicIsland')
    end
    return previous
end

-- Returns how far the safe area stays from each edge of the visible screen, in design units: top, right, bottom and left.
function areas.insets()
    local visible, safe = viewport.visibleRect(), viewport.safeRect()
    return safe.y - visible.y, visible:right() - safe:right(), visible:bottom() - safe:bottom(), safe.x - visible.x
end

-- The kind of device of `device.lua` that draws each simulated device.
areas.kinds = {device = 'island', iphoneNotch = 'notch', iphoneDynamicIsland = 'island', ipad = 'tablet', androidGestureBar = 'android', television = 'tv', custom = 'island'}

-- Formats a whole number of units, never as "-0".
function areas.number(value)
    local rounded = math.floor(value + 0.5)
    return tostring(rounded == 0 and 0 or rounded)
end

-- The shade of the bands of a screen outside its safe area.
areas.band = '#66F0473E'

-- Loads the picture that the screens of the devices show, a painted island seen from its shore.
function areas.wallpaper()
    return assets.texture('interface/images/landscape.png', {filter = 'linear'})
end

-- Draws a device of the kind held sideways around the rectangle `bounds`, with a screen of the shape of the visible screen that shows the picture `wallpaper`, the bands outside the safe area of the visible screen shaded and its safe area outlined, and returns the screen and the safe area of the preview.
function areas.drawPreview(bounds, kind, wallpaper)
    local visible = viewport.visibleRect()
    local fit = math.min(bounds.width / visible.width, bounds.height / visible.height) * 0.86
    local width, height = visible.width * fit, visible.height * fit
    local screen = {bounds.x + (bounds.width - width) / 2, bounds.y + (bounds.height - height) / 2, width, height}
    local top, right, bottom, left = areas.insets()
    local safe = {screen[1] + left * fit, screen[2] + top * fit, width - (left + right) * fit, height - (top + bottom) * fit}
    areas.drawDevice(kind, screen, safe, wallpaper)
    return screen, safe
end

-- Draws a device of the kind around the rectangle `screen`, held sideways unless it is a TV or a tablet, with the picture `wallpaper` on its screen or the plain color of the screen without one, the bands outside the rectangle `safe` shaded and the safe area outlined.
function areas.drawDevice(kind, screen, safe, wallpaper)
    local device, radius = Device.around(kind, screen, kind ~= 'tv' and kind ~= 'tablet')
    device.wallpaper = wallpaper
    Device.draw(device)
    graphics2d.pushMask(function()
        graphics2d.drawShape(screen, {radius = radius, color = '#FFFFFFFF'})
    end, {layer = 4})
    local x, y, width, height = screen[1], screen[2], screen[3], screen[4]
    local safeRight, safeBottom = safe[1] + safe[3], safe[2] + safe[4]
    graphics2d.drawRect({x, y, width, safe[2] - y}, areas.band)
    graphics2d.drawRect({x, safeBottom, width, y + height - safeBottom}, areas.band)
    graphics2d.drawRect({x, safe[2], safe[1] - x, safe[4]}, areas.band)
    graphics2d.drawRect({safeRight, safe[2], x + width - safeRight, safe[4]}, areas.band)
    graphics2d.popMask()
    graphics2d.drawShape(safe, {radius = 8, color = '#00000000', borderWidth = 3, borderColor = '#E63AA8E0', layer = 5})
end

-- A table of what each platform does, in the look of the tables of the UI, whose descriptions wrap: muted captions over rows split by dividers. Each row is a platform and its description.
function areas.platformTable(heading, rows)
    local children = {ui.row{gap = 24, ui.label{text = 'Platform', font = 'caption', color = 'textMuted', width = 280}, ui.label{text = heading, font = 'caption', color = 'textMuted', grow = 1}}}
    for _, row in ipairs(rows) do
        children[#children + 1] = ui.divider{align = 'stretch'}
        children[#children + 1] = ui.row{gap = 24, alignItems = 'start', ui.label{text = row[1], width = 280}, ui.label{text = row[2], grow = 1}}
    end
    return ui.column{gap = 12, align = 'stretch', children = children}
end

return areas
