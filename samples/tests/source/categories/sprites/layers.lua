-- Layers and y-sort: heroes wander a meadow among trees and rocks. With y-sort, whatever stands lower covers what stands higher, a selected hero can be raised above every layer together with its shadow, and a row of heroes on the same y keeps its places by sort ids while the drawing order shuffles every frame.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local Plate = require('categories.sprites.plate')
local SpriteTest = require('categories.sprites.sprite-test')
local Terrain = require('categories.sprites.terrain')

local Layers = haylen.class('Layers', SpriteTest)

Layers.heroes = 6
Layers.props = 14
Layers.sorts = {{id = 'y', text = 'Sort by y'}, {id = 'layer', text = 'As drawn'}}
Layers.heroScale = 0.6
-- The band at the bottom of the meadow that holds the row, which the trees and the wandering heroes keep out of.
Layers.rowBand = 190
Layers.speed = 90
Layers.selection = {outlineWidth = 6, outlineColor = '#FFFFC93C', glowSize = 14, glowColor = '#90FFC93C'}
Layers.plain = {}

Layers.code = [[
graphics2d.beginWorld(camera, {sort = 'y'})  -- Lower draws cover higher ones within a layer.
graphics2d.drawEllipse(hero.x, hero.y, 34, 10, '#50000000', {layer = 0})
hero:draw()  -- A sprite stands on its pivot, here its feet.
graphics2d.pushLayerOffset(10)  -- The selected hero and its shadow draw above everything.
graphics2d.popLayerOffset()
hero.sortId = id  -- Draws on the same y keep their places whatever order they are made in.]]

function Layers:enter()
    self.sort = 'y'
    self.random = m.random(11)
    self.heroTexture = SpriteTest.texture('images/hero.png')
    self.treeTexture = SpriteTest.texture('images/tree.png')
    self.rockTexture = SpriteTest.texture('images/rock.png')
    self.walkers, self.props = {}, {}
    self.selected = 1
    self.raised = true
    self.shuffled, self.ids = true, true
    self.row = {}
    self:frame{
        code = Layers.code,
        hint = 'Click or tap a hero to select it. "As drawn" keeps every draw of a layer in the order it was made.',
        controls = {
            ui.formField{label = 'Order of the draws', ui.segmentedControl{id = 'sort', items = Layers.sorts, selected = 'y', onChange = function(event) self.sort = event.value end}},
            ui.button{id = 'next', text = 'Select the next hero', align = 'stretch', onClick = function() self.selected = self.selected % Layers.heroes + 1 end},
            ui.toggle{id = 'raise', text = 'Raise the selection', checked = true, onChange = function(event) self.raised = event.checked end},
            ui.toggle{id = 'shuffle', text = 'Shuffle the row', checked = true, onChange = function(event) self.shuffled = event.checked end},
            ui.toggle{id = 'ids', text = 'Sort ids in the row', checked = true, onChange = function(event) self:setIds(event.checked) end},
        },
        focus = 'sort',
    }
end

-- Returns a random spot of the meadow above the band of the row.
function Layers:spot(margin)
    return self.random:range(margin, self.area.width - margin), self.random:range(150, self.area.height - Layers.rowBand)
end

-- Bakes the meadow and scatters the props and the heroes over it once the stage has a size.
function Layers:resize(area)
    local size = 96
    self.grass = Terrain.batches(math.ceil(area.width / size), math.ceil(area.height / size), size, 0, 0, function() return Terrain.grass end):bake()
    self.props = {}
    for index = 1, Layers.props do
        local rock = index % 3 == 0
        local x, y = self:spot(60)
        self.props[index] = graphics2d.newSprite(rock and self.rockTexture or self.treeTexture, {x = x, y = y, pivotY = rock and 0.9 or 0.96, scaleX = rock and 0.7 or 0.8, scaleY = rock and 0.7 or 0.8, layer = 1})
    end
    self.walkers = {}
    for index = 1, Layers.heroes do
        local x, y = self:spot(70)
        local hero = graphics2d.newSprite(self.heroTexture, {x = x, y = y, pivotY = 0.95, scaleX = Layers.heroScale, scaleY = Layers.heroScale, layer = 1})
        self.walkers[index] = {sprite = hero, targetX = x, targetY = y}
    end
    -- Heroes that stand on the same y sort the same, so only their sort ids keep them in place.
    self.row = {}
    self.rowY = area.height - 44
    for index = 1, 4 do
        self.row[index] = graphics2d.newSprite(self.heroTexture, {x = area.width * 0.32 + (index - 2.5) * 58, y = self.rowY, pivotY = 0.95, scaleX = Layers.heroScale, scaleY = Layers.heroScale, layer = 1, color = index % 2 == 0 and '#FFFFD27A' or '#FFFFFFFF'})
    end
    self:setIds(self.ids)
