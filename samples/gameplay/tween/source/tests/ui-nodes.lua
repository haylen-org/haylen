-- UI node tweens: a reward card whose nodes slide, pulse, tint, fade and shake through their transforms, which the engine animates without running Lua each frame.
local haylen = require('haylen')
local m = require('haylen.math')
local tween = require('haylen.tween')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local sample = require('sample')

local UiNodes = haylen.class('UiNodes', sample.Test)

local kCode = [[
local card = document:transform('card')
tween.fromTo(card, 0.7, {offset = m.vec2(0, -400), opacity = 0}, {offset = m.vec2(0, 0), opacity = 1}, {ease = 'backOut'})
tween.to(document:transform('claim'), 0.4, {scale = m.vec2(1.12, 1.12)}, {loopMode = 'yoyo', repeatCount = -1})
tween.to(document:transform('title'), 0.5, {tint = '#FFFFD166'}, {loopMode = 'yoyo', repeatCount = 3})]]

function UiNodes:enter()
    self:frame({
        hint = 'Each button starts a tween on a node of the card. Input areas follow the offset and keep their layout size.',
        code = kCode,
        controls = {
            ui.button{id = 'slide', text = 'Slide in', variant = 'primary', onClick = function() self:slideIn() end},
            ui.button{id = 'tint', text = 'Tint the title', onClick = function() self:tintTitle() end},
            ui.button{id = 'fade', text = 'Fade the card', onClick = function() self:fade() end},
            ui.button{id = 'shake', text = 'Shake the reward', onClick = function() self:shake() end},
        },
        focus = 'slide',
    })
    self.card = ui.mount(ui.card{id = 'card', anchor = 'topLeft', width = 560, gap = 16, onCancel = sample.back,
        ui.label{id = 'title', text = 'Treasure found', font = 'title'},
        ui.label{id = 'reward', text = '+250 coins', font = 'heading', color = 'accentText'},
        ui.progress{id = 'bar', value = 0.6, tone = 'success', text = 'Level 3'},
        ui.row{gap = 12,
            ui.button{id = 'claim', text = 'Claim', variant = 'primary', grow = 1},
            ui.button{id = 'later', text = 'Later', grow = 1},
        },
    }, {owner = self, layer = 1})
    tween.to(self.card:transform('claim'), 0.4, {scale = m.vec2(1.12, 1.12)}, {owner = self, loopMode = 'yoyo', repeatCount = -1, ease = 'sineInOut'})
    self:slideIn()
end

-- Places the card in the stage, which the safe area and the frame decide.
function UiNodes:resize(area)
    local stage, safe = self.document:bounds('stage'), viewport.safeRect()
    self.card:set('card', {margin = {stage.y - safe.y + 60, 0, 0, stage.x - safe.x + (stage.width - 560) / 2}})
end

function UiNodes:slideIn()
    tween.fromTo(self.card:transform('card'), 0.7, {offset = m.vec2(0, -400), opacity = 0}, {offset = m.vec2(0, 0), opacity = 1}, {owner = self, ease = 'backOut', overwrite = true})
end

function UiNodes:tintTitle()
    tween.to(self.card:transform('title'), 0.5, {tint = '#FFFFD166'}, {owner = self, loopMode = 'yoyo', repeatCount = 3, overwrite = true})
end

function UiNodes:fade()
    tween.to(self.card:transform('card'), 0.5, {opacity = 0.15}, {owner = self, loopMode = 'yoyo', repeatCount = 1, overwrite = true})
end

function UiNodes:shake()
    tween.shake(self.card:transform('reward'), 0.5, 16, {owner = self, field = 'offset', vibrato = 18})
end

function UiNodes:update(dt)
    UiNodes.super.update(self, dt)
    local card = self.card:transform('card')
    self:status(string.format('card offset %.0f, %.0f   opacity %.2f   claim scale %.2f', card.offset.x, card.offset.y, card.opacity, self.card:transform('claim').scale.x))
end

return UiNodes
