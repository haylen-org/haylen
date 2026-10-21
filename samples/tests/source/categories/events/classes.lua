-- Classes: units built with `haylen.class`, a hero that inherits and calls `super`, walking and flying mixins that hear when a class includes them, an equality metamethod that subclasses copy, and `is` checks on objects and classes. The units walk and fly over a meadow, each drawn from the sheet of its kind.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local Backdrop = require('categories.animation.backdrop')
local EventsTest = require('categories.events.events-test')
local Journal = require('harness.journal')
local Test = require('harness.test')

local Classes = haylen.class('Classes', EventsTest)

local kCode = [[
local Unit = haylen.class('Unit')
function Unit.__eq(a, b) return a.name == b.name end
local Walker = {walk = walk, included = function(class) print(class.name .. ' can walk') end}
local Hero = haylen.class('Hero', Unit, Walker)
function Hero:init(name, x) Hero.super.init(self, name, 100, x) end
print(Hero('Ana'):is(Unit), Dragon:is(Flyer), Hero('Ana') == Hero('Ana'))]]

-- How each kind of unit draws: its sheet, the cells of its frames in a grid of cells of 256 pixels, how fast they play, how tall it stands and where its feet are in its cells.
local kLooks = {
    Hero = {sheet = 'sprites/sheets/walker.png', columns = 8, frames = {8, 9, 10, 11, 12, 13, 14, 15}, rate = 12, size = 150, feet = 248 / 256},
    Bat = {sheet = 'events/bat.png', columns = 2, frames = {0, 1}, rate = 8, size = 110, feet = 0.5},
    Dragon = {sheet = 'events/dragon.png', columns = 2, frames = {0, 1}, rate = 4, size = 170, feet = 248 / 256},
}
-- New units take the next of these places across the meadow.
local kSpots = 8

-- Builds the classes of the test. The mixins write to the journal when a class includes them.
local function defineClasses(journal)
    local Unit = haylen.class('Unit')

    function Unit:init(name, health, x)
        self.name, self.health, self.x, self.lift, self.direction, self.phase = name, health, x, 0, 1, x * 7
    end

    function Unit:describe()
        return self.name .. ' with ' .. self.health .. ' health'
    end

    function Unit.__eq(a, b)
        return a.name == b.name
    end

    local Walker = {
        walk = function(self, dt, left, right)
            self.x = self.x + self.speed * self.direction * dt
            if self.x < left and self.direction < 0 or self.x > right and self.direction > 0 then
                self.direction = -self.direction
            end
        end,
        included = function(class) journal:add('The class "' .. class.name .. '" includes "Walker".', Test.green) end,
    }
    local Flyer = {
        fly = function(self, time)
            self.lift = 120 + math.sin(time * 2.4 + self.phase) * 50
        end,
        included = function(class) journal:add('The class "' .. class.name .. '" includes "Flyer".', Test.green) end,
    }

    local Hero = haylen.class('Hero', Unit, Walker)

    function Hero:init(name, x)
        Hero.super.init(self, name, 100, x)
        self.speed = 160
    end

    function Hero:describe()
        return 'the hero ' .. Hero.super.describe(self)
    end

    local Bat = haylen.class('Bat', Unit, Flyer)

    function Bat:init(name, x)
        Bat.super.init(self, name, 20, x)
    end

    local Dragon = haylen.class('Dragon', Unit, Walker, Flyer)

    function Dragon:init(name, x)
        Dragon.super.init(self, name, 500, x)
        self.speed = 70
    end

    return {Unit = Unit, Walker = Walker, Flyer = Flyer, Hero = Hero, Bat = Bat, Dragon = Dragon}
end

function Classes:enter()
    self.journal = Journal()
    self.classes = defineClasses(self.journal)
    self.units = {}
    self.time = 0
    self.backdrop = Backdrop()
    self.meadowShape = function() graphics2d.drawShape(self.meadow, {radius = 18}) end
    self.sheets = {}
    for name, look in pairs(kLooks) do
        self.sheets[name] = assets.texture(look.sheet, {filter = 'linear'})
    end
    self:frame({
        hint = 'Spawn units and run the checks. Walkers pace the ground, flyers bob in the air, and the dragon does both.',
        code = kCode,
        controls = {
            ui.button{id = 'hero', text = 'Spawn a hero', variant = 'primary', onClick = function() self:addUnit('Hero', 'Ana') end},
            ui.button{id = 'bat', text = 'Spawn a bat', onClick = function() self:addUnit('Bat', 'Bat') end},
            ui.button{id = 'dragon', text = 'Spawn a dragon', onClick = function() self:addUnit('Dragon', 'Ember') end},
            ui.button{id = 'check', text = 'Run the checks', onClick = function() self:check() end},
        },
        focus = 'hero',
    })
    self:addUnit('Hero', 'Ana')
    self:addUnit('Bat', 'Bat')
    self:addUnit('Dragon', 'Ember')
