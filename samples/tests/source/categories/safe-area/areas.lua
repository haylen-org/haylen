-- The safe area the tests of the category show: the simulated devices of `haylen.viewport`, a phone with a dynamic island where the screen has no notch, the insets and the bands of the screen outside the safe area.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local viewport = require('haylen.viewport')

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
