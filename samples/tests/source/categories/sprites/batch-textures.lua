-- Batches of several textures: a meadow of groves of trees with rocks around them, bunnies hopping in small groups and heroes walking a dirt road, from four separate images in one sprite batch that draws them in one call, with a material that sways each sprite by the phase, strength and brightness of its own sprite data. The walkers move with one call that writes the positions of the whole batch.
local assets = require('haylen.assets')
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local Panels = require('categories.sprites.panels')
local SpriteTest = require('categories.sprites.sprite-test')
local Terrain = require('categories.sprites.terrain')

local BatchTextures = haylen.class('BatchTextures', SpriteTest)

BatchTextures.images = {'hero', 'rock', 'tree', 'bunny'}
BatchTextures.hero, BatchTextures.rock, BatchTextures.tree, BatchTextures.bunny = 1, 2, 3, 4
BatchTextures.cell = 84
BatchTextures.layouts = {
    landscape = {view = {1700, 800}, card = {-830, -380, 1660, 760}},
    portrait = {view = {900, 1700}, card = {-430, -830, 860, 1660}},
}

BatchTextures.code = [[
local meadow = graphics2d.newSpriteBatch({hero, rock, tree, bunny})
meadow:add({x = 100, y = 200, pivotY = 1, texture = 3, data = {phase, strength, brightness}})
meadow:writeFields(positions, {'x', 'y'})  -- The walkers move with one call.
meadow:draw({material = sway})  -- One draw call for the four textures.]]

function BatchTextures:enter()
    self.textures = {}
    for index, name in ipairs(BatchTextures.images) do
        self.textures[index] = SpriteTest.texture('images/' .. name .. '.png')
    end
    self.sway = graphics2d.newMaterial(assets.shader('shaders/sway.shader'))
    self.meadow = graphics2d.newSpriteBatch(self.textures)
    self.swaying = true
    self.time = 0
    self.layout = BatchTextures.layouts.landscape
    self:frame{
        code = BatchTextures.code,
        view = self.layout.view,
        hint = 'Every sprite picks one of four textures and sways by its own data, and the whole meadow stays one draw call.',
        controls = {ui.toggle{id = 'sway', text = 'Sway each sprite by its data', checked = true, onChange = function(event) self.swaying = event.checked end}},
        focus = 'sway',
    }
end

-- Picks the layout that draws the meadow larger in the play area, bakes its ground and fills it again.
function BatchTextures:resize(area)
    local layout = Panels.layout(BatchTextures.layouts, area)
    self.view = layout.view
    if layout == self.layout and self.grass then
        return
    end
    self.layout = layout
    local card = layout.card
    local columns, rows = math.floor((card[3] - 40) / BatchTextures.cell), math.floor((card[4] - 86) / BatchTextures.cell)
    local size = math.min((card[3] - 40) / columns, (card[4] - 86) / rows)
    self.field = {card[1] + (card[3] - columns * size) / 2, card[2] + 66, columns * size, rows * size}
    self.road = math.floor(rows * 0.62)
    self.grass, self.edges = Terrain.bake(columns, rows, size, self.field[1], self.field[2], function(column, row)
        return row == self.road and Terrain.path or Terrain.grass
    end)
    self.roadY = self.field[2] + self.road * size
    self.roadHeight = size
    self:fill()
end