end

function Layers:setIds(ids)
    self.ids = ids
    for index, sprite in ipairs(self.row) do
        sprite.sortId = ids and index or 0
    end
end

function Layers:update(dt)
    Layers.super.update(self, dt)
    if not self.area then
        return
    end
    for _, walker in ipairs(self.walkers) do
        local sprite = walker.sprite
        local dx, dy = walker.targetX - sprite.x, walker.targetY - sprite.y
        local distance = math.sqrt(dx * dx + dy * dy)
        if distance < 4 then
            walker.targetX, walker.targetY = self:spot(70)
        else
            sprite.x, sprite.y = sprite.x + dx / distance * Layers.speed * dt, sprite.y + dy / distance * Layers.speed * dt
            sprite.flipHorizontal = dx < 0
        end
    end

    local x, y, pressed = self:pointer()
    if pressed then
        for index, walker in ipairs(self.walkers) do
            local sprite = walker.sprite
            if math.abs(sprite.x - x) < 50 and y < sprite.y and y > sprite.y - 110 then
                self.selected = index
            end
        end
    end

    local order = self.sort == 'y' and 'Draws sort by the y they stand on' or 'Draws keep the order they are made in'
    local selection = string.format('hero %d is selected%s', self.selected, self.raised and ' and raised above everything' or '')
    local row = 'the row draws in order'
    if self.shuffled then
        row = self.ids and 'the row shuffles every frame while its sort ids keep it in place' or 'the row shuffles every frame and flickers without sort ids'
    end
    self:status(string.format('%s, %s, and %s.', order, selection, row))
end

-- The canvas sorts by the y of each draw within a layer, so it takes its own options instead of the stage canvas of the frame.
function Layers:render()
    if self.area then
        graphics2d.beginWorld(self.camera, {sort = self.sort})
        self:draw(self.area)
    end
end

function Layers:drawHero(walker, selected)
    local sprite = walker.sprite
    if selected then
        graphics2d.drawShape({sprite.x - 44, sprite.y - 18, 88, 26}, {kind = 'ellipse', color = '#40FFC93C', borderWidth = 4, borderColor = '#FFFFC93C', layer = 0})
    else
        graphics2d.drawEllipse(sprite.x, sprite.y - 5, 36, 11, '#50000000', {layer = 0})
    end
    sprite.effect = selected and Layers.selection or Layers.plain
    sprite:draw()
end

function Layers:draw(area)
    graphics2d.drawStatic(self.grass, 0, 0, {layer = -99})
    for _, prop in ipairs(self.props) do
        local tree = prop.texture == self.treeTexture
        graphics2d.drawEllipse(prop.x, prop.y - 6, tree and 42 or 34, tree and 12 or 10, '#50000000', {layer = 0})
        prop:draw()
    end
    for index, walker in ipairs(self.walkers) do
        local raised = index == self.selected and self.raised
        if raised then
            graphics2d.pushLayerOffset(10)
        end
        self:drawHero(walker, index == self.selected)
        if raised then
            graphics2d.popLayerOffset()
        end
    end

    if self.shuffled then
        self.random:shuffle(self.row)
    end
    for _, sprite in ipairs(self.row) do
        graphics2d.drawEllipse(sprite.x, sprite.y - 5, 36, 11, '#50000000', {layer = 0})
        sprite:draw()
    end
    Plate.draw('Four heroes on one y', area.width * 0.32 + 290, self.rowY - 40, {layer = 30})
end

return Layers
