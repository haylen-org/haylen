-- Recoloring by parts: one adventurer whose hat, shirt, trousers and boots a mask marks in red, green, blue and yellow, drawn in curated outfits from the same two images. The colors travel with each sprite and the GPU recolors every pixel, so a whole crowd of a sprite batch walking a meadow draws in one call whatever its colors, and the shading of the art stays under every color.
local assets = require('haylen.assets')
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local Panels = require('categories.sprites.panels')
local Terrain = require('categories.sprites.terrain')
local Test = require('harness.test')

local Recolor = haylen.class('Recolor', Test)

Recolor.crowdSize = 120
Recolor.textureOptions = {filter = 'linear', mipmaps = true}
Recolor.outfits = {
    {name = 'Ranger', colors = {red = '#FF3F7D3A', green = '#FFE9DDB8', blue = '#FF6B4A2E', yellow = '#FF4A3426'}},
    {name = 'Sailor', colors = {red = '#FF1F3A5F', green = '#FFF4F1E8', blue = '#FF2F86F6', yellow = '#FF3B2A20'}},
    {name = 'Royal', colors = {red = '#FF8E44AD', green = '#FFFFC93C', blue = '#FF2F3352', yellow = '#FF5B2C83'}},
    {name = 'Desert', colors = {red = '#FFF3D58D', green = '#FFE07A3F', blue = '#FFC8A165', yellow = '#FF8A5A3C'}},
    {name = 'Forest', colors = {red = '#FF7BC74D', green = '#FF2FB3C4', blue = '#FF5A3E2B', yellow = '#FF3E6B2E'}},
    {name = 'Ember', colors = {red = '#FFF0473E', green = '#FFF4E3C1', blue = '#FF3E5C76', yellow = '#FF5A3E2B'}},
}
-- The cards of each layout in world units around the center of the view, the figures of a row of the wardrobe and the cells across the meadow.
Recolor.layouts = {
    landscape = {view = {1700, 800}, wardrobe = {-830, -380, 1660, 380}, meadow = {-830, 20, 1660, 360}, perRow = 8, columns = 24},
    portrait = {view = {900, 1700}, wardrobe = {-430, -830, 860, 900}, meadow = {-430, 90, 860, 740}, perRow = 4, columns = 12},
}
Recolor.walkerHeight = 76

Recolor.code = [[
local mask = assets.texture('sprites/images/outfit-mask.png', {filter = 'linear', mipmaps = true})
local royal = {red = '#FF8E44AD', green = '#FFFFC93C', blue = '#FF2F3352', yellow = '#FF5B2C83'}
graphics2d.draw(base, x, y, {partMask = mask, partColors = royal})
crowd:add({x = x, y = y, width = 48, height = 76, partColors = royal})
crowd:writeFields(positions, {'x', 'y'})  -- The whole crowd walks with one call.
crowd:draw({partMask = mask})  -- One draw call, whatever the colors.]]

function Recolor:enter()
    self.base = assets.texture('sprites/images/outfit-base.png', Recolor.textureOptions)
    self.mask = assets.texture('sprites/images/outfit-mask.png', Recolor.textureOptions)
    self.time = 0
    self.layout = Recolor.layouts.landscape
    self:frame{
        code = Recolor.code,
        view = self.layout.view,
        hint = 'The first adventurer is the base art, white where the parts are, and the second shows its mask. Every outfit keeps the shading and outlines of the art, and the whole crowd draws in one call.',
        controls = {ui.button{id = 'dress', text = 'Dress the crowd again', onClick = function() self:dress() end}},
        focus = 'dress',
    }
end

-- Picks the layout that draws the cards larger in the play area, and bakes its meadow and fills it with the crowd.
function Recolor:resize(area)
    local layout = Panels.layout(Recolor.layouts, area)
    self.view = layout.view
    if layout == self.layout and self.crowd then
        return
    end
    self.layout = layout
    self:bakeMeadow()
    self:makeCrowd()
end

-- The meadow is grass with a dirt road across it and a patch of sand.
function Recolor:bakeMeadow()
    local meadow, columns = self.layout.meadow, self.layout.columns
    local inner = {meadow[1] + 20, meadow[2] + 66, meadow[3] - 40, meadow[4] - 86}
    local size = inner[3] / columns
    local rows = math.floor(inner[4] / size)
    local road = rows // 2
    self.ground = {inner[1], inner[2], inner[3], size * rows}
    self.grass, self.edges = Terrain.bake(columns, rows, size, inner[1], inner[2], function(column, row)
        if row == road and column > 1 and column < columns - 1 then
            return Terrain.path
        end
        if row == 0 and column >= columns * 0.66 and column <= columns * 0.85 then
            return Terrain.sand
        end
        return Terrain.grass
    end)
