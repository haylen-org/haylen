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
local Art = require('categories.algorithms.art')
local picture = require('categories.algorithms.picture')

local Influence = haylen.class('Influence', AlgorithmTest)

Influence.code = [[
local blue = ai.newInfluenceMap({columns = 64, rows = 34, cellSize = 23, x = -736, y = -391})
blue:stamp(unit.x, unit.y, 1, 200, 'linear')  -- Every unit stamps its strength.
blue:propagate(0.35, 0.5)  -- Spreads it with decay.
balance:add(blue)
balance:add(red, -1)  -- Blue above zero, red below.
local x, y = red:findLowest(unit.x, unit.y, 150)  -- The safest cell nearby.]]

local kColumns, kRows, kCell = 64, 34, 23
local kOrigin = {-kColumns * kCell / 2, -kRows * kCell / 2}
local kSpeed = 70
-- Units keep this far inside the map on its sides and its bottom, and farther from its top, which their heads reach toward, so they never touch its frame.
local kMargin = 70
local kTopMargin = 130
-- Influence below this counts as none, so a unit only chases or flees a real threat, and the faint rim of an area fades out instead of stepping from cell to cell.
local kFaint = 0.05
-- With no threat in reach, a unit marches toward the front, this far from the middle on its own side.
local kFront = 140
local kTeams = {blue = {color = '#FF42A5F5', sign = 1}, red = {color = '#FFEF5350', sign = -1}}

function Influence:enter()
    self:frame{
        hint = 'Tap or click to drop a unit of the selected army. Blue areas belong to the blue army and red ones to the red army, and units retreat where the enemy is weakest. R or the X button raises new armies.',
        code = Influence.code,
        controls = {
            ui.formField{label = 'Army of the units you drop', ui.segmentedControl{id = 'team', items = {{id = 'blue', text = 'Blue'}, {id = 'red', text = 'Red'}}, selected = 'blue', onChange = function(event) self.team = event.value end}},
            ui.formField{label = 'Spread decay', ui.slider{id = 'decay', value = 0.35, min = 0.1, max = 1, step = 0.05, showValue = true, onChange = function(event) self.decay = event.value end}},
            ui.button{id = 'reset', text = 'New armies', onClick = function() self:build() end},
        },
        focus = 'team',
    }
    self.team, self.decay = 'blue', 0.35
    self.random = m.random(107)
    self.art = Art()
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
        self.units[index] = {team = team, x = x, y = self.random:range(kOrigin[2] + kTopMargin, -kOrigin[2] - kMargin), strength = 0.7 + (index % 7) * 0.1, facing = team == 'blue' and 1 or -1}
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
        elseif enemy:sample(unit.x, unit.y) > kFaint then
            tx, ty = enemy:findLowest(unit.x, unit.y, 150)
        end
        -- A unit with no enemy worth chasing in reach marches toward the front on the line it stands on.
        if tx == nil or (advantage > -0.1 and enemy:sample(tx, ty) < kFaint) then
            tx, ty = -sign * kFront, unit.y
        end
        unit.target = {tx, ty}
    end
    self:paint(self.balance:values())
end

-- Blue shades where blue is stronger and red shades where red is, blending smoothly between the cells.
function Influence:paint(values)
    self.picture = picture.cells(kColumns, kRows, function(column, row)
        local value = m.clamp(Influence.blurred(values, column, row) * 0.6, -1, 1)
        local level = math.max(0, math.abs(value) - kFaint) / (1 - kFaint)
        if value >= 0 then
            return m.color(0.10, 0.14 + level * 0.25, 0.20 + level * 0.6):toHex()
        end
        return m.color(0.14 + level * 0.65, 0.12, 0.16):toHex()
    end, true)
end

-- The average of a cell and the eight around it, which softens the rim of every area.
function Influence.blurred(values, column, row)
    local sum, count = 0, 0
    for dy = -1, 1 do
        for dx = -1, 1 do
            local x, y = column + dx, row + dy
            if x >= 0 and y >= 0 and x < kColumns and y < kRows then
                sum, count = sum + values[y * kColumns + x + 1], count + 1
            end
        end
    end
    return sum / count
end

function Influence:update(dt)
    Influence.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    if self.pointer.pressed then
        self.units[#self.units + 1] = {team = self.team, x = self.pointer.worldX, y = self.pointer.worldY, strength = 1, facing = 1}
    end
    for _, unit in ipairs(self.units) do
        if unit.target then
            local dx, dy = unit.target[1] - unit.x, unit.target[2] - unit.y
            local distance = math.sqrt(dx * dx + dy * dy)
            if distance > 30 then
                unit.x, unit.y = unit.x + dx / distance * kSpeed * dt, unit.y + dy / distance * kSpeed * dt
                unit.facing = dx < 0 and -1 or 1
            end
        end
        unit.x = m.clamp(unit.x, kOrigin[1] + kMargin, -kOrigin[1] - kMargin)
        unit.y = m.clamp(unit.y, kOrigin[2] + kTopMargin, -kOrigin[2] - kMargin)
    end
    self:status(string.format('%d units stamp their influence on %d cells, spread in %s.', #self.units, kColumns * kRows, AlgorithmTest.milliseconds(self:timing('influence'))))
end

function Influence:draw(area)
    graphics2d.drawShape({kOrigin[1] - 6, kOrigin[2] - 6, kColumns * kCell + 12, kRows * kCell + 12}, {radius = 10, color = '#FF151A24', borderWidth = 3, borderColor = '#FF3A4560', layer = -1})
    graphics2d.draw(self.picture, kOrigin[1], kOrigin[2], {pivotX = 0, pivotY = 0, width = kColumns * kCell, height = kRows * kCell})
    for _, unit in ipairs(self.units) do
        local depth = math.floor(unit.y + 500)
        Art.shadow(unit.x, unit.y, 36, depth - 1)
        self.art:character(unit.x, unit.y + 4, 60 + unit.strength * 24, unit.team, depth, unit.facing < 0)
    end
end

return Influence
