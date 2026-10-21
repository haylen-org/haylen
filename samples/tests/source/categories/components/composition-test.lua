-- Composition: four entities of `composition.lua` built from the same small behaviors, each listed under it: a bunny that wanders and bobs, a knight that follows the cursor, a rock that shoots at the knight and a gem that circles the knight, all with health. A click or a tap hits what it lands on, and the buttons give the bunny a behavior of another entity or take it away while it runs.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local ComponentTest = require('categories.components.component-test')
local Composition = require('categories.components.composition')
local LabelUp = require('categories.components.label-up')

local CompositionTest = haylen.class('CompositionTest', ComponentTest)

CompositionTest.meadowTile = 512

CompositionTest.code = [[
local Composition = require('composition')  -- With health-bar.lua, copied from samples/tests.
local rock = Composition.entity('Rock', rockTexture, 560, 300, 2)
rock:add(Composition.shooter({target = knight, interval = 1.2, shot = orbs, source = {0, 0, 64, 64}}))
rock:add(Composition.health({maximum = 120}))
bunny:remove('wander')  -- Behaviors come and go while it runs.
rock:update(dt)
rock:draw()]]

function CompositionTest:enter()
    CompositionTest.super.enter(self)
    self.labels = LabelUp.new({font = assets.font('fonts/lilita-one-regular.ttf'), size = 36, rise = 80})
    self.meadowOptions = {source = {2, 2, 128, 128}, width = CompositionTest.meadowTile, height = CompositionTest.meadowTile, pivotX = 0, pivotY = 0, layer = -2}
    local function picture(name)
        return assets.texture('sprites/images/' .. name .. '.png', {filter = 'linear'})
    end
    local function onHit(entity, taken)
        self.labels:spawn(entity.x, entity.y - entity.lift - entity:height() - 40, taken > 0 and ('-' .. math.floor(taken + 0.5)) or 'Down', taken > 0 and 'damage' or 'miss')
    end
    local gems = assets.load('sprites/atlases/gems.json', 'atlas', {filter = 'linear'})
    self.orbs = picture('bullets')
    self.meadow = assets.texture('tiled/tiles/terrain.png', {filter = 'linear', mipmaps = true})

    self.cursor = {x = 0, y = 300}
    self.knight = Composition.entity('Knight', picture('hero'), -200, 300, 1.2)
        :add(Composition.follow({target = self.cursor, distance = 40}))
        :add(Composition.health({maximum = 120, width = 150, tick = 30, onHit = onHit}))
    self.bunny = Composition.entity('Bunny', picture('bunny'), -600, 200, 2)
        :add(Composition.wander({speed = 120, area = {-900, 0, 700, 420}}))
        :add(Composition.bob({height = 24, speed = 1.6}))
        :add(Composition.health({maximum = 40, onHit = onHit}))
    self.rock = Composition.entity('Rock', picture('rock'), 620, 360, 2)
        :add(Composition.shooter({target = self.knight, interval = 1.4, shot = self.orbs, source = {0, 0, 64, 64}, size = 56}))
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
            ui.sectionTitle{text = 'What each one is made of'},
            self:behaviorRow(self.knight),
            self:behaviorRow(self.bunny),
            self:behaviorRow(self.rock),
            self:behaviorRow(self.gem),
        },
    }
end

-- A row of the panel names an entity and lists its behaviors, which the update keeps in step as they come and go.
function CompositionTest:behaviorRow(entity)
    return ui.column{gap = 4,
        ui.label{text = entity.name, font = 'heading'},
        ui.label{id = 'made-' .. entity.name, text = CompositionTest.listed(entity), color = 'textMuted'},
    }
end

function CompositionTest.listed(entity)
    local names = {}
    for index, name in ipairs(entity.names) do
        names[index] = index == 1 and name:sub(1, 1):upper() .. name:sub(2) or name
    end
    return table.concat(names, ', ') .. '.'
end

function CompositionTest:toggleShooter()
    if self.bunny:remove('shooter') then
        self:set('shooter', {text = 'Give the bunny a shooter'})
        return
    end
    self.bunny:add(Composition.shooter({target = self.knight, interval = 0.8, shot = self.orbs, source = {128, 0, 64, 64}, size = 44}))
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
    self.made = self.made or {}
    for _, entity in ipairs(self.entities) do
        local listed = CompositionTest.listed(entity)
        if self.made[entity] ~= listed then
            self.made[entity] = listed
            self:set('made-' .. entity.name, {text = listed})
        end
    end
    self:status(string.format('The bunny is made of %d behaviors, the knight of %d, the rock of %d and the gem of %d', #self.bunny.behaviors, #self.knight.behaviors, #self.rock.behaviors, #self.gem.behaviors))
end

-- The entities walk on a meadow seen from above, the seamless grass of the Tiled tests from edge to edge of the play area.
function CompositionTest:draw(area)
    local bounds = graphics2d.canvasBounds()
    local tile = CompositionTest.meadowTile
    local options = self.meadowOptions
    for y = bounds.y - (bounds.y % tile), bounds:bottom(), tile do
        for x = bounds.x - (bounds.x % tile), bounds:right(), tile do
            graphics2d.draw(self.meadow, x, y, options)
        end
    end
    for _, entity in ipairs(self.entities) do
        entity:draw()
        ComponentTest.name(entity.name, entity.x, entity.y - entity.lift + 10)
    end
    self.labels:draw()
end

return CompositionTest
