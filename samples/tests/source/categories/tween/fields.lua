-- Nested and several fields: one card tweened through paths into nested tables and vector components, another through five fields in one tween, each between the dashed outlines of where it starts and where it ends, with the live fields on the panel.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TweenTest = require('categories.tween.tween-test')

local Fields = haylen.class('Fields', TweenTest)

local kView = {1180, 600}
local kStartA, kEndA = {80, 90, 220, 120}, {760, 90, 220, 220}
local kStartB, kEndB = {80, 400, 220, 120}, {760, 460, 320, 80}
local kCode = [[
local a = {position = m.vec2(80, 90), size = {width = 220, height = 120}, style = {color = m.color('#FF4C7DFF')}}
tween.to(a, 1.4, {['position.x'] = 760, ['size.height'] = 220, ['style.color.a'] = 0.35}, {loopMode = 'yoyo', repeatCount = -1})
tween.to(b, 1.4, {x = 760, y = 460, width = 320, height = 80, color = '#FFF2B23A'}, {loopMode = 'yoyo', repeatCount = -1})]]

function Fields:enter()
    self.gem, self.coin = TweenTest.icon('gem'), TweenTest.icon('coin')
    self:frame({
        hint = 'Each card travels between the dashed outlines of where it starts and where it ends. Replay with the button, R or the X button.',
        code = kCode,
        controls = {
            ui.button{id = 'replay', text = 'Replay', variant = 'primary', onClick = function() self:play() end},
            ui.sectionTitle{text = 'Paths into card "a"'},
            ui.label{id = 'fieldsA', text = '', font = 'monospace'},
            ui.sectionTitle{text = 'Fields of card "b"'},
            ui.label{id = 'fieldsB', text = '', font = 'monospace'},
        },
        view = kView,
        focus = 'replay',
    })
    self.camera:snapTo(kView[1] / 2, kView[2] / 2)
    self:play()
end

function Fields:play()
    if self.a then
        tween.killTarget(self.a)
        tween.killTarget(self.b)
    end
    self.a = {position = m.vec2(kStartA[1], kStartA[2]), size = {width = kStartA[3], height = kStartA[4]}, style = {color = m.color('#FF4C7DFF')}}
    self.b = {x = kStartB[1], y = kStartB[2], width = kStartB[3], height = kStartB[4], color = m.color('#FF3DBE7A')}

    local options = {owner = self, ease = 'cubicInOut', loopMode = 'yoyo', repeatCount = -1, repeatDelay = 0.3}
    tween.to(self.a, 1.4, {['position.x'] = kEndA[1], ['size.height'] = kEndA[4], ['style.color.a'] = 0.35}, options)
    tween.to(self.b, 1.4, {x = kEndB[1], y = kEndB[2], width = kEndB[3], height = kEndB[4], color = '#FFF2B23A'}, options)
end

function Fields:update(dt)
    Fields.super.update(self, dt)
    if input.pressed('replay') then
        self:play()
    end
    local a, b = self.a, self.b
    self:set('fieldsA', {text = string.format('position.x    %4.0f\nsize.height   %4.0f\nstyle.color.a %4.2f', a.position.x, a.size.height, a.style.color.a)})
    self:set('fieldsB', {text = string.format('x       %4.0f\ny       %4.0f\nwidth   %4.0f\nheight  %4.0f\ncolor   %s', b.x, b.y, b.width, b.height, b.color:toHex())})
    self:status(string.format('Card "a" stands at %.0f with a height of %.0f, and card "b" at %.0f, %.0f with a size of %.0f by %.0f.', a.position.x, a.size.height, b.x, b.y, b.width, b.height))
end

-- The outline of a place a card passes, dashed so it never reads as a card.
local function ghost(rect, color)
    local x, y, width, height = rect[1], rect[2], rect[3], rect[4]
    graphics2d.drawStroke({x, y, x + width, y, x + width, y + height, x, y + height}, {width = 3, color = color, closed = true, join = 'round', dashes = {12, 9}})
end

-- A card with rounded corners, a dark outline and its icon in the middle, as opaque as its color.
local function card(rect, color, icon)
    graphics2d.drawShape(rect, {radius = 18, color = color, borderWidth = 4, borderColor = m.color('#FF1E1B33'):withAlpha(color.a), layer = 1})
    local size = math.min(rect[3], rect[4]) * 0.6
    graphics2d.draw(icon, rect[1] + rect[3] / 2, rect[2] + rect[4] / 2, {width = size, height = size, color = m.color(1, 1, 1, color.a), layer = 2})
end

function Fields:draw(area)
    local a, b = self.a, self.b
    graphics2d.drawText(nil, 'Nested paths', 80, 20, {size = 28, color = Test.ink})
    graphics2d.drawText(nil, 'One tween writes three fields inside nested tables and a vector.', 80, 56, {size = 19, color = Test.muted})
    ghost(kStartA, '#FF5A6380')
    ghost(kEndA, '#FF8FB0FF')
    card({a.position.x, a.position.y, a.size.width, a.size.height}, a.style.color, self.gem)

    graphics2d.drawText(nil, 'Five fields in one tween', 80, 330, {size = 28, color = Test.ink})
    graphics2d.drawText(nil, 'The position, the size and the color travel together.', 80, 366, {size = 19, color = Test.muted})
    ghost(kStartB, '#FF5A6380')
    ghost(kEndB, '#FFF2B23A')
    card({b.x, b.y, b.width, b.height}, b.color, self.coin)
end

return Fields