end

-- Walkers stand in lanes from the back of the meadow to the front and join the batch in that order, so the ones in front draw over the ones behind.
function Recolor:makeCrowd()
    local ground = self.ground
    self.random = m.random(11)
    local width = Recolor.walkerHeight * self.base.width / self.base.height
    self.walkers = {}
    for index = 1, Recolor.crowdSize do
        local y = ground[2] + Recolor.walkerHeight * 0.75 + self.random:range(0, ground[4] - Recolor.walkerHeight * 0.75 - 6)
        self.walkers[index] = {x = self.random:range(ground[1] + 30, ground[1] + ground[3] - 30), y = y, speed = self.random:range(40, 90) * (self.random:range(0, 1) < 0.5 and -1 or 1), phase = self.random:range(0, math.pi * 2)}
    end
    table.sort(self.walkers, function(a, b) return a.y < b.y end)
    self.crowd = graphics2d.newSpriteBatch(self.base)
    self.crowd:reserve(Recolor.crowdSize)
    for _, walker in ipairs(self.walkers) do
        walker.id = self.crowd:add({x = walker.x, y = walker.y, width = width, height = Recolor.walkerHeight, pivotY = 1, flipHorizontal = walker.speed < 0, partColors = self:pickOutfit()})
    end
    self.positions = collections.newFloatBuffer(Recolor.crowdSize * 2)
end

function Recolor:pickOutfit()
    return Recolor.outfits[self.random:integer(1, #Recolor.outfits)].colors
end

function Recolor:dress()
    for _, walker in ipairs(self.walkers) do
        self.crowd:set(walker.id, {partColors = self:pickOutfit()})
    end
end

function Recolor:update(dt)
    Recolor.super.update(self, dt)
    self.time = self.time + dt
    if self.positions then
        self:walk(dt)
    end
    self:status(string.format('%d adventurers in %d outfits, and %d draw calls for the whole stage.', Recolor.crowdSize, #Recolor.outfits, graphics2d.stats().drawCalls))
end

-- Every walker crosses the meadow with a small hop in its step and turns around at the edges, and one call writes every position.
function Recolor:walk(dt)
    local ground, positions = self.ground, self.positions
    for index, walker in ipairs(self.walkers) do
        walker.x = walker.x + walker.speed * dt
        if (walker.x < ground[1] + 30 and walker.speed < 0) or (walker.x > ground[1] + ground[3] - 30 and walker.speed > 0) then
            walker.speed = -walker.speed
            self.crowd:set(walker.id, {flipHorizontal = walker.speed < 0})
        end
        positions[index * 2 - 1] = walker.x
        positions[index * 2] = walker.y - math.abs(math.sin(self.time * 7 + walker.phase)) * 5
    end
    self.crowd:writeFields(positions, {'x', 'y'})
end

-- Each figure stands on a soft shadow with its name centered under it.
function Recolor:figure(x, feet, height, name, options)
    local width = height * self.base.width / self.base.height
    graphics2d.drawEllipse(x, feet - 4, width * 0.42, 12, '#60000000', {layer = -1})
    graphics2d.draw(self.base, x, feet, {width = width, height = height, pivotY = 1, color = options.color, partMask = options.colors and self.mask or nil, partColors = options.colors})
    if options.mask then
        graphics2d.draw(self.mask, x, feet, {width = width, height = height, pivotY = 1, layer = 1})
    end
    Panels.caption(name, x, feet + 14)
end

-- The wardrobe shows the base art, its mask and every outfit in rows, and the meadow the crowd.
function Recolor:draw(area)
    local layout = self.layout
    local inner = Panels.card(layout.wardrobe, 'One base image and one mask, many outfits')
    local perRow = layout.perRow
    local rows = math.ceil((#Recolor.outfits + 2) / perRow)
    local slot, rowHeight = inner[3] / perRow, inner[4] / rows
    local figures = {{name = 'Base art', options = {}}, {name = 'Part mask', options = {color = '#50FFFFFF', mask = true}}}
    for _, outfit in ipairs(Recolor.outfits) do
        figures[#figures + 1] = {name = outfit.name, options = {colors = outfit.colors}}
    end
    for index, figure in ipairs(figures) do
        local column, row = (index - 1) % perRow, (index - 1) // perRow
        self:figure(inner[1] + slot * (column + 0.5), inner[2] + rowHeight * (row + 1) - 46, rowHeight - 56, figure.name, figure.options)
    end

    Panels.card(layout.meadow, 'A crowd of 120 in one draw call')
    graphics2d.drawStatic(self.grass, 0, 0, {layer = -5})
    graphics2d.drawStatic(self.edges, 0, 0, {layer = -4})
    self.crowd:draw({partMask = self.mask})
end

return Recolor
