-- Nested timelines: three child timelines inside a parent that plays two of them together and the third after them, and yoyos as a whole. A coin travels a square while its glow changes color, then a star spins and grows, over a chart of the children in time with the cursor of the parent going back and forth.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TweenTest = require('categories.tween.tween-test')

local Nested = haylen.class('Nested', TweenTest)

local kView = {1400, 700}
local kSquare = {200, 110, 300}
local kSquareTime, kSpinTime = 1.6, 1.4
-- The children of the parent in its time, with the steps of each one.
local kRows = {
    {title = 'Square', from = 0, steps = {0.4, 0.4, 0.4, 0.4}, names = {'Right', 'Down', 'Left', 'Up'}},
    {title = 'Glow', from = 0, steps = {0.8, 0.8}, names = {'To gold', 'To green'}},
    {title = 'Spin', from = kSquareTime, steps = {0.8, 0.6}, names = {'Turn', 'Grow'}},
}
local kCode = [[
local square = tween.timeline():append(tween.to(a, 0.4, {x = 300})):append(tween.to(a, 0.4, {y = 300}))
local glow = tween.timeline():append(tween.to(c, 0.8, {color = '#FFF2B23A'})):append(tween.to(c, 0.8, {color = '#FF3DBE7A'}))
local spin = tween.timeline():append(tween.to(b, 0.8, {angle = math.pi})):append(tween.to(b, 0.6, {scale = 1.6}))
tween.timeline({repeatCount = -1, loopMode = 'yoyo'}):append(square):join(glow):append(spin)]]

function Nested:enter()
    self.coin, self.star = TweenTest.icon('coin'), TweenTest.icon('star')
    self.trail = TweenTest.trail()
    self:frame({
        hint = 'The parent drives its children, so pausing, reversing or restarting it moves all three. R or the X button restarts it.',
        code = kCode,
        controls = {
            ui.button{id = 'restart', text = 'Restart', variant = 'primary', onClick = function() self:play() end},
            ui.row{wrap = true, gap = 12,
                ui.button{id = 'pause', text = 'Pause', grow = 1, onClick = function() self.parent:pause() end},
                ui.button{id = 'resume', text = 'Resume', grow = 1, onClick = function() self.parent:resume() end},
            },
            ui.button{id = 'reverse', text = 'Reverse', onClick = function() self.parent:reverse() end},
        },
        view = kView,
        focus = 'restart',
    })
    self.camera:snapTo(kView[1] / 2, kView[2] / 2)
    self:play()
end

function Nested:play()
    if self.parent then
        self.parent:kill()
    end
    local a, b, c = {x = 0, y = 0}, {angle = 0, scale = 1}, {color = m.color('#FF4C7DFF')}
    self.a, self.b, self.c = a, b, c
    self.trail:clear()

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

-- Where the parent stands in its own time, found from its children, which show it whichever way the parent plays.
function Nested:parentTime()
    if self.spin.progress > 0 then
        return kSquareTime + self.spin.progress * kSpinTime
    end
    return self.square.progress * kSquareTime
end

function Nested:update(dt)
    Nested.super.update(self, dt)
    if input.pressed('replay') then
        self:play()
    end
    self.trail:push(m.vec2(self.a.x, self.a.y))
    self:status(string.format('The parent is at %.2f seconds, %s. The square is %.0f%% done, the glow %.0f%% and the spin %.0f%%.', self:parentTime(), self.parent.paused and 'paused' or 'playing', self.square.progress * 100, self.glow.progress * 100, self.spin.progress * 100))
end

function Nested:drawSquare()
    local x, y, size = kSquare[1], kSquare[2], kSquare[3]
    graphics2d.drawText(nil, 'Square and glow', x + size / 2, 20, {size = 24, color = Test.ink, anchor = {0.5, 0}})
    graphics2d.drawStroke({x, y, x + size, y, x + size, y + size, x, y + size}, {width = 4, color = TweenTest.grooveEdge, closed = true, join = 'round', dashes = {14, 10}})
    for index = 1, self.trail.size - 1 do
        local point = self.trail:get(index)
        local share = index / self.trail.size
        graphics2d.draw(self.coin, x + point.x, y + point.y, {width = 50 * share, height = 50 * share, color = m.color(1, 1, 1, share * 0.25), layer = 1})
    end
    local cx, cy = x + self.a.x, y + self.a.y
    graphics2d.drawShape({cx - 52, cy - 52, 104, 104}, {kind = 'ellipse', color = self.c.color:withAlpha(0.55), softness = 36, layer = 1})
    graphics2d.draw(self.coin, cx, cy, {width = 72, height = 72, layer = 2})
end

function Nested:drawSpin()
    local x, y = 960, kSquare[2] + kSquare[3] / 2
    graphics2d.drawText(nil, 'Spin', x, 20, {size = 24, color = Test.ink, anchor = {0.5, 0}})
    graphics2d.drawShape({x - 150, y - 150, 300, 300}, {kind = 'ellipse', color = '#00000000', borderWidth = 4, borderColor = TweenTest.grooveEdge})
    local size = 150 * self.b.scale
    graphics2d.draw(self.star, x, y, {width = size, height = size, rotation = self.b.angle, layer = 1})
end

-- The chart of the children in the time of the parent, each step a rounded span, with the cursor of the parent.
function Nested:drawChart()
    local card = {0, 460, kView[1], 230}
    graphics2d.drawShape(card, {radius = 18, color = '#FF1C2130', borderWidth = 2, borderColor = Test.line})
    local total = kSquareTime + kSpinTime
    local left, right = 160, kView[1] - 40
    local function toX(seconds)
        return left + (right - left) * seconds / total
    end
    for half = 0, total * 2 do
        local x = toX(half / 2)
        graphics2d.drawLine(x, 482, x, 664, 1, '#FF2A3043')
        graphics2d.drawText(nil, string.format('%.1f s', half / 2), x, 668, {size = 16, color = Test.muted, anchor = {0.5, 0}})
    end
    for index, row in ipairs(kRows) do
        local y = 488 + (index - 1) * 56
        graphics2d.drawText(nil, row.title, 28, y + 20, {size = 22, color = Test.ink, anchor = {0, 0.5}})
        local at = row.from
        for step, seconds in ipairs(row.steps) do
            local rect = {toX(at) + 2, y + 4, toX(at + seconds) - toX(at) - 4, 32}
            graphics2d.drawShape(rect, {radius = 16, color = index == 3 and Test.violet or (index == 2 and Test.warm or Test.accent)})
            graphics2d.drawText(nil, row.names[step], rect[1] + rect[3] / 2, y + 20, {size = 16, color = '#FF14213D', anchor = {0.5, 0.5}, layer = 1})
            at = at + seconds
        end
    end
    local cursor = toX(self:parentTime())
    graphics2d.drawShape({cursor - 2, 476, 4, 190}, {radius = 2, color = Test.ink, layer = 2})
    graphics2d.drawShape({cursor - 8, 470, 16, 16}, {kind = 'ellipse', color = Test.ink, borderWidth = 3, borderColor = '#FF1E1B33', layer = 2})
end

function Nested:draw(area)
    self:drawSquare()
    self:drawSpin()
    self:drawChart()
end

return Nested
