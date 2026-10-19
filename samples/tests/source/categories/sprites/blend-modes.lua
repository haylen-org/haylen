-- Blend modes: the blend modes of a group over the same picture, one cell each, leaving out the premultiplied mode, which a group draws like alpha, and the opaque one, which replaces everything below the group with its image. The modes up to screen use the blending of the GPU, and the modes from overlay on mix every color with the color below it, which the group reads from a copy of what its canvas drew before it.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local SpriteTest = require('categories.sprites.sprite-test')
local Test = require('harness.test')

local BlendModes = haylen.class('BlendModes', SpriteTest)

BlendModes.code = [[
graphics2d.draw(picture, x, y)
graphics2d.beginGroup({blend = 'softLight', opacity = 0.9})  -- Mixes each color with the picture below.
graphics2d.drawCircle(x, y, 40, '#FFE5534B')
graphics2d.endGroup()]]

BlendModes.modes = {'alpha', 'additive', 'multiply', 'screen', 'overlay', 'darken', 'lighten', 'colorDodge', 'colorBurn', 'hardLight', 'softLight', 'difference', 'exclusion'}
BlendModes.titles = {alpha = 'Alpha', additive = 'Additive', multiply = 'Multiply', screen = 'Screen', overlay = 'Overlay', darken = 'Darken', lighten = 'Lighten', colorDodge = 'Color dodge', colorBurn = 'Color burn', hardLight = 'Hard light', softLight = 'Soft light', difference = 'Difference', exclusion = 'Exclusion'}
BlendModes.colors = {'#FFE5534B', '#FF35B37E', '#FF3C9BE0'}

function BlendModes:enter()
    self.tiles = SpriteTest.texture('images/tiles.png')
    self.tree = SpriteTest.texture('images/tree.png')
    self.hero = SpriteTest.texture('images/hero.png')
    self.opacity = 1
    self.time = 0
    self:frame{
        code = BlendModes.code,
        hint = 'Each cell draws the same picture and then a group of colored circles with its blend mode. Change the opacity of the groups to weaken every mode.',
        controls = {
            ui.formField{label = 'Opacity of the groups', ui.slider{id = 'opacity', min = 0, max = 1, value = self.opacity, step = 0.05, showValue = true, onChange = function(event) self.opacity = event.value end}},
        },
        focus = 'opacity',
    }
end

function BlendModes:update(dt)
    BlendModes.super.update(self, dt)
    self.time = self.time + dt
    local stats = graphics2d.stats()
    self:status(string.format('Modes %d, canvases %d, passes %d, draw calls %d', #BlendModes.modes, stats.canvases, stats.passes, stats.drawCalls))
end

-- Draws the picture of a cell: a strip of ground, a tree and the hero, the same in every cell.
function BlendModes:picture(x, y, width, height)
    graphics2d.drawShape({x, y, width, height}, {radius = 10, color = '#FF6FB7D8'})
    graphics2d.draw(self.tiles, x, y + height * 0.55, {width = width, height = height * 0.45, pivotX = 0, pivotY = 0})
    graphics2d.draw(self.tree, x + width * 0.28, y + height * 0.62, {width = height * 0.55, height = height * 0.55, pivotY = 1})
    graphics2d.draw(self.hero, x + width * 0.7, y + height * 0.66, {width = height * 0.4, height = height * 0.4, pivotY = 1})
end

function BlendModes:draw(area)
    local columns, gap = 5, 12
    local rows = math.ceil(#BlendModes.modes / columns)
    local width = (area.width - gap * (columns + 1)) / columns
    local height = (area.height - gap * (rows + 1)) / rows
    for index, mode in ipairs(BlendModes.modes) do
        local column, row = (index - 1) % columns, (index - 1) // columns
        local x, y = gap + column * (width + gap), gap + row * (height + gap)
        self:picture(x, y, width, height)
        local radius = math.min(width, height) * 0.24
        graphics2d.beginGroup({blend = mode, opacity = self.opacity})
        for circle, color in ipairs(BlendModes.colors) do
            local angle = self.time * 0.6 + circle * math.pi * 2 / 3
            graphics2d.drawCircle(x + width / 2 + math.cos(angle) * radius * 0.55, y + height * 0.45 + math.sin(angle) * radius * 0.55, radius, color)
        end
        graphics2d.endGroup()
        Test.caption(BlendModes.titles[mode], x + 10, y + 8, {color = Test.ink})
    end
end

return BlendModes
