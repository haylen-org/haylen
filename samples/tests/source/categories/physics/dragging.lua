-- Dragging a ragdoll up the stairs with the same strength in two worlds. On the left a mouse joint pulls with a force sized to the one limb it holds, which drags the figure along the steps without lifting it, and on the right `physics2d.newGrabber` sizes the pull to the limb and everything joined to it, so the figure follows the pointer like a crate does.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Dragging = haylen.class('Dragging', PhysicsTest)

local kStrength = 8
local kStandardGravity = 9.80665
local kSteps, kStepWidth, kStepHeight = 5, 70, 50
local kCycle = 5
local kReach = 28

function Dragging:enter()
    self:frame{
        hint = 'Drag a ragdoll or the crate on either side. Every five seconds both sides pull the hand of their ragdoll to the top of the stairs by themselves. R or the X button starts over.',
        controls = {
            ui.checkbox{id = 'automatic', text = 'Pull the hands by themselves', checked = true, onChange = function(event) self.automatic = event.checked end},
            ui.label{text = 'Strength in weights of what the pull moves', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'strength', value = kStrength, min = 2, max = 80, step = 1, showValue = true, decimals = 0, onChange = function(event) self:setStrength(event.value) end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        focus = 'automatic',
    }
    self.automatic = true
    self.strength = kStrength
    self:build()
end

function Dragging:build()
    self.sides = {}
    for index, grabber in ipairs({false, true}) do
        local origin = index == 1 and -400 or 400
        local world = physics2d.newWorld()
        local statics = {}
        local ground = world:createBody({type = 'static', x = origin, y = 400})
        parts.box(ground, 780, 40)
        parts.box(ground, 20, 800, {offsetX = -380, offsetY = -400})
        parts.box(ground, 20, 800, {offsetX = 380, offsetY = -400})
        statics[1] = ground
        -- The stairs rise to the right, and the top step reaches the edge of the side.
        for step = 1, kSteps do
            local left = origin - 100 + (step - 1) * kStepWidth
            local right = step == kSteps and origin + 380 or left + kStepWidth
            local stair = world:createBody({type = 'static', x = (left + right) / 2, y = 380 - step * kStepHeight / 2})
            parts.box(stair, right - left, step * kStepHeight, {friction = 0.8})
            statics[#statics + 1] = stair
        end
        local side = {world = world, origin = origin, statics = statics, grabber = grabber}
        side.doll = physics2d.newRagdoll(world, {x = origin - 220, y = 300, height = 150})
        side.crate = world:createBody({x = origin - 330, y = 350})
        parts.box(side.crate, 50, 50, {density = 2})
        side.anchor = world:createBody({type = 'static'})
        if grabber then
            side.hand = physics2d.newGrabber(world, {strength = self.strength})
        end
        self.sides[index] = side
    end
    self.clock = 0
end

function Dragging:setStrength(value)
    self.strength = value
    self.sides[2].hand.strength = value
end

-- The left side builds the mouse joint itself, with a force of the same strength times the weight of the one body it holds.
function Dragging:take(side, x, y, radius)
    if side.grabber then
        side.hand.pickRadius = radius
        return side.hand:grab(x, y) ~= nil
    end
    for _, shape in ipairs(side.world:queryCircle(x, y, radius)) do
        local body = shape.body
        if body.type == 'dynamic' then
            local force = self.strength * body.mass * kStandardGravity * side.world.pixelsPerMeter
            side.joint = side.world:createJoint('mouse', side.anchor, body, {bx = x, by = y, maxMotorForce = force, hertz = 5, dampingRatio = 0.7})
            side.held = body
            return true
        end
    end
    return false
end

function Dragging:pull(side, x, y)
    if side.grabber then
        side.hand:moveTo(x, y)
    elseif side.joint then
        side.joint.target = {x, y}
    end
end

function Dragging:release(side)
    if side.grabber then
        side.hand:release()
    elseif side.joint then
        side.joint:destroy()
        side.joint, side.held = nil, nil
    end
end

function Dragging:holding(side)
    return side.grabber and side.hand.holding or side.joint ~= nil
end

function Dragging:update(dt)
    Dragging.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local pointer = self.pointer
    for _, side in ipairs(self.sides) do
        if pointer.pressed then
            side.manual = self:take(side, pointer.worldX, pointer.worldY, kReach / self.camera.zoom.x)
        elseif pointer.released and side.manual then
            side.manual = false
            self:release(side)
        end
        if side.manual then
            self:pull(side, pointer.worldX, pointer.worldY)
        end
    end
    local left, right = self.sides[1].doll:body('chest'), self.sides[2].doll:body('chest')
    self:status(string.format('Chest heights above the ground %.0f with the mouse joint and %.0f with the grabber, grabber force %.0f, step %.2f ms', 380 - left.y, 380 - right.y, self.sides[2].hand.force, self:stepTime()))
end

-- Every cycle both sides take the left hand of their ragdoll, pull it to the top of the stairs, hold it there a moment and let go.
function Dragging:fixedUpdate(step)
    self.clock = (self.clock + step) % kCycle
    for _, side in ipairs(self.sides) do
        if self.automatic and not side.manual then
            local hand = side.doll:body('lowerArmLeft')
            if self.clock < 3 then
                if not self:holding(side) then
                    self:take(side, hand.x, hand.y, 4)
                end
                local progress = math.min(1, self.clock / 2.4)
                self:pull(side, side.origin - 200 + progress * 460, 330 - progress * 290)
            elseif self:holding(side) then
                self:release(side)
            end
        end
        self:simulate(side.world, step)
    end
end

function Dragging:draw(area)
    graphics2d.drawLine(0, -430, 0, 430, 2, '#44FFFFFF')
    local titles = {'Mouse joint sized to the limb', 'Grabber sized to the figure'}
    for index, side in ipairs(self.sides) do
        graphics2d.drawText(nil, titles[index], side.origin, -390, {size = 28, color = index == 1 and '#FFFF8A84' or '#FF6FDCA0', anchor = {0.5, 0.5}})
        parts.drawAll(side.statics)
        parts.draw(side.crate, {layer = 1})
        side.world:debugDraw({layer = 2})
        if side.grabber and side.hand.holding then
            local handle, target = side.hand.handle, side.hand.target
            graphics2d.drawLine(handle.x, handle.y, target.x, target.y, 3, '#CCFFFFFF', {layer = 3})
        elseif side.joint and side.held.valid then
            local target = side.joint.target
            graphics2d.drawLine(side.held.x, side.held.y, target.x, target.y, 3, '#CCFFFFFF', {layer = 3})
        end
    end
end

return Dragging
