-- Layers and y-sort: heroes wander among trees and rocks. With y-sort, whatever stands lower covers what stands higher, a selected hero can be raised above every layer together with its shadow, and a row of heroes on the same y keeps its places by sort ids while the drawing order shuffles every frame.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local SpriteTest = require('categories.sprites.sprite-test')

local Layers = haylen.class('Layers', SpriteTest)

Layers.heroes = 6
Layers.sorts = {{id = 'y', text = 'Sort by y'}, {id = 'layer', text = 'Layer only'}}

Layers.code = [[
graphics2d.beginWorld(camera, {sort = 'y'})  -- Lower draws cover higher ones within a layer.
drawShadow(hero, {layer = 0})  hero:draw()  -- Sprites stand on their pivot, here their feet.
graphics2d.pushLayerOffset(10)  drawHero(selected)  graphics2d.popLayerOffset()  -- Above everything, shadow included.
hero.sortId = id  -- Draws on the same y keep their places whatever order they are drawn in.]]

-- An ellipse under a hero or a tree, on the ground layer.
function Layers.drawShadow(x, y, width)
    local points = {}
    for index = 0, 15 do
        local angle = index / 16 * math.pi * 2
        points[index + 1] = {x + math.cos(angle) * width, y + math.sin(angle) * width * 0.35}
    end
    graphics2d.drawPolygon(points, '#50000000', {layer = 0})
end

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
        hint = 'Click or tap a hero to select it. With "Layer only" the draws keep the order they were made in.',
        controls = {
            ui.segmentedControl{id = 'sort', items = Layers.sorts, selected = 'y', onChange = function(event) self.sort = event.value end},
            ui.button{id = 'next', text = 'Select the next hero', onClick = function() self.selected = self.selected % Layers.heroes + 1 end},
            ui.toggle{id = 'raise', text = 'Raise the selected hero', checked = true, onChange = function(event) self.raised = event.checked end},
            ui.toggle{id = 'shuffle', text = 'Shuffle the drawing order of the row', checked = true, onChange = function(event) self.shuffled = event.checked end},
            ui.toggle{id = 'ids', text = 'Keep the row in place with sort ids', checked = true, onChange = function(event) self:setIds(event.checked) end},
        },
        focus = 'sort',
    }
end

-- Scatters the props and the heroes over the stage once it has a size.
function Layers:resize(area)
    local random = self.random
    self.props = {}
    for index = 1, 18 do
        local texture = index % 3 == 0 and self.rockTexture or self.treeTexture
        self.props[index] = graphics2d.newSprite(texture, {x = random:range(40, area.width - 40), y = random:range(120, area.height - 10), pivotY = 1, layer = 1})
    end
    self.walkers = {}
    for index = 1, Layers.heroes do
        local hero = graphics2d.newSprite(self.heroTexture, {x = random:range(60, area.width - 60), y = random:range(120, area.height - 20), pivotY = 0.95, scaleX = 0.8, scaleY = 0.8, layer = 1})
        self.walkers[index] = {sprite = hero, targetX = hero.x, targetY = hero.y}
    end
    -- Heroes that stand on the same y sort the same, so only their sort ids keep them in place.
    self.row = {}
    for index = 1, 4 do
        self.row[index] = graphics2d.newSprite(self.heroTexture, {x = area.width * 0.5 + (index - 2.5) * 34, y = area.height - 16, pivotY = 0.95, scaleX = 0.8, scaleY = 0.8, layer = 1, color = index % 2 == 0 and '#FFB8D8FF' or '#FFFFFFFF'})
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
    local random = self.random
    for _, walker in ipairs(self.walkers) do
        local sprite = walker.sprite
        local dx, dy = walker.targetX - sprite.x, walker.targetY - sprite.y
        local distance = math.sqrt(dx * dx + dy * dy)
        if distance < 4 then
            walker.targetX, walker.targetY = random:range(60, self.area.width - 60), random:range(120, self.area.height - 20)
        else
            sprite.x, sprite.y = sprite.x + dx / distance * 90 * dt, sprite.y + dy / distance * 90 * dt
            sprite.flipHorizontal = dx < 0
        end
    end

    local x, y, pressed = self:pointer()
    if pressed then
        for index, walker in ipairs(self.walkers) do
            if math.abs(walker.sprite.x - x) < 40 and y < walker.sprite.y and y > walker.sprite.y - 80 then
                self.selected = index
            end
        end
    end
    self:status(string.format('Sort "%s"   Hero %d selected%s   Row %s, %s', self.sort, self.selected, self.raised and ' and raised by 10 layers' or '', self.shuffled and 'shuffled' or 'in order', self.ids and 'with sort ids' or 'without sort ids'))
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
    Layers.drawShadow(sprite.x, sprite.y - 4, 28)
    sprite.color = selected and '#FFFFE08A' or '#FFFFFFFF'
    sprite:draw()
end

function Layers:draw(area)
    graphics2d.drawRect(graphics2d.canvasBounds(), '#FF3E6E3A', {layer = -99})
    for _, prop in ipairs(self.props) do
        Layers.drawShadow(prop.x, prop.y - 4, prop.texture == self.treeTexture and 26 or 20)
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
        sprite:draw()
    end
end

return Layers
