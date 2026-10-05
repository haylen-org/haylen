-- Custom easing: curves written as Lua functions from progress to eased progress, each plotted and driving a ball across its lane.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TweenTest = require('categories.tween.tween-test')

local CustomEase = haylen.class('CustomEase', TweenTest)

local kSamples = 60
local kCurves = {
    {name = 'Wobble', ease = function(t) return t + math.sin(t * math.pi * 6) * (1 - t) * 0.2 end},
    {name = 'Spring', ease = function(t) return 1 - math.cos(t * 4.5 * math.pi) * math.exp(-t * 6) end},
    {name = 'Heartbeat', ease = function(t) return t + math.sin(t * math.pi * 4) ^ 8 * 0.25 end},
    {name = 'Smootherstep', ease = function(t) return t * t * t * (t * (t * 6 - 15) + 10) end},
}
local kCode = [[
local spring = function(t) return 1 - math.cos(t * 4.5 * math.pi) * math.exp(-t * 6) end
tween.to(ball, 2, {x = 1000}, {ease = spring, repeatCount = -1, repeatDelay = 0.5})]]

function CustomEase:enter()
    self.balls = {}
    for index = 1, #kCurves do
        self.balls[index] = {x = 0}
    end
    self:frame({
        hint = 'Replay with the button, R or the X button.',
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
    self:status(string.format('Wobble %.0f, spring %.0f, heartbeat %.0f, smootherstep %.0f', self.balls[1].x, self.balls[2].x, self.balls[3].x, self.balls[4].x))
end

function CustomEase:draw(area)
    local lanes = TweenTest.lanes({x = area.x + 260, y = area.y, width = area.width - 260, height = area.height}, #kCurves)
    for index, lane in ipairs(lanes) do
        local curve = kCurves[index]
        TweenTest.drawLane(lane, curve.name, self.balls[index].x, Test.green)

        local plotX, plotY, plotWidth, plotHeight = 24, lane.y + 16, 220, lane.height - 32
        graphics2d.drawRectOutline({plotX, plotY, plotWidth, plotHeight}, 1, Test.line)
        local points = {}
        for step = 0, kSamples do
            local t = step / kSamples
            points[step + 1] = {plotX + plotWidth * t, plotY + plotHeight * (0.9 - curve.ease(t) * 0.8)}
        end
        graphics2d.drawPolyline(points, 2, Test.warm)
    end
end

return CustomEase
