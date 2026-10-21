-- The base of the shader tests: the painted art and the materials of the category, a layout of a fixed size fitted into the play area with 0, 0 at its top-left corner, the grid of cards the tests show their materials in, each a little scene with a sky, the ground and a title, and a press that a click or a tap on the play area, E, Enter, Space or the south or west button makes.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')

local Pointer = require('harness.pointer')
local Test = require('harness.test')

local ShaderTest = haylen.class('ShaderTest', Test)

ShaderTest.layout = {1340, 760}
ShaderTest.actions = {actions = {
    Pointer.actions[1],
    Pointer.actions[2],
    {name = 'flash', type = 'button', bindings = {'key:e', 'key:enter', 'key:space', 'button:south', 'button:west'}},
}}
ShaderTest.linear = {filter = 'linear'}
-- The pictures the materials shade: a path, the region of the picture in it, and where the subject stands in that region, on its feet or on its middle.
ShaderTest.pictures = {
    knight = {path = 'sprites/images/hero.png', source = {0, 0, 192, 192}, feet = 0.97},
    walker = {path = 'sprites/sheets/walker.png', source = {0, 0, 256, 256}, feet = 248 / 256},
    dragon = {path = 'events/dragon.png', source = {0, 0, 256, 256}, feet = 248 / 256},
    bat = {path = 'events/bat.png', source = {0, 0, 256, 256}, feet = 0.5},
    crate = {path = 'physics/crate.png', source = {0, 0, 160, 151}, feet = 0.97},
    flag = {path = 'physics/flag.png', source = {0, 0, 136, 192}, feet = 0.98},
    planet = {path = 'shaders/images/planet.png', source = {0, 0, 512, 512}, feet = 0.5},
}
ShaderTest.skies = {day = {'#FF5FB4EC', '#FFAEDDF6', '#FFE4F5FB'}, night = {'#FF0E1426', '#FF1E2A50', '#FF3A4A80'}}
ShaderTest.ink = '#FF15172A'
-- The subjects of a card stand on this share of its height, where the grass of the ground strip is, this share of the strip below its top.
ShaderTest.feet = 0.8
ShaderTest.footing = 70 / 256

-- Returns the texture of a picture, loaded once with the linear filter.
function ShaderTest.texture(name)
    return assets.texture(ShaderTest.pictures[name].path, ShaderTest.linear)
end

function ShaderTest.planet()
    return ShaderTest.texture('planet')
end

function ShaderTest.noise()
    return assets.texture('shaders/images/noise.png', {filter = 'linear', wrap = 'repeat'})
end

function ShaderTest.pattern()
    return assets.texture('shaders/images/pattern.png', {filter = 'linear', wrap = 'repeat'})
end

function ShaderTest.ramp(name)
    return assets.texture('shaders/images/ramp-' .. name .. '.png', {filter = 'linear'})
end

function ShaderTest.material(name, uniforms)
    return graphics2d.newMaterial(assets.shader('shaders/' .. name .. '.shader'), uniforms)
end

-- The texel of a picture in its texture, which the shaders that read the pixels around them need.
function ShaderTest.texel(name)
    local texture = ShaderTest.texture(name)
    return {1 / texture.width, 1 / texture.height}
end

-- Returns the sky of the cards, a vertical gradient by day or by night, made once.
function ShaderTest.sky(night)
    local key = night and 'night' or 'day'
    ShaderTest.skyTextures = ShaderTest.skyTextures or {}
    if not ShaderTest.skyTextures[key] then
        local colors = ShaderTest.skies[key]
        local image = graphics.newImage(4, 128)
        image:fillGradient({angle = math.pi / 2, stops = {{0, colors[1]}, {0.7, colors[2]}, {1, colors[3]}}})
        ShaderTest.skyTextures[key] = graphics.newTexture(image, ShaderTest.linear)
    end
    return ShaderTest.skyTextures[key]
end

-- Returns the centers and sizes of `count` cells in rows of `columns` over the layout, or over its top `room` units, each with the rectangle of its card.
function ShaderTest.cells(count, columns, room)
    local rows = math.ceil(count / columns)
    local gap, margin = 24, 24
    local width = (ShaderTest.layout[1] - margin * 2 - gap * (columns - 1)) / columns
    local height = ((room or ShaderTest.layout[2]) - margin * 2 - gap * (rows - 1)) / rows
    local cells = {}
    for index = 1, count do
        local column, row = (index - 1) % columns, (index - 1) // columns
        -- A last row with fewer cells sits in the middle.
        local inRow = math.min(columns, count - row * columns)
        local left = margin + (ShaderTest.layout[1] - margin * 2 - inRow * width - (inRow - 1) * gap) / 2 + column * (width + gap)
        local top = margin + row * (height + gap)
        cells[index] = {x = left + width / 2, y = top + height * 0.48, size = math.min(width, height) * 0.62, rect = {left, top, width, height}}
    end
    return cells
