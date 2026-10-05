-- The base of the shader tests: the images and materials of the category, a layout of a fixed size fitted into the play area with 0, 0 at its top-left corner, the grid of captioned cells the tests show their materials in, and a press that a click or a tap on the play area, E, Enter, Space or the south or west button makes.
local assets = require('haylen.assets')
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

-- The pixel-art hero, 32 by 32 with a transparent border, drawn with nearest filtering.
function ShaderTest.hero()
    return assets.texture('shaders/images/hero.png', {filter = 'nearest'})
end

function ShaderTest.planet()
    return assets.texture('shaders/images/planet.png', {filter = 'linear'})
end

function ShaderTest.noise()
    return assets.texture('shaders/images/noise.png', {filter = 'linear', wrap = 'repeat'})
end

function ShaderTest.pattern()
    return assets.texture('shaders/images/pattern.png', {filter = 'linear', wrap = 'repeat'})
end

function ShaderTest.ramp(name)
    return assets.texture('shaders/images/ramp_' .. name .. '.png', {filter = 'linear'})
end

function ShaderTest.material(name, uniforms)
    return graphics2d.newMaterial(assets.shader('shaders/' .. name .. '.shader'), uniforms)
end

-- Returns the centers and sizes of `count` cells in rows of `columns` over the layout.
function ShaderTest.cells(count, columns)
    local rows = math.ceil(count / columns)
    local width, height = ShaderTest.layout[1] / columns, ShaderTest.layout[2] / rows
    local cells = {}
    for index = 1, count do
        local column, row = (index - 1) % columns, (index - 1) // columns
        cells[index] = {x = width * (column + 0.5), y = height * (row + 0.5) - 20, size = math.min(width, height) * 0.7}
    end
    return cells
end

function ShaderTest.caption(text, x, y)
    graphics2d.drawText(nil, text, x, y, {size = 26, anchor = {0.5, 0}, outlineWidth = 3, layer = 10})
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