end

-- Places each new unit on the next spot across the meadow, as a share of its width until the meadow has a size.
function Classes:addUnit(className, name)
    local share = (#self.units % kSpots + 0.5) / kSpots
    local unit = self.classes[className](name, self.meadow and self.meadow.x + share * self.meadow.width or share)
    unit.kind = className
    table.insert(self.units, unit)
    self.journal:add('Spawned ' .. unit:describe() .. '.')
end

function Classes:check()
    local classes, journal = self.classes, self.journal
    local ana = classes.Hero('Ana', 0)
    journal:add(string.format('A hero is a unit: %s. A walker: %s. A flyer: %s.', Classes.yes(ana:is(classes.Unit)), Classes.yes(ana:is(classes.Walker)), Classes.yes(ana:is(classes.Flyer))), Test.warm)
    journal:add(string.format('The dragon class is a flyer: %s. The bat class is a walker: %s.', Classes.yes(classes.Dragon:is(classes.Flyer)), Classes.yes(classes.Bat:is(classes.Walker))), Test.warm)
    journal:add(string.format('Two heroes named Ana are equal through the "__eq" that "Hero" copied from "Unit": %s.', Classes.yes(ana == classes.Hero('Ana', 50))), Test.warm)
    journal:add(string.format('The class prints as "%s", its "super" is "Unit": %s, and its name is "%s".', tostring(classes.Hero), Classes.yes(classes.Hero.super == classes.Unit), classes.Hero.name), Test.warm)
end

function Classes.yes(value)
    return value and 'yes' or 'no'
end

-- Lays out the meadow over the top of the play area and moves the units that waited for it to their spots.
function Classes:resize(area)
    local first = self.meadow == nil
    self.meadow = m.rect(24, 24, area.width - 48, math.max(260, area.height * 0.58))
    if first then
        for _, unit in ipairs(self.units) do
            unit.x = self.meadow.x + unit.x * self.meadow.width
        end
    end
end

function Classes:update(dt)
    Classes.super.update(self, dt)
    self.time = self.time + dt
    if self.meadow then
        for _, unit in ipairs(self.units) do
            if unit.walk then
                unit:walk(dt, self.meadow.x + 60, self.meadow:right() - 60)
            end
            if unit.fly then
                unit:fly(self.time)
            end
        end
    end
    self:status(string.format('%d units walk and fly, each one made by its class.', #self.units))
end

function Classes:drawUnit(unit, ground)
    local look = kLooks[unit.kind]
    local frame = look.frames[math.floor(self.time * look.rate + unit.phase) % #look.frames + 1]
    local size = look.size * 256 / 236
    local y = ground - unit.lift
    Backdrop.shadow(unit.x, ground, look.size * (unit.lift > 0 and 0.4 or 0.6), 3)
    graphics2d.draw(self.sheets[unit.kind], unit.x, y, {source = {frame % look.columns * 256, frame // look.columns * 256, 256, 256}, width = size, height = size, pivotY = look.feet, flipHorizontal = unit.walk ~= nil and unit.direction < 0, layer = 4})
    local head = look.feet == 0.5 and look.size * 0.5 or look.size
    self:pill(unit.name, unit.x, y - head - 10, '#E6151A2A', {color = '#FFFFFFFF', layer = 6})
end

function Classes:draw(area)
    local meadow = self.meadow
    local ground = meadow:bottom() - 36
    graphics2d.pushMask(self.meadowShape)
    self.backdrop:draw(meadow, ground, 150, 0)
    for _, unit in ipairs(self.units) do
        self:drawUnit(unit, ground)
    end
    graphics2d.popMask()
    graphics2d.drawShape(meadow, {radius = 18, color = '#00000000', borderWidth = 3, borderColor = EventsTest.cardBorder, layer = 8})
    local top = meadow:bottom() + 16
    self:logPanel(self.journal, {24, top, area.width - 48, area.height - top - 24}, 'What happened')
end

return Classes
