-- Composition: four entities of `composition.lua` built from the same small behaviors, each listed under it: a bunny that wanders and bobs, a knight that follows the cursor, a rock that shoots at the knight and a gem that circles the knight, all with health. A click or a tap hits what it lands on, and the buttons give the bunny a behavior of another entity or take it away while it runs.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local ComponentTest = require('categories.components.component-test')
local Composition = require('categories.components.composition')
local LabelUp = require('categories.components.label-up')
local Test = require('harness.test')

local CompositionTest = haylen.class('CompositionTest', ComponentTest)

CompositionTest.code = [[
local Composition = require('composition')  -- With health-bar.lua, copied from samples/tests.
local rock = Composition.entity('Rock', rockTexture, 560, 300, 2)
rock:add(Composition.shooter({target = knight, interval = 1.2})):add(Composition.health({maximum = 120}))
bunny:add(Composition.shooter({target = knight}))  bunny:remove('shooter')  -- Behaviors come and go while it runs.
rock:update(dt)  rock:draw()  rock:hit(15)]]

function CompositionTest:enter()
    CompositionTest.super.enter(self)
    self.labels = LabelUp.new({size = 30, rise = 80})
    local function picture(name)
        return assets.texture('sprites/images/' .. name .. '.png', {filter = 'linear'})
    end
    local function onHit(entity, taken)
        self.labels:spawn(entity.x, entity.y - entity.lift - entity:height() - 40, taken > 0 and ('-' .. math.floor(taken + 0.5)) or 'Down', taken > 0 and 'damage' or 'miss')
    end
    local gems = assets.load('sprites/atlases/gems.json', 'atlas', {filter = 'linear'})

    self.cursor = {x = 0, y = 300}
    self.knight = Composition.entity('Knight', picture('hero'), -200, 300, 1.2)
        :add(Composition.follow({target = self.cursor, distance = 40}))
        :add(Composition.health({maximum = 120, width = 150, tick = 30, onHit = onHit}))
    self.bunny = Composition.entity('Bunny', picture('bunny'), -600, 200, 2)
        :add(Composition.wander({speed = 120, area = {-900, 0, 700, 420}}))
        :add(Composition.bob({height = 24, speed = 1.6}))
        :add(Composition.health({maximum = 40, onHit = onHit}))
    self.rock = Composition.entity('Rock', picture('rock'), 620, 360, 2)
        :add(Composition.shooter({target = self.knight, interval = 1.4}))
        :add(Composition.health({maximum = 200, width = 170, tick = 50, onHit = onHit}))
    self.gem = Composition.entity('Gem', gems.texture, 0, 0, 1, gems:source('gem-blue.png'))
    self.gem:add(Composition.orbit({center = self.knight, radius = 170, speed = 0.35})):add(Composition.bob({height = 10, speed = 2})):add(Composition.health({maximum = 20, width = 80, onHit = onHit}))
    self.entities = {self.bunny, self.knight, self.rock, self.gem}

    self:frame{
        hint = 'Move the cursor to lead the knight, and click, tap or press E or the south button on an entity to hit it. The buttons change the behaviors of the bunny while it runs.',
        code = CompositionTest.code,
        play = true,
        view = ComponentTest.view,
        controls = {
            ui.button{id = 'shooter', text = 'Give the bunny a shooter', variant = 'primary', onClick = function() self:toggleShooter() end},
            ui.button{id = 'follow', text = 'Make the bunny follow the cursor', onClick = function() self:toggleFollow() end},
        },
    }
end

function CompositionTest:toggleShooter()
    if self.bunny:remove('shooter') then
        self:set('shooter', {text = 'Give the bunny a shooter'})
        return
    end
    self.bunny:add(Composition.shooter({target = self.knight, interval = 0.8, color = '#FFFF8A84'}))
    self:set('shooter', {text = 'Take the shooter from the bunny'})
end

-- Swaps the wander of the bunny for a follow of the cursor, and back.
function CompositionTest:toggleFollow()
    if self.bunny:remove('follow') then
        self.bunny:add(Composition.wander({speed = 120, area = {-900, 0, 700, 420}}))
        self:set('follow', {text = 'Make the bunny follow the cursor'})
        return
    end
    self.bunny:remove('wander')
    self.bunny:add(Composition.follow({target = self.cursor, distance = 120, speed = 200}))
    self:set('follow', {text = 'Let the bunny wander again'})
end

-- Returns the entity whose drawing covers the cursor, or nothing.
function CompositionTest:entityUnder()
    local x, y = self.pointer.worldX, self.pointer.worldY
    for _, entity in ipairs(self.entities) do
        local width, height = entity:width(), entity:height()
        local bottom = entity.y - entity.lift
        if math.abs(x - entity.x) < width / 2 and y < bottom and y > bottom - height then
            return entity
        end
    end
end

function CompositionTest:update(dt)
    CompositionTest.super.update(self, dt)
    if not self.stage then
        return
    end

    self.cursor.x, self.cursor.y = self.pointer.worldX, math.max(0, math.min(500, self.pointer.worldY))
    if self:pressed() then
        local entity = self:entityUnder()
        if entity then
            entity:hit(math.random(6, 16))
        end
    end
    for _, entity in ipairs(self.entities) do
        entity:update(dt)
    end
    self.labels:update(dt)
    self:status(string.format('Bunny made of %d behaviors, knight %d, rock %d, gem %d', #self.bunny.behaviors, #self.knight.behaviors, #self.rock.behaviors, #self.gem.behaviors))
end

function CompositionTest:draw(area)
    graphics2d.drawRect({-960, 0, 1920, 540}, '#FF222A3D')
    for _, entity in ipairs(self.entities) do
        entity:draw()
        Test.caption(entity.name .. ': ' .. table.concat(entity.names, ', '), entity.x, entity.y - entity.lift + 12, {anchor = {0.5, 0}, color = Test.ink, size = 22, layer = 4})
    end
    self.labels:draw()
end

return CompositionTest
