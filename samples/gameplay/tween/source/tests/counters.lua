-- Counters and typewriter: a score that counts in whole numbers next to a plain number, and dialog lines revealed one character at a time.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local sample = require('sample')

local Counters = haylen.class('Counters', sample.Test)

local kLines = {
    'The tide is coming in. Gather the wood before dark.',
    'Every tween reads its start value when it starts.',
    'A text tween reveals the new line over the old one.',
    'Typewriters, subtitles and tutorial hints all work this way.',
}
local kCode = [[
tween.to(hud, 1.5, {score = hud.score + 250, plain = hud.plain + 250}, {integers = {'score'}, ease = 'quadOut'})
tween.to(dialog, 2.5, {text = 'The tide is coming in. Gather the wood before dark.'})]]

function Counters:enter()
    self.hud = {score = 0, plain = 0}
    self.dialog = {text = ''}
    self.line = 0
    self:frame({
        hint = 'Add points with the button or R, and show the next line with the second button.',
        code = kCode,
        controls = {
            ui.button{id = 'points', text = 'Add 250 points', variant = 'primary', onClick = function() self:addPoints() end},
            ui.button{id = 'line', text = 'Next line', onClick = function() self:nextLine() end},
        },
        focus = 'points',
    })
    self:addPoints()
    self:nextLine()
end

function Counters:addPoints()
    tween.to(self.hud, 1.5, {score = math.floor(self.hud.score) + 250, plain = math.floor(self.hud.plain + 0.5) + 250}, {owner = self, integers = {'score'}, ease = 'quadOut', overwrite = true})
end

function Counters:nextLine()
    self.line = self.line % #kLines + 1
    self.dialog.text = ''
    tween.to(self.dialog, 2.5, {text = kLines[self.line]}, {owner = self, overwrite = true})
end

function Counters:update(dt)
    Counters.super.update(self, dt)
    if input.pressed('replay') then
        self:addPoints()
    end
    self:status(string.format('score %d   plain %.3f   %d characters shown', self.hud.score, self.hud.plain, utf8.len(self.dialog.text)))
end

function Counters:draw(area)
    graphics2d.drawText(nil, 'integers', 80, 40, {size = 26, color = sample.muted})
    graphics2d.drawText(nil, string.format('%d', self.hud.score), 80, 80, {size = 110, color = sample.warm})
    graphics2d.drawText(nil, 'a plain number', area.width / 2, 40, {size = 26, color = sample.muted})
    graphics2d.drawText(nil, string.format('%.3f', self.hud.plain), area.width / 2, 80, {size = 110, color = sample.ink})

    local box = {60, area.height - 250, area.width - 120, 190}
    graphics2d.drawRect(box, '#FF232739')
    graphics2d.drawRectOutline(box, 3, '#FF3A4058')
    graphics2d.drawText(nil, self.dialog.text, 100, area.height - 220, {size = 40, color = sample.ink, maxWidth = area.width - 200, layer = 1})
end

return Counters
