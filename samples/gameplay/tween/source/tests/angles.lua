-- Shortest-path angles: two needles turn to the same heading, one listed in the angles option that takes the short way around, one as a plain number that unwinds the long way.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local sample = require('sample')

local Angles = haylen.class('Angles', sample.Test)

local kHeadings = {350, 10, 200, 170, 300, 60}
local kCode = [[
local ship = {heading = math.rad(350)}
tween.to(ship, 1.2, {heading = math.rad(10)}, {angles = {'heading'}})  -- turns 20 degrees
tween.to(plain, 1.2, {heading = math.rad(10)})  -- turns 340 degrees back]]

function Angles:enter()
    self.short = {heading = math.rad(kHeadings[1])}
    self.plain = {heading = math.rad(kHeadings[1])}
    self.step = 1
    self:frame({
        hint = 'Turn to the next heading with the button, R or the west button.',
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
    local options = {owner = self, ease = 'cubic_in_out', overwrite = true}
    tween.to(self.plain, 1.2, {heading = target}, options)
    options.angles = {'heading'}
    tween.to(self.short, 1.2, {heading = target}, options)
    self:set('target', {text = 'Heading ' .. kHeadings[self.step] .. ' degrees'})
end

function Angles:update(dt)
    Angles.super.update(self, dt)
    if input.pressed('replay') then
        self:turn()
    end
    self:status(string.format('short %.0f degrees   plain %.0f degrees', m.degrees(self.short.heading) % 360, m.degrees(self.plain.heading) % 360))
end

function Angles:drawDial(x, y, radius, heading, label, color)
    graphics2d.drawRing(x, y, radius, 4, sample.line)
    for tick = 0, 11 do
        local angle = tick * m.tau / 12
        graphics2d.drawLine(x + math.cos(angle) * (radius - 16), y + math.sin(angle) * (radius - 16), x + math.cos(angle) * radius, y + math.sin(angle) * radius, 3, sample.line)
    end
    local target = math.rad(kHeadings[self.step])
    graphics2d.drawCircle(x + math.cos(target) * (radius + 20), y + math.sin(target) * (radius + 20), 8, sample.warm)
    graphics2d.drawLine(x, y, x + math.cos(heading) * (radius - 24), y + math.sin(heading) * (radius - 24), 10, color, {layer = 1})
    graphics2d.drawCircle(x, y, 14, color, {layer = 2})
    graphics2d.drawText(nil, label, x, y + radius + 50, {size = 30, color = sample.ink, anchor = {0.5, 0.5}})
end

function Angles:draw(area)
    local radius = math.min(area.width / 5, area.height / 2 - 90)
    self:drawDial(area.width * 0.28, area.height / 2 - 30, radius, self.short.heading, 'angles = {\'heading\'}', sample.green)
    self:drawDial(area.width * 0.72, area.height / 2 - 30, radius, self.plain.heading, 'a plain number', sample.red)
end

return Angles
