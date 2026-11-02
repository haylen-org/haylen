-- Composition: four entities of `composition.lua` built from the same small behaviors, each listed under it: a bunny that wanders and bobs, a knight that follows the cursor, a rock that shoots at the knight and a gem that circles the knight, all with health. A click or a tap hits what it lands on, and the buttons give the bunny a behavior of another entity or take it away while it runs.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local ComponentTest = require('categories.components.component-test')
local Composition = require('categories.components.composition')
local LabelUp = require('categories.components.label-up')

local CompositionTest = haylen.class('CompositionTest', ComponentTest)

-- The meadow is the green of the grass tile of the Tiled tests with its tufts and flowers, regions of `tiled/tiles/terrain.png`, scattered over cells of this size so no pattern repeats.
CompositionTest.meadow = '#FF7CC647'
CompositionTest.cell = 150
CompositionTest.tuftScale = 3.6
CompositionTest.tufts = {{56, 3, 21, 19}, {24, 23, 20, 15}, {98, 26, 20, 19}, {61, 46, 20, 17}, {2, 54, 19, 16}, {96, 68, 21, 18}, {38, 78, 20, 17}, {16, 99, 21, 19}, {82, 103, 20, 18}}
CompositionTest.flowers = {{150, 14, 43, 44}, {202, 56, 47, 43}, {148, 78, 50, 44}}

CompositionTest.code = [[
local Composition = require('composition')  -- With health-bar.lua, copied from samples/tests.
local rock = Composition.entity('Rock', rockTexture, 560, 300, 2)
rock:add(Composition.shooter({target = knight, interval = 1.2, shot = orbs, source = {0, 0, 64, 64}}))
rock:add(Composition.health({maximum = 120}))
bunny:remove('wander')  -- Behaviors come and go while it runs.
rock:update(dt)  -- Then rock:draw() where the frame draws.]]

function CompositionTest:enter()
    CompositionTest.super.enter(self)
    self.labels = LabelUp.new({font = assets.font('fonts/lilita-one-regular.ttf'), size = 36, rise = 80})
    self.meadowOptions = {source = nil, scaleX = CompositionTest.tuftScale, scaleY = CompositionTest.tuftScale, layer = -2}
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
        :add(Composition.shooter({target = self.knight, interval = 1.4, shot = self.orbs, source = {0, 0, 64, 64}, size = 96, glow = '#A0FFD34A'}))
        :add(Composition.health({maximum = 200, width = 170, tick = 50, onHit = onHit}))
    self.gem = Composition.entity('Gem', gems.texture, 0, 0, 1, gems:source('gem-blue.png'))
    self.gem:add(Composition.orbit({center = self.knight, radius = 170, speed = 0.35})):add(Composition.bob({height = 10, speed = 2})):add(Composition.health({maximum = 20, width = 80, onHit = onHit}))
    self.entities = {self.bunny, self.knight, self.rock, self.gem}
    self.drawOrder = {self.bunny, self.knight, self.rock, self.gem}

    self:frame{
        hint = 'Move the cursor to lead the knight, and click, tap or press E or the south button on an entity to hit it. The switches give the bunny behaviors of the others while it runs.',
        code = CompositionTest.code,
        play = true,
        view = ComponentTest.view,
        controls = {
            ui.toggle{id = 'shooter', text = 'Bunny shoots the knight', onChange = function(event) self:setShooter(event.checked) end},
            ui.toggle{id = 'follow', text = 'Bunny chases the cursor', onChange = function(event) self:setFollow(event.checked) end},
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

-- Gives the bunny the shooter of the rock with pink orbs, or takes it away.
function CompositionTest:setShooter(enabled)
    if enabled then
        self.bunny:add(Composition.shooter({target = self.knight, interval = 0.8, shot = self.orbs, source = {128, 0, 64, 64}, size = 84, glow = '#A0FF7FC8'}))
    else
        self.bunny:remove('shooter')
    end
end

-- Swaps the wander of the bunny for a follow of the cursor, and back.
function CompositionTest:setFollow(enabled)
    if enabled then
        self.bunny:remove('wander')
        self.bunny:add(Composition.follow({target = self.cursor, distance = 120, speed = 200}))
    else
        self.bunny:remove('follow')
        self.bunny:add(Composition.wander({speed = 120, area = {-900, 0, 700, 420}}))
    end
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
    self:status(string.format('The bunny is made of %d behaviors, the knight of %d, the rock of %d and the gem of %d.', #self.bunny.behaviors, #self.knight.behaviors, #self.rock.behaviors, #self.gem.behaviors))
end

-- Returns a number from 0 to 1 that stays the same for a cell and a salt, so the meadow keeps its tufts in place from frame to frame.
function CompositionTest.noise(column, row, salt)
    local value = (column * 374761393 + row * 668265263 + salt * 1442695041) % 2147483647
    value = ((value ~ (value >> 13)) * 1274126177) % 2147483647
    return value / 2147483647
end

-- The entities walk on a meadow seen from above, from edge to edge of the play area: one tuft or now and then a clump of flowers at a random spot of every cell, and some cells left bare.
function CompositionTest:draw(area)
    local bounds = graphics2d.canvasBounds()
    graphics2d.drawRect(bounds, CompositionTest.meadow, {layer = -3})
    local cell = CompositionTest.cell
    local options = self.meadowOptions
    for row = math.floor(bounds.y / cell), math.floor(bounds:bottom() / cell) do
        for column = math.floor(bounds.x / cell), math.floor(bounds:right() / cell) do
            local roll = CompositionTest.noise(column, row, 1)
            if roll > 0.2 then
                local list = roll > 0.96 and CompositionTest.flowers or CompositionTest.tufts
                options.source = list[math.floor(CompositionTest.noise(column, row, 2) * #list) + 1]
                local x = (column + 0.15 + CompositionTest.noise(column, row, 3) * 0.7) * cell
                local y = (row + 0.15 + CompositionTest.noise(column, row, 4) * 0.7) * cell
                graphics2d.draw(self.meadow, x, y, options)
            end
        end
    end
    table.sort(self.drawOrder, CompositionTest.isFarther)
    for _, entity in ipairs(self.drawOrder) do
        entity:draw()
        ComponentTest.name(entity.name, entity.x, entity.y - entity.lift + 10)
    end
    self.labels:draw()
end

-- The entities stand on a meadow seen from above, so the one whose feet stand higher is farther away and draws first, and the gem passes behind the knight on the far side of its orbit.
function CompositionTest.isFarther(a, b)
    return a.y < b.y
end

return CompositionTest
