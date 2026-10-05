-- Bouncy balls dropped from five heights in two worlds. On the left the default thresholds stop impacts slower than one meter per second dead and report no hit for them, and on the right `restitutionThreshold` and `hitThreshold` are low, so even the lowest ball keeps bouncing and every impact flashes through `world.onHit`.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local PhysicsTest = require('categories.physics.physics-test')

local BouncesHits = haylen.class('BouncesHits', PhysicsTest)

local kFloorY = 360
local kRadius = 22
local kHeights = {1, 2, 5, 30, 200}
local kFlash = 0.15

function BouncesHits:enter()
    self:frame{
        hint = 'Each ball flashes when its world reports a hit. Drop the balls again to start the bounces over. R or the X button starts over.',
        controls = {
            ui.button{id = 'drop', text = 'Drop the balls again', onClick = function() self:drop() end},
            ui.label{text = 'Restitution of the balls', font = 'caption', color = 'textMuted'},
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
    self.sides = {}
    for index, low in ipairs({false, true}) do
        local origin = index == 1 and -400 or 400
        local world = physics2d.newWorld(low and {restitutionThreshold = 4, hitThreshold = 8} or {})
        local floor = world:createBody({type = 'static', x = origin, y = kFloorY + 20})
        floor:addBox(760, 40)
        local side = {world = world, origin = origin, balls = {}, hits = 0}
        for slot = 1, #kHeights do
            local ball = world:createBody({x = origin + (slot - 3) * 130})
            ball.data = {shape = ball:addCircle(kRadius, {restitution = self.restitution}), flash = 0, slot = slot}
            side.balls[slot] = ball
        end
        world.onHit = function(a, b)
            for _, body in ipairs({a, b}) do
                if body.data and body.data.flash then
                    body.data.flash = kFlash
                    side.hits = side.hits + 1
                end
            end
        end
        self.sides[index] = side
    end
    self:drop()
end

function BouncesHits:drop()
    for _, side in ipairs(self.sides) do
        for slot, ball in ipairs(side.balls) do
            ball:setTransform(ball.x, kFloorY - kRadius - kHeights[slot], 0)
            ball.velocity = {0, 0}
            ball.awake = true
        end
    end
end

function BouncesHits:setRestitution(value)
    self.restitution = value
    for _, side in ipairs(self.sides) do
        for _, ball in ipairs(side.balls) do
            ball.data.shape.restitution = value
        end
    end
end

function BouncesHits:update(dt)
    BouncesHits.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    for _, side in ipairs(self.sides) do
        for _, ball in ipairs(side.balls) do
            ball.data.flash = math.max(0, ball.data.flash - dt)
        end
    end
    local left, right = self.sides[1], self.sides[2]
    self:status(string.format('Hits reported %d with the default thresholds and %d with low ones, lowest ball speeds %.0f and %.0f, step %.2f ms', left.hits, right.hits, math.abs(left.balls[1].velocity.y), math.abs(right.balls[1].velocity.y), self:stepTime()))
end

function BouncesHits:fixedUpdate(step)
    for _, side in ipairs(self.sides) do
        self:simulate(side.world, step)
    end
end

function BouncesHits:draw(area)
    graphics2d.drawLine(0, -430, 0, 430, 2, '#44FFFFFF')
    local titles = {'Default thresholds', 'Low thresholds'}
    for index, side in ipairs(self.sides) do
        graphics2d.drawText(nil, titles[index], side.origin, -390, {size = 28, color = index == 1 and '#FFFF8A84' or '#FF6FDCA0', anchor = {0.5, 0.5}})
        graphics2d.drawText(nil, string.format('%d hits', side.hits), side.origin, -350, {size = 22, color = '#CCFFFFFF', anchor = {0.5, 0.5}})
        graphics2d.drawRect({side.origin - 380, kFloorY, 760, 40}, '#FF4F5B6E')
        for slot, ball in ipairs(side.balls) do
            local color = ball.data.flash > 0 and '#FFFFFFFF' or '#FF64B5F6'
            graphics2d.drawCircle(ball.x, ball.y, kRadius, color, {layer = 1})
            graphics2d.drawText(nil, string.format('%d', kHeights[slot]), ball.x, kFloorY + 60, {size = 20, color = '#88FFFFFF', anchor = {0.5, 0.5}})
        end
    end
end

return BouncesHits
