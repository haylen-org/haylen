-- Nested timelines: three child timelines inside a parent that plays them in sequence and in parallel and yoyos as a whole, with the progress of each one.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TweenTest = require('categories.tween.tween-test')

local Nested = haylen.class('Nested', TweenTest)

local kCode = [[
local square = tween.timeline():append(tween.to(a, 0.4, {x = 300})):append(tween.to(a, 0.4, {y = 300})) ...
local glow = tween.timeline():append(tween.to(c, 0.8, {color = '#FFF2B23A'})):append(tween.to(c, 0.8, {color = '#FF3DBE7A'}))
local spin = tween.timeline():append(tween.to(b, 0.8, {angle = math.pi})):append(tween.to(b, 0.6, {scale = 1.6}))
tween.timeline({repeatCount = -1, loopMode = 'yoyo'}):append(square):join(glow):append(spin)]]

function Nested:enter()
    self:frame({
        hint = 'The parent drives its children, so pausing, reversing or restarting it moves all three. R or the X button restarts it.',
        code = kCode,
        controls = {
            ui.button{id = 'restart', text = 'Restart', variant = 'primary', onClick = function() self:play() end},
            ui.row{gap = 12,
                ui.button{id = 'pause', text = 'Pause', grow = 1, onClick = function() self.parent:pause() end},
                ui.button{id = 'resume', text = 'Resume', grow = 1, onClick = function() self.parent:resume() end},
            },
            ui.button{id = 'reverse', text = 'Reverse', onClick = function() self.parent:reverse() end},
        },
        focus = 'restart',
    })
    self:play()
end

function Nested:play()
    if self.parent then
        self.parent:kill()
    end
    local a, b, c = {x = 0, y = 0}, {angle = 0, scale = 1}, {color = m.color('#FF4C7DFF')}
    self.a, self.b, self.c = a, b, c

    self.square = tween.timeline()
        :append(tween.to(a, 0.4, {x = 300}, {ease = 'quadInOut'}))
        :append(tween.to(a, 0.4, {y = 300}, {ease = 'quadInOut'}))
        :append(tween.to(a, 0.4, {x = 0}, {ease = 'quadInOut'}))
        :append(tween.to(a, 0.4, {y = 0}, {ease = 'quadInOut'}))
    self.glow = tween.timeline()
        :append(tween.to(c, 0.8, {color = '#FFF2B23A'}))
        :append(tween.to(c, 0.8, {color = '#FF3DBE7A'}))
    self.spin = tween.timeline()
        :append(tween.to(b, 0.8, {angle = math.pi}, {ease = 'backOut'}))
        :append(tween.to(b, 0.6, {scale = 1.6}, {ease = 'elasticOut'}))
    self.parent = tween.timeline({owner = self, repeatCount = -1, loopMode = 'yoyo', repeatDelay = 0.3})
        :append(self.square)
        :join(self.glow)
        :append(self.spin)
end

function Nested:update(dt)
    Nested.super.update(self, dt)
    if input.pressed('replay') then
        self:play()
    end
    self:status(string.format('Parent %.2f, square %.2f, glow %.2f, spin %.2f', self.parent.progress, self.square.progress, self.glow.progress, self.spin.progress))
end

-- Draws a square of `size` centered on `x`, `y` and turned by `angle`.
local function drawTurned(x, y, size, angle, color)
    local points = {}
    for corner = 0, 3 do
        local turn = angle + math.pi / 4 + corner * math.pi / 2
        points[corner + 1] = {x + math.cos(turn) * size * 0.707, y + math.sin(turn) * size * 0.707}
    end
    graphics2d.drawPolygon(points, color)
end

function Nested:draw(area)
    local originX, originY = 80, 60
    graphics2d.drawRectOutline({originX, originY, 360, 360}, 2, Test.line)
    graphics2d.drawRect({originX + self.a.x, originY + self.a.y, 60, 60}, self.c.color, {layer = 1})
    drawTurned(area.width * 0.62, originY + 180, 120 * self.b.scale, self.b.angle, Test.accent)

    local bars = {{'Parent', self.parent}, {'Square', self.square}, {'Glow', self.glow}, {'Spin', self.spin}}
    for index, bar in ipairs(bars) do
        local y = area.height - 190 + index * 36
        graphics2d.drawText(nil, bar[1], 80, y, {size = 24, color = Test.ink, anchor = {0, 0.5}})
        graphics2d.drawRect({200, y - 8, area.width - 280, 16}, Test.line)
        graphics2d.drawRect({200, y - 8, (area.width - 280) * bar[2].progress, 16}, index == 1 and Test.warm or Test.accent, {layer = 1})
    end
end

return Nested
