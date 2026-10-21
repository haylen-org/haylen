-- The base of the particle tests: a canvas over the play area that fits a world of 1920 by 1080 around the origin with a backdrop, the ground and the props of the scenes, and for the tests that ask for it a cursor that starts in the middle and follows the mouse, a finger, the arrows, WASD, the directional pad and the sticks inside the play area. A click, a tap, E, Enter, Space, the south or west button or the right trigger fire what a test fires.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')

local Pointer = require('harness.pointer')
local Test = require('harness.test')

local ParticleTest = haylen.class('ParticleTest', Test)

ParticleTest.view = {1920, 1080}
ParticleTest.atlases = {effects = 'particles/library/textures/effects.json', pixel = 'particles/library/textures/pixel.json', props = 'particles/props.json', scenery = 'lighting/scenery.json'}
ParticleTest.tiles = 'tiled/tiles/terrain.png'
ParticleTest.floors = {stone = {530, 2, 128, 128}, grass = {2, 2, 128, 128}, flowers = {134, 2, 128, 128}, dirt = {266, 2, 128, 128}, sand = {398, 2, 128, 128}}
ParticleTest.gradients = {}
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

-- Fills the view with a smooth gradient from the color `top` to the color `bottom`, below everything else, from a texture that dithers its steps so it shows no bands.
function ParticleTest.backdrop(top, bottom)
    local key = table.concat(top, ',') .. '|' .. table.concat(bottom, ',')
    local texture = ParticleTest.gradients[key]
    if not texture then
        local image = graphics.newImage(4, 256)
        image:fillGradient({angle = math.pi / 2, colors = {{top[1], top[2], top[3], 1}, {bottom[1], bottom[2], bottom[3], 1}}})
        texture = graphics.newTexture(image, {filter = 'linear'})
        ParticleTest.gradients[key] = texture
    end
    local area = graphics2d.canvasBounds()
    graphics2d.draw(texture, area.x, area.y, {width = area.width, height = area.height, pivotX = 0, pivotY = 0, layer = -10})
end

-- Covers the canvas below `y` with a floor of painted tiles, `stone`, `grass`, `flowers`, `dirt` or `sand`, of `size` world units, with a dark rim along its top edge.
function ParticleTest.paveFloor(kind, y, size, color)
    local tiles = assets.texture(ParticleTest.tiles, {filter = 'linear'})
    local area = graphics2d.canvasBounds()
    size = size or 160
    for row = y, area:bottom(), size do
        for column = math.floor(area.x / size) * size, area:right(), size do
            graphics2d.draw(tiles, column, row, {source = ParticleTest.floors[kind], width = size, height = size, pivotX = 0, pivotY = 0, layer = -5, color = color})
        end
    end
    graphics2d.drawRect({area.x, y - 4, area.width, 8}, '#FF1E1B33', {layer = -4})
end

-- Draws a painted prop of the particle scenes, such as the confetti cannon, the fountain, the spaceship, the grinder or the campfire, or of the lighting scenery, such as a cottage, with its pivot at `x`, `y`.
function ParticleTest.prop(name, x, y, scale, options)
    options = options or {}
    local atlas = assets.load(ParticleTest.atlases.props, 'atlas', {filter = 'linear'})
    if not atlas:hasFrame(name) then
        atlas = assets.load(ParticleTest.atlases.scenery, 'atlas', {filter = 'linear'})
    end
    local source = atlas:source(name)
    options.source = source
    options.width, options.height = source.width * scale, source.height * scale
    options.layer = options.layer or 1
    graphics2d.draw(atlas.texture, x, y, options)
end

-- Draws a legend as chips of the interface in the top-left corner of the canvas, one line each, with a dot of its color when a line gives one.
function ParticleTest.legend(lines)
    local area = graphics2d.canvasBounds()
    for index, line in ipairs(lines) do
        local x, y = area.x + 40, area.y + 40 + (index - 1) * 64
        local width = graphics2d.measureText(nil, line.text, {size = 26}) + (line.color and 72 or 48)
        graphics2d.drawShape({x, y, width, 50}, {radius = 25, color = '#D02F3352', borderWidth = 3, borderColor = '#FF15172A', layer = 30})
        if line.color then
            graphics2d.drawCircle(x + 28, y + 25, 11, line.color, {layer = 31})
        end
        graphics2d.drawText(nil, line.text, x + (line.color and 52 or 24), y + 25, {size = 26, anchor = {0, 0.5}, layer = 31})
    end
end

-- Draws a caption centered under a point of the world, in words for people with the name of the option in a smaller second line when one is given.
function ParticleTest.label(text, x, y, option)
    graphics2d.drawText(nil, text, x, y, {size = 40, anchor = {0.5, 0}, outlineWidth = 4, layer = 20})
    if option then
        graphics2d.drawText(nil, option, x, y + 50, {size = 28, anchor = {0.5, 0}, outlineWidth = 3, color = '#FFC8D0E8', layer = 20})
    end
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
