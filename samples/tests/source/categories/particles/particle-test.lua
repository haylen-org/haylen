-- The base of the particle tests: a canvas over the play area that fits a world of 1920 by 1080 around the origin with a night backdrop, and for the tests that ask for it a cursor that starts in the middle and follows the mouse, a finger, the arrows, WASD, the directional pad and the sticks inside the play area. A click, a tap, E, Enter, Space, the south or west button or the right trigger fire what a test fires.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')

local Pointer = require('harness.pointer')
local Test = require('harness.test')

local ParticleTest = haylen.class('ParticleTest', Test)

ParticleTest.view = {1920, 1080}
ParticleTest.actions = {actions = {
    {name = 'cursor', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'stick:right'}},
    {name = 'press', type = 'button', bindings = {'axis:rightTrigger+'}},
    {name = 'fire', type = 'button', bindings = {'key:e', 'key:enter', 'key:space', 'button:south', 'button:west'}},
}}

function ParticleTest.texture(name)
    return assets.texture('particles/images/' .. name .. '.png', {filter = 'linear'})
end

-- Returns the source rectangles of an image made of square frames side by side.
function ParticleTest.frames(texture)
    local frames = {}
    for index = 0, texture.width // texture.height - 1 do
        frames[#frames + 1] = {index * texture.height, 0, texture.height, texture.height}
    end
    return frames
end

-- Fills the view with a gradient from the color `top` to the color `bottom`, below everything else.
function ParticleTest.backdrop(top, bottom)
    local area = graphics2d.canvasBounds()
    local bands = 32
    for index = 0, bands - 1 do
        local t = index / (bands - 1)
        local color = {top[1] + (bottom[1] - top[1]) * t, top[2] + (bottom[2] - top[2]) * t, top[3] + (bottom[3] - top[3]) * t, 1}
        graphics2d.drawRect({area.x, area.y + area.height * index / bands, area.width, area.height / bands + 1}, color, {layer = -10})
    end
end

-- Draws a caption centered under a point of the world.
function ParticleTest.label(text, x, y)
    graphics2d.drawText(nil, text, x, y, {size = 30, anchor = {0.5, 0}, outlineWidth = 3, layer = 20})
end

function ParticleTest:init(entry)
    ParticleTest.super.init(self, entry)
    self.pointer = Pointer({confined = true, shown = true})
    self.cursorX, self.cursorY = 0, 0
end

-- Mounts the frame of `Test:frame` with the world of the category. With `cursor = true` the test reads the cursor, which takes the action map and the first focus.
function ParticleTest:frame(options)
    self.cursor = options.cursor
    options.cursor = nil
    if self.cursor then
        self:loadActions(ParticleTest.actions)
        options.play = true
    end
    options.view = options.view or ParticleTest.view
    ParticleTest.super.frame(self, options)
end

-- Follows the play area, then moves the cursor, which stays inside the play area, in screen and world coordinates.
function ParticleTest:update(dt)
    ParticleTest.super.update(self, dt)
    if not self.stage or not self.cursor then
        return
    end
    self.pointer:update(dt, self)
    self.cursorX, self.cursorY = self.pointer.worldX, self.pointer.worldY
end

-- Tells whether the cursor pressed this frame, to fire something.
function ParticleTest:pressed()
    return self.pointer.pressed or input.pressed('fire')
end

-- Tells whether the cursor is held, to keep something going.
function ParticleTest:held()
    return self.pointer.down or input.down('fire')
end

function ParticleTest:renderUi()
    if self.stage and self.cursor then
        self.pointer:draw()
    end
end

return ParticleTest
