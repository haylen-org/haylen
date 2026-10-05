-- Two characters on the same hill and stairs, driven by the same input: on the left a dynamic capsule without friction, as characters often are so they never stick to walls, slides down slopes and stumbles over steps, and on the right a mover from `physics2d.newMover` stands still on slopes, keeps its speed on them, steps up the stairs and stays on the ground going down.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local SlopesSteps = haylen.class('SlopesSteps', PhysicsTest)

SlopesSteps.actions = {
    {name = 'move', type = 'axis', positive = {'key:d', 'key:right', 'button:dpadRight', 'axis:leftX+', 'virtual:right'}, negative = {'key:a', 'key:left', 'button:dpadLeft', 'axis:leftX-', 'virtual:left'}},
    {name = 'jump', type = 'button', bindings = {'key:space', 'key:w', 'key:up', 'button:south', 'virtual:jump'}},
}

local kGravity = 1800
local kSpeed = 320
local kJump = 760
local kRadius, kHeight = 14, 56

-- The ground of both sides relative to the middle of a side: a flat start, a 30 degree hill, a plateau, stairs down and a 60 degree wall that is too steep to walk.
local kGround = {{-380, -420}, {-380, 200}, {-260, 200}, {-60, 85}, {20, 85}, {20, 103}, {60, 103}, {60, 121}, {100, 121}, {100, 139}, {140, 139}, {140, 157}, {180, 157}, {180, 175}, {220, 175}, {220, 200}, {300, 200}, {380, 60}, {380, -420}}

function SlopesSteps:enter()
    self:frame{
        hint = 'Both characters start on the hill. Walk with A and D, the arrows, the left stick or the touch buttons, and jump with Space, W or the south button. Both characters get the same input. R or the X button starts over.',
        controls = {
            ui.label{text = 'Steepest slope the mover walks, in degrees', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'slope', value = 50, min = 10, max = 80, step = 5, showValue = true, decimals = 0, onChange = function(event) self.mover.maxSlope = math.rad(event.value) end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        play = true,
        pointer = false,
        actions = SlopesSteps.actions,
        overlay = {
            ui.touchButton{action = 'left', text = 'Left', size = 130, touchOnly = true, anchor = 'bottomLeft', margin = {0, 0, 110, 40}},
            ui.touchButton{action = 'right', text = 'Right', size = 130, touchOnly = true, anchor = 'bottomLeft', margin = {0, 0, 110, 190}},
            ui.touchButton{action = 'jump', text = 'Jump', size = 150, touchOnly = true, anchor = 'bottomRight', margin = {0, 540, 110, 0}},
        },
    }
    self:build()
end

function SlopesSteps:build()
    self.sides = {}
    for index = 1, 2 do
        local origin = index == 1 and -400 or 400
        local world = physics2d.newWorld({gravity = {0, kGravity}})
        local ground = world:createBody({type = 'static', x = origin})
        parts.chain(ground, kGround, false, {friction = 0.6})
        self.sides[index] = {world = world, origin = origin, ground = ground}
    end

    local left, right = self.sides[1], self.sides[2]
    self.capsule = left.world:createBody({x = left.origin - 160, y = 100, fixedRotation = true, sleepEnabled = false})
    parts.capsule(self.capsule, 0, -(kHeight / 2 - kRadius), 0, kHeight / 2 - kRadius, kRadius, {friction = 0})
    parts.paint(self.capsule, '#FFE57373')
    self.mover = physics2d.newMover(right.world, {x = right.origin - 160, y = 100, radius = kRadius, height = kHeight, stepHeight = 20, snapDistance = 20, maxSlope = math.rad(50)})
    self.velocity = {x = 0, y = 0}
end

-- The capsule stands when a short ray from its feet meets the ground.
function SlopesSteps:capsuleGrounded()
    local x, y = self.capsule.x, self.capsule.y
    return self.sides[1].world:raycast(x, y + kHeight / 2 - 2, x, y + kHeight / 2 + 6) ~= nil
end

function SlopesSteps:exit()
    SlopesSteps.super.exit(self)
    input.clearVirtual()
end

function SlopesSteps:update(dt)
    SlopesSteps.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    if input.pressed('jump') then
        self.jumping = true
    end
    local mover = self.mover
    self:status(string.format('Capsule speed %.0f, %.0f. Mover speed %.0f, %.0f, %s, ground normal %.2f, %.2f, step %.2f ms', self.capsule.velocity.x, self.capsule.velocity.y, self.velocity.x, self.velocity.y, mover.grounded and 'grounded' or 'in the air', mover.groundNormal.x, mover.groundNormal.y, self:stepTime()))
end

function SlopesSteps:fixedUpdate(step)
    local move = input.value('move')
    local jump = self.jumping
    self.jumping = false

    -- The capsule takes the input as its horizontal velocity and keeps the vertical one that gravity gives it.
    local capsule = self.capsule
    local vy = capsule.velocity.y
    if jump and self:capsuleGrounded() then
        vy = -kJump
    end
    capsule.velocity = {move * kSpeed, vy}

    -- The mover integrates its own velocity, moves by it and drops the part that pushes into what it hit.
    local mover = self.mover
    local velocity = mover:clip(move * kSpeed, self.velocity.y + kGravity * step)
    if jump and mover.grounded then
        velocity.y = -kJump
    end
    mover:move(velocity.x * step, velocity.y * step)
    self.velocity = mover:clip(velocity.x, velocity.y)

    for _, side in ipairs(self.sides) do
        self:simulate(side.world, step)
    end
end

function SlopesSteps:draw(area)
    graphics2d.drawLine(0, -430, 0, 430, 2, '#44FFFFFF')
    local titles = {'Dynamic capsule', 'Mover'}
    for index, side in ipairs(self.sides) do
        graphics2d.drawText(nil, titles[index], side.origin, -380, {size = 28, color = index == 1 and '#FFFF8A84' or '#FF6FDCA0', anchor = {0.5, 0.5}})
        parts.draw(side.ground)
    end
    parts.draw(self.capsule, {layer = 1})
    local position = self.mover.position
    local reach = kHeight / 2 - kRadius
    graphics2d.drawLine(position.x, position.y - reach, position.x, position.y + reach, kRadius * 2, '#FF6FDCA0', {layer = 1})
    graphics2d.drawCircle(position.x, position.y - reach, kRadius, '#FF6FDCA0', {layer = 1})
    graphics2d.drawCircle(position.x, position.y + reach, kRadius, '#FF6FDCA0', {layer = 1})
end

return SlopesSteps
