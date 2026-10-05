-- One world drawn twice in slow motion. The world steps at the fixed rate of the app, so in slow motion several frames pass between two steps: on the left the bodies are drawn where the last step left them and move in jumps, and on the right the world interpolates and `body:renderTransform()` blends between the last two steps, so they glide.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Interpolation = haylen.class('Interpolation', PhysicsTest)

local kOffset = 400
local kBallRadius = 26
local kBar = {width = 260, height = 18}

function Interpolation:enter()
    self:frame{
        hint = 'Slow the time down to see the steps. The world is the same on both sides, only the drawing differs. R or the X button starts over.',
        controls = {
            ui.label{text = 'Time scale', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'scale', value = 0.1, min = 0.02, max = 1, step = 0.02, showValue = true, decimals = 2, onChange = function(event) haylen.setTimeScale(event.value) end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        pointer = false,
        focus = 'scale',
    }
    haylen.setTimeScale(0.1)
    self:build()
end

function Interpolation:build()
    self.world = physics2d.newWorld({interpolate = true})
    local pivot = self.world:createBody({type = 'static', x = 0, y = -340})
    self.pendulum = self.world:createBody({x = 220, y = -340})
    self.pendulum:addCircle(kBallRadius, {density = 2})
    self.world:createJoint('distance', pivot, self.pendulum, {ax = 0, ay = -340, bx = 220, by = -340})

    local hub = self.world:createBody({type = 'static', x = 0, y = 60})
    self.bar = self.world:createBody({x = 0, y = 60, gravityScale = 0})
    self.bar:addBox(kBar.width, kBar.height)
    self.world:createJoint('revolute', hub, self.bar, {ax = 0, ay = 60, enableMotor = true, motorSpeed = 3, maxMotorTorque = 1e9})

    local floor = self.world:createBody({type = 'static', x = 250, y = 380})
    floor:addBox(160, 20)
    self.ball = self.world:createBody({x = 250, y = 100})
    self.ball:addCircle(kBallRadius, {restitution = 0.9})
end

function Interpolation:exit()
    Interpolation.super.exit(self)
    haylen.setTimeScale(1)
end

function Interpolation:update(dt)
    Interpolation.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self:status(string.format('Time scale %.2f, fixed steps every %.1f frames, interpolation %.2f, step %.2f ms', haylen.timeScale(), 1 / math.max(haylen.timeScale(), 0.001) * haylen.fixedStep() * 60, haylen.interpolation(), self:stepTime()))
end

function Interpolation:fixedUpdate(step)
    self:simulate(self.world, step)
end

function Interpolation:drawBodies(offset, transform, color)
    local order = {layer = 1}
    local x, y = transform(self.pendulum)
    graphics2d.drawLine(offset, -340, x + offset, y, 3, '#88FFFFFF', order)
    graphics2d.drawCircle(x + offset, y, kBallRadius, color, order)
    local bx, by, rotation = transform(self.bar)
    local cos, sin = math.cos(rotation), math.sin(rotation)
    local hx, hy = cos * kBar.width / 2, sin * kBar.width / 2
    graphics2d.drawLine(bx - hx + offset, by - hy, bx + hx + offset, by + hy, kBar.height, color, order)
    local cx, cy = transform(self.ball)
    graphics2d.drawCircle(cx + offset, cy, kBallRadius, color, order)
    graphics2d.drawRect({offset + 170, 370, 160, 20}, '#FF4F5B6E')
end

function Interpolation:draw(area)
    graphics2d.drawLine(0, -430, 0, 430, 2, '#44FFFFFF')
    graphics2d.drawText(nil, 'Where the last step left them', -kOffset, -400, {size = 28, color = '#FFFF8A84', anchor = {0.5, 0.5}})
    graphics2d.drawText(nil, 'Interpolated with renderTransform', kOffset, -400, {size = 28, color = '#FF6FDCA0', anchor = {0.5, 0.5}})
    self:drawBodies(-kOffset, function(body) return body.x, body.y, body.rotation end, '#FFE57373')
    self:drawBodies(kOffset, function(body) return body:renderTransform() end, '#FF6FDCA0')
end

return Interpolation
