-- Counters and typewriter: a score that counts in whole numbers next to a plain number, and the lines of a dialogue revealed one character at a time in a wooden panel with the portrait of who speaks.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TweenTest = require('categories.tween.tween-test')

local Counters = haylen.class('Counters', TweenTest)

local kLines = {
    'The tide is coming in. Gather the wood before dark.',
    'Every tween reads its start value when it starts.',
    'A text tween reveals the new line over the old one.',
    'Typewriters, subtitles and tutorial hints all work this way.',
}
local kInk = '#FF3B2A1E'
local kCode = [[
tween.to(hud, 1.5, {score = hud.score + 250, plain = hud.plain + 250}, {integers = {'score'}, ease = 'quadOut'})
tween.to(dialog, 2.5, {text = 'The tide is coming in. Gather the wood before dark.'})]]

function Counters:enter()
    self.hud = {score = 0, plain = 0}
    self.dialog = {text = ''}
    self.lineIndex = 0
    self.coin = TweenTest.icon('coin')
    self.portrait = assets.texture('interface/images/avatar-ana.png', {filter = 'linear'})
    self.panel = graphics2d.newNineSlice(assets.texture('nine-slice/parchment.png', {filter = 'linear'}), {borders = {50, 50, 50, 50}})
    self:frame({
        hint = 'Add points with the button, R or the X button, and show the next line with the second button.',
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
    self.lineIndex = self.lineIndex % #kLines + 1
    self.dialog.text = ''
    tween.to(self.dialog, 2.5, {text = kLines[self.lineIndex]}, {owner = self, overwrite = true})
end

function Counters:update(dt)
    Counters.super.update(self, dt)
    if input.pressed('replay') then
        self:addPoints()
    end
    self:status(string.format('The score shows %d, the plain number %.3f, and the dialogue %d of %d characters.', self.hud.score, self.hud.plain, utf8.len(self.dialog.text), utf8.len(kLines[self.lineIndex])))
end

-- A card with the title of a counter and its number, the whole one beside a coin.
function Counters:drawCounter(rect, title, caption, text, color, icon)
    graphics2d.drawShape(rect, {radius = 20, color = '#FF1F2535', borderWidth = 3, borderColor = '#FF39415A'})
    local x, y = rect[1] + 32, rect[2] + 24
    graphics2d.drawText(nil, title, x, y, {size = 26, color = Test.ink})
    graphics2d.drawText(nil, caption, x, y + 34, {size = 18, color = Test.muted})
    if icon then
        graphics2d.draw(icon, x + 40, y + 124, {width = 80, height = 80})
        x = x + 100
    end
    graphics2d.drawText(nil, text, x, y + 124, {size = 96, color = color, anchor = {0, 0.5}})
end

function Counters:draw(area)
    local width = math.min(1100, area.width - 80)
    local left = (area.width - width) / 2
    local top = math.max(20, (area.height - 520) / 2)
    local half = (width - 32) / 2
    self:drawCounter({left, top, half, 220}, 'Whole numbers', 'The option "integers" rounds the score.', string.format('%d', self.hud.score), Test.warm, self.coin)
    self:drawCounter({left + half + 32, top, half, 220}, 'A plain number', 'The same tween without it.', string.format('%.3f', self.hud.plain), Test.ink)

    local box = {left, top + 260, width, 240}
    graphics2d.drawNineSlice(self.panel, box)
    local portrait = {box[1] + 40, box[2] + 44, 152, 152}
    graphics2d.pushMask(function()
        graphics2d.drawShape(portrait, {radius = 24})
    end)
    graphics2d.draw(self.portrait, portrait[1] + 76, portrait[2] + 76, {width = 152, height = 152})
    graphics2d.popMask()
    graphics2d.drawShape(portrait, {radius = 24, color = '#00000000', borderWidth = 5, borderColor = '#FF6B4426', layer = 1})
    graphics2d.drawText(nil, 'Ana', box[1] + 224, box[2] + 44, {size = 30, color = '#FF8A3B12', bold = true})
    graphics2d.drawText(nil, self.dialog.text, box[1] + 224, box[2] + 88, {size = 32, color = kInk, maxWidth = box[3] - 290, layer = 1})
    if utf8.len(self.dialog.text) == utf8.len(kLines[self.lineIndex]) then
        local bob = math.abs(math.sin(haylen.elapsed() * 4)) * 8
        local x, y = box[1] + box[3] - 56, box[2] + box[4] - 54 + bob
        graphics2d.drawPolygon({{x - 14, y - 8}, {x + 14, y - 8}, {x, y + 10}}, kInk, {layer = 1})
    end
end

return Counters
