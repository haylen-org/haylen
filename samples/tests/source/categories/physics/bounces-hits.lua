-- Bouncy balls dropped from five heights in two worlds where a meter is 300 units, so one meter per second is a speed the eye sees. On the left the default thresholds of one meter per second stop slow impacts dead and report no hit for them, so the low balls never bounce and the high ones stop while their bounces still show, and on the right `restitutionThreshold` and `hitThreshold` are low, so every ball keeps bouncing and every impact flashes through `world.onHit`.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local BouncesHits = haylen.class('BouncesHits', PhysicsTest)

BouncesHits.code = [[
local world = physics2d.newWorld({pixelsPerMeter = 300})  -- Bounces and hits from 300 units per second.
local lively = physics2d.newWorld({pixelsPerMeter = 300, restitutionThreshold = 4, hitThreshold = 8})
ball:addCircle(24, {restitution = 0.9})
world.onHit = function(a, b, contact) flash(a, b) end  -- Every impact faster than the hit threshold.]]

BouncesHits.ground = 340
BouncesHits.radius = 24
BouncesHits.heights = {20, 45, 90, 180, 330}
BouncesHits.flash = 0.25
BouncesHits.colors = {'teal', 'green', 'yellow', 'orange', 'red'}

function BouncesHits:enter()
    self:frame{
        code = BouncesHits.code,
        hint = 'Each ball flashes a ring when its world reports a hit. Drop the balls again to start the bounces over. R or the X button starts over.',
        controls = {
            ui.button{id = 'drop', text = 'Drop the balls again', onClick = function() self:drop() end},
            ui.label{text = 'Bounciness of the balls', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'restitution', value = 0.9, min = 0.3, max = 1, step = 0.05, showValue = true, decimals = 2, onChange = function(event) self:setRestitution(event.value) end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        pointer = false,
        focus = 'drop',
    }
    self.restitution = 0.9
    self:build()
end

function BouncesHits:build()
    self.halves = {}
    for index, low in ipairs({false, true}) do
        local origin = index == 1 and -400 or 400
        local options = {pixelsPerMeter = 300}
        if low then
            options.restitutionThreshold, options.hitThreshold = 4, 8
        end
        local world = physics2d.newWorld(options)
        local floor = world:createBody({type = 'static', x = origin, y = BouncesHits.ground + 30})
        floor:addBox(800, 60)
        local half = {world = world, origin = origin, balls = {}, hits = 0}
        for slot = 1, #BouncesHits.heights do
            local ball = world:createBody({x = origin + (slot - 3) * 130})
            ball.data = {shape = ball:addCircle(BouncesHits.radius, {restitution = self.restitution}), flash = 0, slot = slot}
            Look.dress(ball, BouncesHits.colors[slot])
            half.balls[slot] = ball
        end
        world.onHit = function(a, b)
            for _, body in ipairs({a, b}) do
                if body.data and body.data.flash then
                    body.data.flash = BouncesHits.flash
                    half.hits = half.hits + 1
                end
            end
        end
        self.halves[index] = half
    end
    self:drop()
end

function BouncesHits:drop()
    for _, half in ipairs(self.halves) do
        for slot, ball in ipairs(half.balls) do
            ball:setTransform(ball.x, BouncesHits.ground - BouncesHits.radius - BouncesHits.heights[slot], 0)
            ball.velocity = {0, 0}
            ball.awake = true
        end
    end
end

function BouncesHits:setRestitution(value)
    self.restitution = value
    for _, half in ipairs(self.halves) do
        for _, ball in ipairs(half.balls) do
            ball.data.shape.restitution = value
        end
    end
end

-- Counts the balls of a side that still bounce.
function BouncesHits:bouncing(half)
    local count = 0
    for _, ball in ipairs(half.balls) do
        count = count + ((BouncesHits.ground - BouncesHits.radius - ball.y > 1 or math.abs(ball.velocity.y) > 4) and 1 or 0)
    end
    return count
end

function BouncesHits:update(dt)
    BouncesHits.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    for _, half in ipairs(self.halves) do
        for _, ball in ipairs(half.balls) do
            ball.data.flash = math.max(0, ball.data.flash - dt)
        end
    end
    local left, right = self.halves[1], self.halves[2]
    self:status(string.format('With the default thresholds %d of 5 balls still bounce and %d hits were reported, and with low ones %d still bounce after %d hits.', self:bouncing(left), left.hits, self:bouncing(right), right.hits))
end

function BouncesHits:fixedUpdate(step)
    for _, half in ipairs(self.halves) do
        self:simulate(half.world, step)
    end
end

function BouncesHits:draw(area)
    Scenery.sky(self.camera, BouncesHits.ground)
    Scenery.ground(BouncesHits.ground)
    self:compare('Default thresholds', 'Low thresholds', 0, -350)
    local unit = Look.pixel
    for _, half in ipairs(self.halves) do
        self:label(string.format('%d hits reported', half.hits), half.origin, -300, {size = 18})
        for slot, ball in ipairs(half.balls) do
            local start = BouncesHits.ground - BouncesHits.radius - BouncesHits.heights[slot]
            Look.dashed(ball.x - 34, start, ball.x + 34, start, '#B32F3352', 0)
            self:tag(string.format('%d', BouncesHits.heights[slot]), ball.x, start - BouncesHits.radius - 14, {size = 16})
            Look.draw(ball, {layer = 1})
            local flash = ball.data.flash / BouncesHits.flash
            if flash > 0 then
                local grow = 1 - flash
                graphics2d.drawRing(ball.x, BouncesHits.ground - 4, BouncesHits.radius * (0.8 + grow * 1.4), 5 * unit * flash + unit, {1, 1, 1, flash}, {layer = 2})
            end
        end
    end
end

return BouncesHits
