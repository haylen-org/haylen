-- Classes: units built with `haylen.class`, a hero that inherits and calls `super`, walking and flying mixins that hear when a class includes them, an equality metamethod that subclasses copy, and `is` checks on objects and classes.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Journal = require('journal')
local sample = require('sample')

local Classes = haylen.class('Classes', sample.Test)

local kCode = [[
local Unit = haylen.class('Unit')  function Unit.__eq(a, b) return a.name == b.name end
local Walker = {walk = function(self, dt) ... end, included = function(class) print(class.name .. ' can walk') end}
local Hero = haylen.class('Hero', Unit, Walker)
function Hero:init(name, x) Hero.super.init(self, name, 100, x) end
print(Hero('Ana'):is(Unit), Dragon:is(Flyer), Hero('Ana') == Hero('Ana'), tostring(Hero))]]

-- Builds the classes of the test. The mixins write to the journal when a class includes them.
local function defineClasses(journal)
    local Unit = haylen.class('Unit')

    function Unit:init(name, health, x)
        self.name, self.health, self.x, self.lift, self.direction = name, health, x, 0, 1
    end

    function Unit:describe()
        return self.name .. ' with ' .. self.health .. ' health'
    end

    function Unit.__eq(a, b)
        return a.name == b.name
    end

    local Walker = {
        walk = function(self, dt, width)
            self.x = self.x + self.speed * self.direction * dt
            if self.x < 40 or self.x > width - 40 then
                self.direction = -self.direction
            end
        end,
        included = function(class) journal:add(class.name .. ' includes Walker', sample.green) end,
    }
    local Flyer = {
        fly = function(self, time)
            self.lift = 60 + math.sin(time * 3 + self.x * 0.01) * 40
        end,
        included = function(class) journal:add(class.name .. ' includes Flyer', sample.green) end,
    }

    local Hero = haylen.class('Hero', Unit, Walker)

    function Hero:init(name, x)
        Hero.super.init(self, name, 100, x)
        self.speed, self.color, self.size = 160, sample.accent, 28
    end

    function Hero:describe()
        return 'the hero ' .. Hero.super.describe(self)
    end

    local Bat = haylen.class('Bat', Unit, Flyer)

    function Bat:init(name, x)
        Bat.super.init(self, name, 20, x)
        self.color, self.size = '#FFC9A0FF', 18
    end

    local Dragon = haylen.class('Dragon', Unit, Walker, Flyer)

    function Dragon:init(name, x)
        Dragon.super.init(self, name, 500, x)
        self.speed, self.color, self.size = 60, sample.red, 44
    end

    return {Unit = Unit, Walker = Walker, Flyer = Flyer, Hero = Hero, Bat = Bat, Dragon = Dragon}
end

function Classes:enter()
    self.journal = Journal()
    self.classes = defineClasses(self.journal)
    self.units = {}
    self.time = 0
    self:frame({
        hint = 'Spawn units and run the checks. Walkers pace the ground, flyers bob in the air, and the dragon does both.',
        code = kCode,
        controls = {
            ui.button{id = 'hero', text = 'Spawn a hero', variant = 'primary', onClick = function() self:spawn('Hero', 'Ana') end},
            ui.button{id = 'bat', text = 'Spawn a bat', onClick = function() self:spawn('Bat', 'Bat') end},
            ui.button{id = 'dragon', text = 'Spawn a dragon', onClick = function() self:spawn('Dragon', 'Ember') end},
            ui.button{id = 'check', text = 'Run the checks', onClick = function() self:check() end},
        },
        focus = 'hero',
    })
    self:spawn('Hero', 'Ana')
    self:spawn('Bat', 'Bat')
    self:spawn('Dragon', 'Ember')
end

function Classes:spawn(className, name)
    local unit = self.classes[className](name, 60 + #self.units * 110)
    table.insert(self.units, unit)
    self.journal:add(tostring(unit):match('^[^:]+') .. ' spawned: ' .. unit:describe())
end

function Classes:check()
    local classes, journal = self.classes, self.journal
    local ana = classes.Hero('Ana', 0)
    journal:add('Check "Hero(Ana):is(Unit)" ' .. tostring(ana:is(classes.Unit)) .. ', "is(Walker)" ' .. tostring(ana:is(classes.Walker)) .. ', "is(Flyer)" ' .. tostring(ana:is(classes.Flyer)), sample.warm)
    journal:add('Check "Dragon:is(Flyer)" ' .. tostring(classes.Dragon:is(classes.Flyer)) .. ', "Bat:is(Walker)" ' .. tostring(classes.Bat:is(classes.Walker)), sample.warm)
    journal:add('Check "Hero(Ana) == Hero(Ana)" ' .. tostring(ana == classes.Hero('Ana', 50)) .. ' through the "__eq" that "Hero" copied from "Unit"', sample.warm)
    journal:add('Check "tostring(Hero)" ' .. tostring(classes.Hero) .. ', "Hero.super == Unit" ' .. tostring(classes.Hero.super == classes.Unit) .. ', "Hero.name" ' .. classes.Hero.name, sample.warm)
end

function Classes:update(dt)
    Classes.super.update(self, dt)
    self.time = self.time + dt
    for _, unit in ipairs(self.units) do
        if unit.walk and self.area then
            unit:walk(dt, self.area.width)
        end
        if unit.fly then
            unit:fly(self.time)
        end
    end
    self:status(#self.units .. ' units')
end

function Classes:draw(area)
    local ground = area.height * 0.42
    graphics2d.drawLine(0, ground, area.width, ground, 3, sample.line)
    for _, unit in ipairs(self.units) do
        local y = ground - unit.size - unit.lift
        graphics2d.drawCircle(unit.x, y, unit.size, unit.color)
        graphics2d.drawText(nil, unit.name, unit.x, y - unit.size - 18, {size = 22, color = sample.ink, anchor = {0.5, 0.5}})
    end
    self.journal:draw(24, ground + 20, area.height - ground - 40, 26)
end

return Classes
