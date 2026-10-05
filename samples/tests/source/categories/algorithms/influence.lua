-- Influence maps of two armies: every unit stamps its strength, the maps spread it with decay, and each unit reads the balance under it to advance where its side is stronger and fall back to the safest cell nearby.
local ai = require('haylen.ai')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local profiler = require('haylen.debug')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local picture = require('categories.algorithms.picture')

local Influence = haylen.class('Influence', AlgorithmTest)

local kColumns, kRows, kCell = 64, 34, 23
local kOrigin = {-kColumns * kCell / 2, -kRows * kCell / 2}
local kSpeed = 70
local kTeams = {blue = {color = '#FF42A5F5', sign = 1}, red = {color = '#FFEF5350', sign = -1}}

function Influence:enter()
    self:frame{
        hint = 'Tap or click to drop a unit of the selected army. Blue areas belong to the blue army and red ones to the red army, and units retreat where the enemy is weakest. R or the X button raises new armies.',
        controls = {
            ui.radioGroup{id = 'team', horizontal = true, items = {{id = 'blue', text = 'Blue'}, {id = 'red', text = 'Red'}}, selected = 'blue', onChange = function(event) self.team = event.value end},
            ui.label{text = 'Spread decay', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'decay', value = 0.35, min = 0.1, max = 1, step = 0.05, showValue = true, onChange = function(event) self.decay = event.value end},
            ui.button{id = 'reset', text = 'New armies', onClick = function() self:build() end},
        },
        focus = 'team',
    }
    self.team, self.decay = 'blue', 0.35
    self.random = m.random(107)
    self:build()
    timer.every(0.1, function() self:think() end, {owner = self})
end

function Influence:build()
    local options = {columns = kColumns, rows = kRows, cellSize = kCell, x = kOrigin[1], y = kOrigin[2]}
    self.maps = {blue = ai.newInfluenceMap(options), red = ai.newInfluenceMap(options)}
    self.balance = ai.newInfluenceMap(options)
    self.units = {}
    for index = 1, 30 do
        local team = index % 2 == 0 and 'blue' or 'red'
        local x = (team == 'blue' and -500 or 500) + self.random:range(-150, 150)
        self.units[index] = {team = team, x = x, y = self.random:range(-350, 350), strength = self.random:range(0.6, 1.4)}
    end
    self:think()
end

-- Stamps every unit, spreads both maps, adds them up with opposite signs and moves the units on the result.
function Influence:think()
    profiler.beginScope('influence')
    for _, map in pairs(self.maps) do
        map:fill(0)
    end
    for _, unit in ipairs(self.units) do
        self.maps[unit.team]:stamp(unit.x, unit.y, unit.strength, 200, 'linear')
    end
    for _, map in pairs(self.maps) do
        for _ = 1, 3 do
            map:propagate(self.decay, 0.5)
        end
    end
    self.balance:fill(0)
    self.balance:add(self.maps.blue)
    self.balance:add(self.maps.red, -1)
    profiler.endScope()

    for _, unit in ipairs(self.units) do
        local sign = kTeams[unit.team].sign
        local advantage = self.balance:sample(unit.x, unit.y) * sign
        local enemy = self.maps[unit.team == 'blue' and 'red' or 'blue']
        local tx, ty
        if advantage > -0.1 then
            tx, ty = enemy:findHighest(unit.x, unit.y, 400)
        else
            tx, ty = enemy:findLowest(unit.x, unit.y, 150)
        end
        unit.target = tx and {tx, ty} or nil
    end
    self:paint(self.balance:values())
end

-- Blue shades where blue is stronger and red shades where red is, in a few steps.
function Influence:paint(values)
    self.picture = picture.cells(kColumns, kRows, function(column, row)
        local value = m.clamp(values[row * kColumns + column + 1] * 0.6, -1, 1)
        local level = math.floor(math.abs(value) * 8 + 0.5) / 8
        if value >= 0 then
            return m.color(0.10, 0.14 + level * 0.25, 0.20 + level * 0.6):toHex()
        end
        return m.color(0.14 + level * 0.65, 0.12, 0.16):toHex()
    end)
end

function Influence:update(dt)
    Influence.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    if self.pointer.pressed then
        self.units[#self.units + 1] = {team = self.team, x = self.pointer.worldX, y = self.pointer.worldY, strength = 1}
    end
    for _, unit in ipairs(self.units) do
        if unit.target then
            local dx, dy = unit.target[1] - unit.x, unit.target[2] - unit.y
            local distance = math.sqrt(dx * dx + dy * dy)
            if distance > 30 then
                unit.x, unit.y = unit.x + dx / distance * kSpeed * dt, unit.y + dy / distance * kSpeed * dt
            end
        end
    end
    self:status(string.format('Units %d, cells %d, stamp and spread %.3f ms', #self.units, kColumns * kRows, self:timing('influence')))
end

function Influence:draw(area)
    graphics2d.draw(self.picture, kOrigin[1], kOrigin[2], {pivotX = 0, pivotY = 0, width = kColumns * kCell, height = kRows * kCell})
    for _, unit in ipairs(self.units) do
        graphics2d.drawCircle(unit.x, unit.y, 8 + unit.strength * 6, kTeams[unit.team].color, {layer = 1})
    end
end

return Influence
