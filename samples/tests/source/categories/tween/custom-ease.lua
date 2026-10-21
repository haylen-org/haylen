-- Custom easing: curves written as Lua functions from progress to eased progress, each plotted and driving an icon along its rail, which leaves room past both ends for the curves that overshoot.
local haylen = require('haylen')
local input = require('haylen.input')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TweenTest = require('categories.tween.tween-test')

local CustomEase = haylen.class('CustomEase', TweenTest)

local kSamples = 60
local kLabel = 460
local kCurves = {
    {title = 'Wobble', caption = 'Shivers on its way', icon = 'coin', color = Test.warm, ease = function(t) return t + math.sin(t * math.pi * 6) * (1 - t) * 0.2 end},
    {title = 'Spring', caption = 'Overshoots and settles', icon = 'gem', color = Test.accent, ease = function(t) return 1 - math.cos(t * 4.5 * math.pi) * math.exp(-t * 6) end},
    {title = 'Heartbeat', caption = 'Two beats on the way', icon = 'heart', color = Test.red, ease = function(t) return t + math.sin(t * math.pi * 4) ^ 8 * 0.25 end},
    {title = 'Smootherstep', caption = 'Eases in and out', icon = 'star', color = Test.green, ease = function(t) return t * t * t * (t * (t * 6 - 15) + 10) end},
}
local kCode = [[
local spring = function(t) return 1 - math.cos(t * 4.5 * math.pi) * math.exp(-t * 6) end
tween.to(ball, 2, {x = 1000}, {ease = spring, repeatCount = -1, repeatDelay = 0.5})]]

function CustomEase:enter()
    self.balls, self.trails, self.icons = {}, {}, {}
    self.low, self.high = 0, 1
    for index, curve in ipairs(kCurves) do
        self.balls[index] = {x = 0}
        self.trails[index] = TweenTest.trail()
        self.icons[index] = TweenTest.icon(curve.icon)
        curve.values, curve.low, curve.high = TweenTest.sample(curve.ease, kSamples)
        curve.points = {}
        self.low, self.high = math.min(self.low, curve.low), math.max(self.high, curve.high)
    end
    self:frame({
        hint = 'Every rail runs past 0 and 1000, so the curves that overshoot stay in view. Replay with the button, R or the X button.',
        code = kCode,
        controls = {ui.button{id = 'replay', text = 'Replay', variant = 'primary', onClick = function() self:play() end}},
        focus = 'replay',
    })
    self:play()
end

function CustomEase:play()
    for index, curve in ipairs(kCurves) do
        local ball = self.balls[index]
        tween.killTarget(ball)
        ball.x = 0
        tween.to(ball, 2, {x = 1000}, {owner = self, ease = curve.ease, repeatCount = -1, repeatDelay = 0.5})
    end
end

function CustomEase:update(dt)
    CustomEase.super.update(self, dt)
    if input.pressed('replay') then
        self:play()
    end
    for index, ball in ipairs(self.balls) do
        self.trails[index]:push(ball.x)
    end
    self:status(string.format('The wobble stands at %.0f, the spring at %.0f, the heartbeat at %.0f and the smootherstep at %.0f.', self.balls[1].x, self.balls[2].x, self.balls[3].x, self.balls[4].x))
end

function CustomEase:draw(area)
    local min, max = self.low * 1000 - 40, self.high * 1000 + 40
    for index, lane in ipairs(TweenTest.lanes(area, #kCurves, kLabel)) do
        local curve = kCurves[index]
        local ball = self.balls[index]
        TweenTest.drawLane(lane, {title = curve.title, caption = curve.caption, captionWidth = 200, value = ball.x, from = 0, to = 1000, min = min, max = max, color = curve.color, icon = self.icons[index], trail = self.trails[index], ticks = {0, 500, 1000}})
        local plot = {lane.x - 236, lane.y + 10, 196, lane.height - 20}
        TweenTest.drawPlot(plot, curve.values, curve.low, curve.high, {color = curve.color, points = curve.points})
    end
end

return CustomEase
