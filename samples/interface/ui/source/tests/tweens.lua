-- UI tweens: `document:transform` returns the transform of a node, whose offset, scale, opacity and tint `haylen.tween` animates natively without running Lua every frame.
local haylen = require('haylen')
local m = require('haylen.math')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local sample = require('sample')

local Tweens = haylen.class('Tweens', sample.Test)

Tweens.hints = 'Each button plays a tween on a transform. Transforms only change how a node looks and where it takes input, so the layout around it never moves.'
Tweens.focus = 'replay'

local kTints = {'#FFFFFFFF', '#FFFF8A80', '#FF80D8FF', '#FFFFE57F'}

function Tweens:init(entry)
    Tweens.super.init(self, entry)
    self.tint = 1
    self.shown = true
end

function Tweens:content()
    local cards = {}
    for index = 1, 5 do
        cards[index] = ui.card{id = 'card-' .. index, width = 190, gap = 8, align = 'start',
            ui.icon{image = 'icons/' .. ({'sword', 'shield', 'potion', 'gem', 'coin'})[index] .. '.png', size = 64},
            ui.label{text = 'Item ' .. index},
        }
    end
    return sample.columns{
        ui.column{width = 460, gap = 16,
            ui.button{id = 'replay', text = 'Replay the entrance', variant = 'primary', align = 'stretch', onClick = function() self:entrance() end},
            ui.button{text = 'Shake the panel', align = 'stretch', onClick = function() self:shake() end},
            ui.button{text = 'Fade the panel', align = 'stretch', onClick = function() self:fade() end},
            ui.button{text = 'Tint the panel', align = 'stretch', onClick = function() self:cycleTint() end},
            ui.button{id = 'pulse', text = 'Pulsing forever', align = 'stretch'},
        },
        ui.column{grow = 1, gap = 24,
            ui.label{id = 'title', text = 'Treasure found!', font = 'title'},
            ui.row{gap = 16, children = cards},
            ui.panel{id = 'panel', width = 820, gap = 12,
                ui.label{text = 'A panel with its children', font = 'heading'},
                ui.label{text = 'Transforms reach every child of the node, and a scroll or a popup keeps its own look.', color = 'textMuted'},
                ui.progress{value = 0.65, tone = 'success', text = 'Health'},
            },
        },
    }
end

-- The title drops in and the cards rise one after another with a stagger.
function Tweens:entrance()
    local title = self.document:transform('title')
    tween.fromTo(title, 0.6, {offset = m.vec2(0, -80), opacity = 0}, {offset = m.vec2(0, 0), opacity = 1}, {ease = 'backOut', owner = self})
    local cards = {}
    for index = 1, 5 do
        cards[index] = self.document:transform('card-' .. index)
        cards[index].opacity = 0
    end
    tween.stagger(cards, 0.08, function(card)
        return tween.fromTo(card, 0.4, {offset = m.vec2(0, 60), opacity = 0, scale = m.vec2(0.8, 0.8)}, {offset = m.vec2(0, 0), opacity = 1, scale = m.vec2(1, 1)}, {ease = 'quadOut'})
    end, {owner = self, delay = 0.2})
    self:setStatus('Entrance with a stagger of five cards')
end

function Tweens:shake()
    tween.shake(self.document:transform('panel'), 0.5, 14, {field = 'offset', vibrato = 16, owner = self})
    self:setStatus('Shake on the offset')
end

function Tweens:fade()
    self.shown = not self.shown
    tween.to(self.document:transform('panel'), 0.4, {opacity = self.shown and 1 or 0.2}, {ease = 'sineInOut', owner = self, overwrite = true})
    self:setStatus(self.shown and 'Panel faded in' or 'Panel faded out')
end

function Tweens:cycleTint()
    self.tint = self.tint % #kTints + 1
    tween.to(self.document:transform('panel'), 0.4, {tint = kTints[self.tint]}, {owner = self})
    self:setStatus('Tint ' .. kTints[self.tint])
end

function Tweens:started()
    tween.to(self.document:transform('pulse'), 0.6, {scale = m.vec2(1.06, 1.06)}, {repeatCount = -1, loopMode = 'yoyo', ease = 'sineInOut', owner = self})
    self:entrance()
end

return Tweens
