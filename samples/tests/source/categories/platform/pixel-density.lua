-- Pixel density: the app caps the framebuffer pixels in each point of the window or takes every pixel of the display, and the test shows the size of the framebuffer, its pixels in each point and the points of the window, which stay the same, over circles, thin lines and text that turn softer as the cap lowers, and gives back the cap of the project when it ends.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')
local window = require('haylen.window')

local Test = require('harness.test')

local PixelDensity = haylen.class('PixelDensity', Test)

PixelDensity.caps = {{id = '0', text = 'Every pixel'}, {id = '2', text = '2'}, {id = '1.5', text = '1.5'}, {id = '1', text = '1'}, {id = '0.5', text = '0.5'}}
PixelDensity.margin = 48

PixelDensity.code = [[
window.setMaxPixelDensity(1.5)  -- Fewer pixels for a busy scene.
window.setMaxPixelDensity(0)    -- Every pixel of the display.
local width, height = window.framebufferSize()
print(width, height, window.dpiScale())]]

function PixelDensity:enter()
    self.previous = window.maxPixelDensity()
    self:frame{
        hint = 'Pick a cap. The framebuffer shrinks on a display denser than the cap and stretches over the window, so the shapes keep their place and size while their edges soften.',
        focus = 'cap',
        controls = {
            ui.formField{label = 'Pixels in each point', ui.segmentedControl{id = 'cap', items = PixelDensity.caps, selected = string.format('%g', self.previous), onChange = function(event)
                window.setMaxPixelDensity(tonumber(event.value))
            end}},
        },
    }
end

function PixelDensity:exit()
    window.setMaxPixelDensity(self.previous)
end

function PixelDensity:update(dt)
    PixelDensity.super.update(self, dt)
    local width, height = window.framebufferSize()
    local scale = window.dpiScale()
    local cap = window.maxPixelDensity()
    local limit = cap == 0 and 'every pixel of the display' or string.format('at most %g pixels in each point', cap)
    self:status(string.format('The app draws %s: a framebuffer of %d by %d pixels at %g pixels in each point, for a window of %d by %d points.', limit, width, height, scale, math.floor(width / scale + 0.5), math.floor(height / scale + 0.5)))
end

-- Rings, hairlines and text of several sizes show how sharp the framebuffer is, and their place on the screen never moves.
function PixelDensity:draw(area)
    local margin = PixelDensity.margin
    local middle = area.height * 0.42
    for index = 1, 4 do
        graphics2d.drawRing(margin + 90, middle, 18 * index, 2, Test.accent)
    end
    for index = 0, 8 do
        local x = margin + 220 + index * 14
        graphics2d.drawLine(x, middle - 70, x + 40, middle + 70, 1, Test.ink)
    end
    local left = margin + 400
    Test.caption('Sharp edges at full density', left, middle - 40, {size = 36, color = Test.ink})
    Test.caption('Small text shows the softening first.', left, middle + 10, {size = 20})
    graphics2d.drawRect({margin, area.height * 0.8, area.width - margin * 2, 2}, Test.warm)
end

return PixelDensity
