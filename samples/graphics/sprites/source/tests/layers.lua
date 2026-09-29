-- Layers and y-sort: heroes wander among trees and rocks. With y-sort, whatever stands lower covers what stands higher, and a selected hero can be raised above every layer together with its shadow.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local sample = require('sample')

local Layers = haylen.class('Layers', sample.Test)

local kHeroes = 6
local kSorts = {{id = 'y', text = 'Sort by y'}, {id = 'layer', text = 'Layer only'}}
local kCode = [[
graphics2d.beginWorld(camera, {sort = 'y'})  -- lower draws cover higher ones within a layer
drawShadow(hero, {layer = 0})  hero:draw()  -- sprites stand on their pivot, here their feet
graphics2d.pushLayerOffset(10)  drawHero(selected)  graphics2d.popLayerOffset()  -- above everything, shadow included]]

-- An ellipse under a hero or a tree, on the ground layer.
local function drawShadow(x, y, width)
    local points = {}
    for index = 0, 15 do
        local angle = index / 16 * math.pi * 2
        points[index + 1] = {x + math.cos(angle) * width, y + math.sin(angle) * width * 0.35}
    end
    graphics2d.drawPolygon(points, '#50000000', {layer = 0})
end

function Layers:enter()
    self.canvas = {sort = 'y'}
    self.random = m.random(11)
    self.heroTexture = sample.texture('images/hero.png')
    self.treeTexture = sample.texture('images/tree.png')
    self.rockTexture = sample.texture('images/rock.png')
    self.heroes, self.props = {}, {}
    self.selected = 1
    self.raised = true
    self:frame({
        hint = 'Click or tap a hero to select it. With Layer only, the draws keep the order they were made in.',
        code = kCode,
        controls = {
            ui.segmentedControl{id = 'sort', items = kSorts, selected = 'y', onChange = function(event) self.canvas.sort = event.value end},
            ui.button{id = 'next', text = 'Select the next hero', onClick = function() self.selected = self.selected % kHeroes + 1 end},
            ui.toggle{id = 'raise', text = 'Raise the selected hero', checked = true, onChange = function(event) self.raised = event.checked end},
        },
        focus = 'sort',
    })
end

-- Scatters the props and the heroes over the stage once it has a size.
function Layers:resize(area)
    local random = self.random
    self.props = {}
    for index = 1, 18 do
        local texture = index % 3 == 0 and self.rockTexture or self.treeTexture
        self.props[index] = graphics2d.newSprite(texture, {x = random:range(40, area.width - 40), y = random:range(120, area.height - 10), pivotY = 1, layer = 1})
    end
    self.heroes = {}
    for index = 1, kHeroes do
        local hero = graphics2d.newSprite(self.heroTexture, {x = random:range(60, area.width - 60), y = random:range(120, area.height - 20), pivotY = 0.95, scaleX = 0.8, scaleY = 0.8, layer = 1})
        self.heroes[index] = {sprite = hero, targetX = hero.x, targetY = hero.y}
    end
end

function Layers:update(dt)
    Layers.super.update(self, dt)
    if not self.area then
        return
    end
    local random = self.random
    for _, hero in ipairs(self.heroes) do
        local sprite = hero.sprite
        local dx, dy = hero.targetX - sprite.x, hero.targetY - sprite.y
        local distance = math.sqrt(dx * dx + dy * dy)
        if distance < 4 then
            hero.targetX, hero.targetY = random:range(60, self.area.width - 60), random:range(120, self.area.height - 20)
        else
            sprite.x, sprite.y = sprite.x + dx / distance * 90 * dt, sprite.y + dy / distance * 90 * dt
            sprite.flipX = dx < 0
        end
    end

    local x, y, pressed = self:pointer()
    if pressed then
        for index, hero in ipairs(self.heroes) do
            if math.abs(hero.sprite.x - x) < 40 and y < hero.sprite.y and y > hero.sprite.y - 80 then
                self.selected = index
            end
        end
    end
    self:status(string.format('sort %s   hero %d selected%s', self.canvas.sort, self.selected, self.raised and ' and raised by 10 layers' or ''))
end

function Layers:drawHero(hero, selected)
    local sprite = hero.sprite
    drawShadow(sprite.x, sprite.y - 4, 28)
    sprite.color = selected and '#FFFFE08A' or '#FFFFFFFF'
    sprite:draw()
end

function Layers:draw(area)
    graphics2d.drawRect(area, '#FF3E6E3A', {layer = -99})
    for _, prop in ipairs(self.props) do
        drawShadow(prop.x, prop.y - 4, prop.texture == self.treeTexture and 26 or 20)
        prop:draw()
    end
    for index, hero in ipairs(self.heroes) do
        local raised = index == self.selected and self.raised
        if raised then
            graphics2d.pushLayerOffset(10)
        end
        self:drawHero(hero, index == self.selected)
        if raised then
            graphics2d.popLayerOffset()
        end
    end
end

return Layers