end

-- Draws the card of a cell: a sky by day or by night, a strip of ground and the title on a plaque at the bottom. The subject draws between the sky and the plaque.
function ShaderTest:drawCard(cell, title, night)
    local rect = cell.rect
    self.cardShape = self.cardShape or {radius = 20}
    self.cardRect = rect
    self.maskCard = self.maskCard or function() graphics2d.drawShape(self.cardRect, self.cardShape) end
    graphics2d.pushMask(self.maskCard, {layer = 0})
    graphics2d.drawShape(rect, {texture = ShaderTest.sky(night)})
    self.ground = self.ground or assets.texture('camera/ground.png', ShaderTest.linear)
    local groundHeight = rect[4] * 0.26
    local groundWidth = groundHeight * self.ground.width / self.ground.height
    local top = rect[2] + rect[4] * ShaderTest.feet - groundHeight * ShaderTest.footing
    for x = rect[1], rect[1] + rect[3], groundWidth do
        graphics2d.draw(self.ground, x, top, {width = groundWidth + 1, height = groundHeight, pivotX = 0, pivotY = 0, color = night and '#FF6C76A8' or nil, layer = 1})
    end
    graphics2d.popMask()
    graphics2d.drawShape(rect, {radius = 20, color = '#00000000', borderWidth = 3, borderColor = ShaderTest.ink, layer = 8})
    local width = graphics2d.measureText(self:font(), title, {size = 26}) + 36
    local plate = {rect[1] + rect[3] / 2 - width / 2, rect[2] + rect[4] - 52, width, 40}
    graphics2d.drawShape(plate, {radius = 20, color = '#E62F3352', borderWidth = 2, borderColor = ShaderTest.ink, layer = 9})
    graphics2d.drawText(self:font(), title, rect[1] + rect[3] / 2, plate[2] + 20, {size = 26, anchor = {0.5, 0.5}, outlineWidth = 3, outlineColor = ShaderTest.ink, layer = 10})
end

-- Draws a picture standing on the ground of a cell, or floating in its middle, at `size`, with the options of the draw.
function ShaderTest:drawPicture(cell, name, size, options)
    local picture = ShaderTest.pictures[name]
    local source = picture.source
    local scale = size / math.max(source[3], source[4])
    options.source = options.source or source
    options.width, options.height = source[3] * scale, source[4] * scale
    options.pivotY = picture.feet
    options.layer = options.layer or 4
    local y = picture.feet > 0.6 and cell.rect[2] + cell.rect[4] * ShaderTest.feet or cell.y
    if picture.feet > 0.6 then
        graphics2d.drawShape({cell.x - size * 0.26, y - size * 0.05, size * 0.52, size * 0.1}, {kind = 'ellipse', color = '#55203018', layer = 3})
    end
    graphics2d.draw(ShaderTest.texture(name), cell.x, y, options)
end

function ShaderTest:font()
    self.labelFont = self.labelFont or assets.font('fonts/lilita-one-regular.ttf')
    return self.labelFont
end

function ShaderTest:init(entry)
    ShaderTest.super.init(self, entry)
    self.pointer = Pointer()
end

-- Mounts the frame of `Test:frame` with the layout of the category fitted into the play area. With `presses = true` the test reads presses, which take the action map and the first focus.
function ShaderTest:frame(options)
    self.presses = options.presses
    if options.presses then
        self:loadActions(ShaderTest.actions)
        options.play = true
    end
    options.presses = nil
    options.view = options.view or ShaderTest.layout
    ShaderTest.super.frame(self, options)
    if options.view == ShaderTest.layout then
        self.camera.position = {ShaderTest.layout[1] / 2, ShaderTest.layout[2] / 2}
    end
end

function ShaderTest:update(dt)
    ShaderTest.super.update(self, dt)
    if self.presses then
        self.pointer:update(dt, self)
    end
end

-- Tells whether the player pressed this frame, on the play area or with a button.
function ShaderTest:pressed()
    return self.pointer.pressed or input.pressed('flash')
end

function ShaderTest:renderUi()
    if self.presses then
        self.pointer:draw()
    end
end

return ShaderTest
