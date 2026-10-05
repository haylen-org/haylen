-- Nested and several fields: one card tweened through paths into nested tables and vector components, another through five fields in one tween.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TweenTest = require('categories.tween.tween-test')

local Fields = haylen.class('Fields', TweenTest)

local kCode = [[
local a = {position = m.vec2(80, 60), size = {width = 220, height = 120}, style = {color = m.color('#FF4C7DFF')}}
tween.to(a, 1.4, {['position.x'] = 760, ['size.height'] = 220, ['style.color.a'] = 0.35}, {loopMode = 'yoyo', repeatCount = -1})
tween.to(b, 1.4, {x = 760, y = 420, width = 320, height = 80, color = '#FFF2B23A'}, {loopMode = 'yoyo', repeatCount = -1})]]

function Fields:enter()
    self.a = {position = m.vec2(80, 60), size = {width = 220, height = 120}, style = {color = m.color('#FF4C7DFF')}}
    self.b = {x = 80, y = 360, width = 220, height = 120, color = m.color('#FF3DBE7A')}
    self:frame({
        hint = 'Replay with the button, R or the X button.',
        code = kCode,
        controls = {ui.button{id = 'replay', text = 'Replay', variant = 'primary', onClick = function() self:play() end}},
        focus = 'replay',
    })
    self:play()
end

function Fields:play()
    tween.killTarget(self.a)
    tween.killTarget(self.b)
    self.a.position, self.a.size.width, self.a.size.height, self.a.style.color = m.vec2(80, 60), 220, 120, m.color('#FF4C7DFF')
    self.b.x, self.b.y, self.b.width, self.b.height, self.b.color = 80, 360, 220, 120, m.color('#FF3DBE7A')

    local options = {owner = self, ease = 'cubicInOut', loopMode = 'yoyo', repeatCount = -1, repeatDelay = 0.3}
    tween.to(self.a, 1.4, {['position.x'] = 760, ['size.height'] = 220, ['style.color.a'] = 0.35}, options)
    tween.to(self.b, 1.4, {x = 760, y = 420, width = 320, height = 80, color = '#FFF2B23A'}, options)
end

function Fields:update(dt)
    Fields.super.update(self, dt)
    if input.pressed('replay') then
        self:play()
    end
    self:status(string.format('Fields "a.position.x" %.0f, "a.size.height" %.0f, "a.style.color.a" %.2f, "b" at %.0f, %.0f, %.0f by %.0f', self.a.position.x, self.a.size.height, self.a.style.color.a, self.b.x, self.b.y, self.b.width, self.b.height))
end

function Fields:draw(area)
    local a, b = self.a, self.b
    graphics2d.drawText(nil, 'Nested paths', 80, 30, {size = 26, color = Test.muted})
    graphics2d.drawRect({a.position.x, a.position.y, a.size.width, a.size.height}, a.style.color)
    graphics2d.drawText(nil, 'Five fields in one tween', 80, 330, {size = 26, color = Test.muted})
    graphics2d.drawRect({b.x, b.y, b.width, b.height}, b.color)
end

return Fields