-- Adds a sprite of an image whose feet stand at a point of the field, `height` units tall, with the strength of its sway and a walk when it moves, unless it would stand on the road or reach past the field.
function BatchTextures:place(sprites, which, x, y, height, strength, walk)
    local field = self.field
    local onRoad = which ~= BatchTextures.hero and y > self.roadY - self.roadHeight * 0.5 - 6 and y < self.roadY + self.roadHeight * 0.5 + height * 0.3
    if onRoad or y - height * 0.9 < field[2] or y > field[2] + field[4] - 4 or x < field[1] + 20 or x > field[1] + field[3] - 20 then
        return
    end
    sprites[#sprites + 1] = {which = which, x = x, y = y, height = height, strength = strength, walk = walk}
end

-- Lays out the groves, the rocks, the groups of bunnies and the parties of heroes, and adds them to the batch from the back to the front.
function BatchTextures:fill()
    local random = m.random(3)
    local field = self.field
    local sprites = {}
    local function spot(top, bottom)
        return field[1] + random:range(0.03, 0.97) * field[3], top + random:range(0, 1) * (bottom - top)
    end
    local roadTop, roadBottom = self.roadY - self.roadHeight * 0.5, self.roadY + self.roadHeight * 0.5
    local groves = math.floor(field[3] * field[4] / 90000)
    for _ = 1, groves do
        local x, y = spot(field[2] + 90, roadTop - 30)
        if random:range(0, 1) < 0.4 then
            x, y = spot(roadBottom + 80, field[2] + field[4] - 4)
        end
        for _ = 1, random:integer(6, 11) do
            local treeY = y + random:range(-50, 50)
            self:place(sprites, BatchTextures.tree, x + random:range(-90, 90), treeY, random:range(96, 140), random:range(0.04, 0.09))
        end
        for _ = 1, random:integer(4, 8) do
            self:place(sprites, BatchTextures.rock, x + random:range(-130, 130), y + random:range(-40, 70), random:range(26, 44), 0)
        end
    end
    for _ = 1, groves * 2 do
        local x, y = spot(field[2] + 40, field[2] + field[4] - 4)
        if y < roadTop - 10 or y > roadBottom + 10 then
            for _ = 1, random:integer(5, 9) do
                self:place(sprites, BatchTextures.bunny, x + random:range(-60, 60), y + random:range(-24, 24), random:range(30, 40), 0.02, {speed = random:range(-26, 26), range = 60, hop = 9})
            end
        end
    end
    for _ = 1, math.floor(field[3] / 70) do
        local x = field[1] + random:range(0.02, 0.98) * field[3]
        self:place(sprites, BatchTextures.hero, x, roadTop + 14 + random:range(0, self.roadHeight - 18), random:range(58, 66), 0, {speed = random:range(30, 60) * (random:range(0, 1) < 0.5 and -1 or 1), range = field[3], hop = 4})
    end
    table.sort(sprites, function(a, b) return a.y < b.y end)

    self.meadow:clear()
    self.meadow:reserve(#sprites)
    self.walkers = {}
    self.positions = collections.newFloatBuffer(#sprites * 2)
    for index, sprite in ipairs(sprites) do
        local texture = self.textures[sprite.which]
        local width = sprite.height * texture.width / texture.height
        self.meadow:add({x = sprite.x, y = sprite.y, width = width, height = sprite.height, pivotY = 1, texture = sprite.which, data = {random:range(0, math.pi * 2), sprite.strength, random:range(0, 0.15)}})
        self.positions[index * 2 - 1], self.positions[index * 2] = sprite.x, sprite.y
        if sprite.walk then
            local walk = sprite.walk
            self.walkers[#self.walkers + 1] = {index = index, x = sprite.x, y = sprite.y, home = sprite.x, speed = walk.speed, range = walk.range, hop = walk.hop, phase = random:range(0, math.pi * 2)}
            self.meadow:set(index, {flipHorizontal = walk.speed < 0})
        end
    end
end

function BatchTextures:update(dt)
    BatchTextures.super.update(self, dt)
    self.time = self.time + dt
    self.sway:set('time', self.time)
    if self.positions then
        self:walk(dt)
    end
    self:status(string.format('The meadow draws %d sprites of %d textures in one batch, and the whole stage takes %d draw calls.', self.meadow:size(), #self.textures, graphics2d.stats().drawCalls))
end

-- Bunnies hop back and forth around their group and heroes walk the road and turn at its ends, and one call writes every position.
function BatchTextures:walk(dt)
    local field, positions = self.field, self.positions
    for _, walker in ipairs(self.walkers) do
        walker.x = walker.x + walker.speed * dt
        local low, high = math.max(field[1] + 16, walker.home - walker.range), math.min(field[1] + field[3] - 16, walker.home + walker.range)
        if (walker.x < low and walker.speed < 0) or (walker.x > high and walker.speed > 0) then
            walker.speed = -walker.speed
            self.meadow:set(walker.index, {flipHorizontal = walker.speed < 0})
        end
        positions[walker.index * 2 - 1] = walker.x
        positions[walker.index * 2] = walker.y - math.abs(math.sin(self.time * 6 + walker.phase)) * walker.hop
    end
    self.meadow:writeFields(positions, {'x', 'y'})
end

function BatchTextures:draw(area)
    Panels.card(self.layout.card, 'Four textures, one batch, one draw call')
    graphics2d.drawStatic(self.grass, 0, 0, {layer = -5})
    graphics2d.drawStatic(self.edges, 0, 0, {layer = -4})
    self.meadow:draw(self.swaying and {material = self.sway} or nil)
end

return BatchTextures
