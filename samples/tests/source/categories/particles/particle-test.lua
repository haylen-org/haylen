-- The base of the particle tests: a canvas over the play area that fits a world of 1920 by 1080 around the origin with a night backdrop, and for the tests that ask for it a cursor that starts in the middle and follows the mouse, a finger, the arrows, WASD, the directional pad and the sticks inside the play area. A click, a tap, E, Enter, Space, the south or west button or the right trigger fire what a test fires.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')

local Pointer = require('harness.pointer')
local Test = require('harness.test')

local ParticleTest = haylen.class('ParticleTest', Test)

ParticleTest.view = {1920, 1080}
ParticleTest.atlases = {effects = 'particles/library/textures/effects.json', pixel = 'particles/library/textures/pixel.json'}
ParticleTest.actions = {actions = {
    {name = 'cursor', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'stick:right'}},
    {name = 'press', type = 'button', bindings = {'axis:rightTrigger+'}},
    {name = 'fire', type = 'button', bindings = {'key:e', 'key:enter', 'key:space', 'button:south', 'button:west'}},
}}

-- Fills the emitter options `options` with the texture and the frames of a piece of the particle atlases of the library in `content/particles/library/textures` and returns them: the frame `name`, or the frames `name-1`, `name-2` and on of a sheet. The pixel art pieces, whose names start with `pixel-`, come from the atlas with the nearest filter, and the others from the atlas with the linear filter.
function ParticleTest.look(name, options)
    local pixel = name:find('^pixel%-') ~= nil
    local atlas = assets.load(pixel and ParticleTest.atlases.pixel or ParticleTest.atlases.effects, 'atlas', {filter = pixel and 'nearest' or 'linear'})
    local frames = {}
    if atlas:hasFrame(name) then
        frames[1] = atlas:source(name)
    end
    while atlas:hasFrame(name .. '-' .. #frames + 1) do
        frames[#frames + 1] = atlas:source(name .. '-' .. #frames + 1)
    end
    if #frames == 0 then
        error(('The particle atlases have no piece "%s".'):format(name), 2)
    end
    options.texture, options.frames = atlas.texture, frames
    return options
end

-- Returns a texture of the library that tests draw whole, such as the glow line of a ribbon and the sheet of runes that "frameGrid" cuts.
function ParticleTest.library(name)
    return assets.texture('particles/library/textures/' .. name .. '.png', {filter = 'linear'})
end

-- Updates every emitter, system or trail of a list and returns how many live particles or points they hold.
function ParticleTest.updateAll(list, dt)
    local count = 0
    for index = 1, #list do
        local emitter = list[index]
        emitter:update(dt)
        count = count + emitter.count
    end
    return count
end

function ParticleTest.drawAll(list)
    for index = 1, #list do
        list[index]:draw()
    end
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

-- Shows a line of live values made with `string.format`, which only runs when the status line refreshes.
function ParticleTest:report(format, ...)
    if self.statusTime >= Test.statusInterval then
        self:status(string.format(format, ...))
    end
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
