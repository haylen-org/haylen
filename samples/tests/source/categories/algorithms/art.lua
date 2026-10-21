-- The art the algorithm tests share: the painted tiles and props of the Tiled tests, the adventurer whose outfit takes the colors of a team through its part mask, flags for the ends of a path, and labels on a dark pill that stay readable over any map.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Art = haylen.class('Art')

-- The tiles of `tiled/tiles/terrain.png` by name: eight columns of 128 pixels with a margin of 2 and a spacing of 4.
Art.tileIds = {grass = 0, flowers = 1, dirt = 2, sand = 3, stone = 4, brick = 5, water = 8, lava = 12, bush = 18, rock = 19, crate = 20, chest = 21, spikes = 22, flag = 23, coin = 24, slime = 28, sign = 30, key = 31}
Art.tileSize = 128
Art.cellsPerTile = 4
Art.outline = '#FF1E1B33'
Art.pill = '#E01A1E2B'

-- The adventurer stands on the bottom of its 512 pixel image, with its feet on this share of the height.
Art.personFeet = 0.96

-- Team colors of the adventurer, for the hat, the shirt, the trousers and the boots.
Art.teams = {
    blue = {red = '#FF2F86F6', green = '#FF7FB8FF', blue = '#FF2B3F8F', yellow = '#FF5A3A22'},
    red = {red = '#FFF0473E', green = '#FFFFB0A0', blue = '#FF6E2430', yellow = '#FF4A2A1C'},
    green = {red = '#FF4CAF50', green = '#FFCDEFA8', blue = '#FF35603A', yellow = '#FF5A3A22'},
    gold = {red = '#FFFFC93C', green = '#FFFFF0C0', blue = '#FF7A5A2A', yellow = '#FF5A3A22'},
    violet = {red = '#FF9B6BE8', green = '#FFE3D4FF', blue = '#FF4B3A8A', yellow = '#FF3A2A4A'},
    teal = {red = '#FF2FB3C4', green = '#FFC2F0F4', blue = '#FF1F5F6A', yellow = '#FF5A3A22'},
}

function Art:init()
    local linear = {filter = 'linear', mipmaps = true}
    self.terrain = assets.texture('tiled/tiles/terrain.png', linear)
    self.person = assets.texture('sprites/images/outfit-base.png', linear)
    self.personMask = assets.texture('sprites/images/outfit-mask.png', linear)
    self.sources = {}
    for name, id in pairs(Art.tileIds) do
        self.sources[name] = {2 + id % 8 * 132, 2 + id // 8 * 132, Art.tileSize, Art.tileSize}
    end
    -- The options of every draw, filled in place.
    self.tileOptions = {source = nil, width = 0, height = 0, pivotX = 0, pivotY = 0, layer = 0, color = nil}
    self.propOptions = {source = nil, width = 0, height = 0, pivotX = 0.5, pivotY = 1, layer = 0, flipHorizontal = false, color = nil}
    self.personOptions = {width = 0, height = 0, pivotX = 0.5, pivotY = Art.personFeet, layer = 0, flipHorizontal = false, partMask = self.personMask, partColors = nil}
    self.textStyle = {size = 22, color = '#FFFFFFFF', anchor = {0.5, 0.5}, layer = 0}
    self.pillShape = {radius = 0, color = Art.pill, layer = 0}
end

-- Fills the rectangle at `x`, `y` of `width` by `height` with a terrain tile, tinted by `color` when given.
function Art:tile(name, x, y, width, height, layer, color)
    local options = self.tileOptions
    options.source, options.width, options.height, options.layer, options.color = self.sources[name], width, height or width, layer or 0, color
    graphics2d.draw(self.terrain, x, y, options)
end

-- Adds to a sprite batch of the terrain texture the cell `column`, `row` of a grid of `size` units whose top-left corner is `left`, `top`, painted with the part of a terrain tile that continues its texture from the cells around it, since a tile spans `Art.cellsPerTile` cells on each side.
function Art:addCell(batch, name, left, top, size, column, row, color)
    local source = self.sources[name]
    local part = Art.tileSize / Art.cellsPerTile
    batch:add({x = left + column * size, y = top + row * size, width = size + 0.5, height = size + 0.5, pivotX = 0, pivotY = 0, color = color, source = {source[1] + column % Art.cellsPerTile * part, source[2] + row % Art.cellsPerTile * part, part, part}})
end

-- Draws a prop of the terrain atlas, such as a bush, a rock, a chest, a coin or a flag, standing on `x`, `y` at `size` units across.
function Art:prop(name, x, y, size, layer, flip, color)
    local options = self.propOptions
    options.source, options.width, options.height, options.layer, options.flipHorizontal, options.color = self.sources[name], size, size, layer or 0, flip or false, color
    graphics2d.draw(self.terrain, x, y, options)
end

-- Draws the adventurer standing on `x`, `y`, `height` units tall, in the colors of a team of `Art.teams` or a table of part colors.
function Art:character(x, y, height, team, layer, flip)
    local options = self.personOptions
    options.width, options.height, options.layer, options.flipHorizontal = height, height, layer or 0, flip or false
    options.partColors = type(team) == 'string' and Art.teams[team] or team
    graphics2d.draw(self.person, x, y, options)
end

-- Draws a shadow under something that stands on `x`, `y`.
function Art.shadow(x, y, width, layer)
    graphics2d.drawEllipse(x, y, width / 2, width / 7, '#50000000', {layer = layer or 0})
end

-- Draws a short text centered on `x`, `y` on a dark pill, so it reads over lines and colors.
function Art:label(text, x, y, size, color, layer)
    local style = self.textStyle
    style.size, style.color, style.layer = size or 22, color or '#FFFFFFFF', (layer or 10) + 1
    local width, height = graphics2d.measureText(nil, text, style)
    local padX, padY = style.size * 0.45, style.size * 0.18
    local shape = self.pillShape
    shape.radius, shape.layer = height / 2 + padY, layer or 10
    graphics2d.drawShape({x - width / 2 - padX, y - height / 2 - padY, width + padX * 2, height + padY * 2}, shape)
    graphics2d.drawText(nil, text, x, y, style)
end

-- Marks the start of a path with a green pin and its goal with a red flag, both standing on `x`, `y`.
function Art:start(x, y, size, layer)
    layer = layer or 20
    Art.shadow(x, y, size * 0.5, layer)
    graphics2d.drawShape({x - size * 0.06, y - size * 0.62, size * 0.12, size * 0.62}, {radius = size * 0.04, color = '#FF6E4024', borderWidth = size * 0.025, borderColor = Art.outline, layer = layer})
    graphics2d.drawShape({x - size * 0.22, y - size * 0.98, size * 0.44, size * 0.44}, {kind = 'ellipse', color = '#FF5BD16A', borderWidth = size * 0.05, borderColor = Art.outline, layer = layer + 1})
    graphics2d.drawShape({x - size * 0.11, y - size * 0.9, size * 0.12, size * 0.12}, {kind = 'ellipse', color = '#B0FFFFFF', layer = layer + 1})
end

function Art:goal(x, y, size, layer)
    Art.shadow(x, y, size * 0.5, layer or 20)
    self:prop('flag', x + size * 0.12, y + size * 0.1, size * 1.2, (layer or 20) + 1)
end

return Art
