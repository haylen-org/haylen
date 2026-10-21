-- Shortest-path angles: two compass needles turn to the same heading, one listed in the `angles` option that takes the short way around, one as a plain number that unwinds the long way, each with the arc it swept.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TweenTest = require('categories.tween.tween-test')

local Angles = haylen.class('Angles', TweenTest)

local kHeadings = {350, 10, 200, 170, 300, 60}
local kCode = [[
local ship = {heading = math.rad(350)}
tween.to(ship, 1.2, {heading = math.rad(10)}, {angles = {'heading'}})  -- Turns 20 degrees.
tween.to(plain, 1.2, {heading = math.rad(10)})  -- Turns 340 degrees back.]]

function Angles:enter()
    self.short = {heading = math.rad(kHeadings[1])}
    self.plain = {heading = math.rad(kHeadings[1])}
    self.step = 1
    self:frame({
        hint = 'Turn to the next heading with the button, R or the X button. The colored arc is the way each needle turned.',
        code = kCode,
        controls = {
            ui.button{id = 'turn', text = 'Next heading', variant = 'primary', onClick = function() self:turn() end},
            ui.label{id = 'target', text = ''},
        },
        focus = 'turn',
    })
    self:turn()
end

function Angles:turn()
    self.step = self.step % #kHeadings + 1
    local target = math.rad(kHeadings[self.step])
    self.shortStart, self.plainStart = self.short.heading, self.plain.heading
    local options = {owner = self, ease = 'cubicInOut', overwrite = true}
    tween.to(self.plain, 1.2, {heading = target}, options)
    options.angles = {'heading'}
    tween.to(self.short, 1.2, {heading = target}, options)
    self:set('target', {text = 'Both needles turn to ' .. kHeadings[self.step] .. ' degrees.'})
end

function Angles:update(dt)
    Angles.super.update(self, dt)
    if input.pressed('replay') then
        self:turn()
    end
    self:status(string.format('The short way turned %.0f degrees and the plain number %.0f degrees.', math.abs(m.degrees(self.short.heading - self.shortStart)), math.abs(m.degrees(self.plain.heading - self.plainStart))))
end

-- A compass with the arc its needle swept from `start` to `heading`, a mark on the rim at the target and the needle.
function Angles:drawDial(x, y, radius, start, heading, title, color)
    graphics2d.drawShape({x - radius, y - radius, radius * 2, radius * 2}, {kind = 'ellipse', color = '#FF1F2535', borderWidth = 6, borderColor = '#FF39415A'})
    for tick = 0, 23 do
        local angle = tick * m.tau / 24
        local inner = radius - (tick % 6 == 0 and 26 or 16)
        graphics2d.drawLine(x + math.cos(angle) * inner, y + math.sin(angle) * inner, x + math.cos(angle) * (radius - 8), y + math.sin(angle) * (radius - 8), tick % 6 == 0 and 4 or 2, TweenTest.tick)
    end
    for quarter = 0, 3 do
        local angle = quarter * m.tau / 4
        graphics2d.drawText(nil, tostring(quarter * 90), x + math.cos(angle) * (radius - 48), y + math.sin(angle) * (radius - 48), {size = 18, color = Test.muted, anchor = {0.5, 0.5}})
    end

    local low, high = math.min(start, heading), math.max(start, heading)
    if high - low > 0.01 then
        graphics2d.drawArc(x, y, radius - 30, 12, low, high, m.color(color):withAlpha(0.45))
    end
    local target = math.rad(kHeadings[self.step])
    local rim, inward = radius + 4, radius - 18
    graphics2d.drawPolygon({{x + math.cos(target) * inward, y + math.sin(target) * inward}, {x + math.cos(target + 0.12) * rim, y + math.sin(target + 0.12) * rim}, {x + math.cos(target - 0.12) * rim, y + math.sin(target - 0.12) * rim}}, Test.warm, {layer = 1})

    local tip, side = radius - 22, 14
    local cos, sin = math.cos(heading), math.sin(heading)
    graphics2d.drawPolygon({{x + cos * tip, y + sin * tip}, {x - sin * side, y + cos * side}, {x - cos * 26, y - sin * 26}, {x + sin * side, y - cos * side}}, color, {layer = 2})
    graphics2d.drawShape({x - 14, y - 14, 28, 28}, {kind = 'ellipse', color = '#FFE8EAF2', borderWidth = 4, borderColor = '#FF1E1B33', layer = 3})
    graphics2d.drawText(nil, title, x, y + radius + 24, {size = 30, color = Test.ink, anchor = {0.5, 0}})
end

function Angles:draw(area)
    local radius = math.min(area.width / 5, area.height / 2 - 70)
    local y = area.height / 2 - 30
    self:drawDial(area.width * 0.28, y, radius, self.shortStart, self.short.heading, 'The short way', Test.green)
    self:drawDial(area.width * 0.72, y, radius, self.plainStart, self.plain.heading, 'A plain number', Test.red)
end

return Angles
