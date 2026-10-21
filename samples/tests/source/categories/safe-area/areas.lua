-- The safe area the tests of the category show: the simulated devices of `haylen.viewport`, a phone with a dynamic island where the screen has no notch, the insets, the bands of the screen outside the safe area and a preview of the device with its safe area.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
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

-- Draws a device of the kind held sideways around the rectangle `bounds`, with a screen of the shape of the visible screen, the bands outside the safe area of the visible screen shaded on its screen and its safe area outlined, and returns the screen and the safe area of the preview, which the tests fill.
function areas.drawPreview(bounds, kind)
    local visible = viewport.visibleRect()
    local fit = math.min(bounds.width / visible.width, bounds.height / visible.height) * 0.86
    local width, height = visible.width * fit, visible.height * fit
    local screen = {bounds.x + (bounds.width - width) / 2, bounds.y + (bounds.height - height) / 2, width, height}
    local top, right, bottom, left = areas.insets()
    local safe = {screen[1] + left * fit, screen[2] + top * fit, width - (left + right) * fit, height - (top + bottom) * fit}
    local device, radius = Device.around(kind, screen, kind ~= 'tv' and kind ~= 'tablet')
    device.screen = '#FF1E3A5F'
    Device.draw(device)
    graphics2d.pushMask(function()
        graphics2d.drawShape(screen, {radius = radius, color = '#FFFFFFFF'})
    end, {layer = 3})
    graphics2d.drawRect(screen, '#40F0473E')
    graphics2d.drawRect(safe, '#FF1E3A5F', {layer = 1})
    graphics2d.popMask()
    graphics2d.drawShape(safe, {radius = 8, color = '#00000000', borderWidth = 2, borderColor = '#CC3AA8E0', layer = 5})
    return screen, safe
end

-- Fills the four bands of the screen outside the safe area and outlines the safe area.
function areas.drawBands()
    local visible, safe = viewport.visibleRect(), viewport.safeRect()
    local top, right, bottom, left = areas.insets()
    graphics2d.beginScreen()
    graphics2d.drawRect({visible.x, visible.y, visible.width, top}, '#40FF4040')
    graphics2d.drawRect({visible.x, safe:bottom(), visible.width, bottom}, '#40FF4040')
    graphics2d.drawRect({visible.x, safe.y, left, safe.height}, '#40FF4040')
    graphics2d.drawRect({safe:right(), safe.y, right, safe.height}, '#40FF4040')
    graphics2d.drawRectOutline(safe, 3, '#FF3AA8E0')
end

return areas
