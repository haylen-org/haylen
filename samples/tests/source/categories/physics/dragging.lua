-- Dragging a ragdoll up the stairs with the same strength in two worlds. On the left a mouse joint pulls with a force sized to the one limb it holds, which drags the figure along the steps without lifting it, and on the right `physics2d.newGrabber` sizes the pull to the limb and everything joined to it, so the figure follows the pointer like a crate does. The walls of the two worlds meet in one stone pillar between the sides.
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Figure = require('categories.physics.figure')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Dragging = haylen.class('Dragging', PhysicsTest)

Dragging.code = [[
local hand = physics2d.newGrabber(world, {strength = 8})
hand:grab(x, y)
hand:moveTo(x, y)  -- Lifts the limb and everything joined to it.
local force = 8 * limb.mass * 9.8 * world.pixelsPerMeter  -- One limb only.
local joint = world:createJoint('mouse', anchor, limb, {bx = x, by = y, maxMotorForce = force})
joint.target = {x, y}]]

Dragging.strength = 8
Dragging.standardGravity = 9.80665
Dragging.steps, Dragging.stepWidth, Dragging.stepHeight = 4, 80, 64
Dragging.stairLeft = -110
Dragging.cycle = 5
Dragging.reach = 28
Dragging.ground = 360
Dragging.height = 230
Dragging.stiffness = 0.1
-- The pillars at the sides and between them reach up to this height, and the walls of the worlds reach far above them.
Dragging.pillarTop = -230

function Dragging:enter()
    self:frame{
        code = Dragging.code,
        hint = 'Drag a ragdoll or the crate on either side. Every five seconds both sides pull the hand of their ragdoll to the top of the stairs by themselves. R or the X button starts over.',
        controls = {
            ui.checkbox{id = 'automatic', text = 'Pull the hands by themselves', checked = true, onChange = function(event) self.automatic = event.checked end},
            ui.label{text = 'Strength, in times the weight the hand holds', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'strength', value = Dragging.strength, min = 2, max = 80, step = 1, showValue = true, decimals = 0, onChange = function(event) self:setStrength(event.value) end},
            ui.checkbox{id = 'outlines', text = 'Show the capsules', onChange = function(event) self.outlines = event.checked end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        focus = 'automatic',
        view = {1580, 700},
    }
    self.camera.position = {0, 20}
    self.automatic = true
    self.strength = Dragging.strength
    self.rope = {0, 0, 0, 0}
    self:build()
end

function Dragging:build()
    self.halves = {}
    for index, grabber in ipairs({false, true}) do
        local origin = index == 1 and -400 or 400
        local world = physics2d.newWorld({owner = self})
        local ground = world:createBody({type = 'static', x = origin, y = Dragging.ground + 30})
        ground:addBox(800, 60)
        ground:addBox(20, 800, {offsetX = -380, offsetY = -430})
        ground:addBox(20, 800, {offsetX = 380, offsetY = -430})
        -- The stairs rise to the right, and the top step reaches the edge of the side.
        local steps = {}
        for step = 1, Dragging.steps do
            local left = origin + Dragging.stairLeft + (step - 1) * Dragging.stepWidth
            local right = step == Dragging.steps and origin + 370 or left + Dragging.stepWidth
            local top = Dragging.ground - step * Dragging.stepHeight
            local stair = world:createBody({type = 'static', x = (left + right) / 2, y = (top + Dragging.ground) / 2})
            stair:addBox(right - left, Dragging.ground - top, {friction = 0.8})
            steps[step] = {left = left, right = right, top = top}
        end
        local half = {world = world, origin = origin, steps = steps, grabber = grabber}
        half.doll = physics2d.newRagdoll(world, {x = origin - 250, y = Dragging.ground - Dragging.height * 0.42, height = Dragging.height, stiffness = Dragging.stiffness})
        half.outfit = Figure.dress(half.doll, index)
        local top = steps[Dragging.steps].top
        half.crate = world:createBody({x = origin + 300, y = top - 30})
        half.crate:addBox(56, 56, {density = 2})
        Look.dress(half.crate, 'woodCrate')
        half.anchor = world:createBody({type = 'static'})
        if grabber then
            half.hand = physics2d.newGrabber(world, {strength = self.strength})
        end
        self.halves[index] = half
    end
    self.clock = 0
end

function Dragging:setStrength(value)
    self.strength = value
    self.halves[2].hand.strength = value
end

-- The left side builds the mouse joint itself, with a force of the same strength times the weight of the one body it holds.
function Dragging:take(half, x, y, radius)
    if half.grabber then
        half.hand.pickRadius = radius
        return half.hand:grab(x, y) ~= nil
    end
    for _, shape in ipairs(half.world:queryCircle(x, y, radius)) do
        local body = shape.body
        if body.type == 'dynamic' then
            local force = self.strength * body.mass * Dragging.standardGravity * half.world.pixelsPerMeter
            half.joint = half.world:createJoint('mouse', half.anchor, body, {bx = x, by = y, maxMotorForce = force, hertz = 5, dampingRatio = 0.7})
            half.held = body
            return true
        end
    end
    return false
end

function Dragging:pull(half, x, y)
    if half.grabber then
        half.hand:moveTo(x, y)
    elseif half.joint then
        half.joint.target = {x, y}
    end
end

function Dragging:release(half)
    if half.grabber then
        half.hand:release()
    elseif half.joint then
        half.joint:destroy()
        half.joint, half.held = nil, nil
    end
end

function Dragging:holding(half)
    return half.grabber and half.hand.holding or half.joint ~= nil
end

function Dragging:update(dt)
    Dragging.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local pointer = self.pointer
    for _, half in ipairs(self.halves) do
        if pointer.pressed then
            half.manual = self:take(half, pointer.worldX, pointer.worldY, Dragging.reach / self.camera.zoom.x)
        elseif pointer.released and half.manual then
            half.manual = false
            self:release(half)
        end
        if half.manual then
            self:pull(half, pointer.worldX, pointer.worldY)
        end
    end
    local left, right = self.halves[1].doll:body('chest'), self.halves[2].doll:body('chest')
    self:status(string.format('The chest of the figure is %d units above the ground with the mouse joint and %d with the grabber.', math.floor(Dragging.ground - left.y + 0.5), math.floor(Dragging.ground - right.y + 0.5)))
end

-- Every cycle both sides take the near hand of their ragdoll, pull it to the top of the stairs, hold it there a moment and let go.
function Dragging:fixedUpdate(step)
    self.clock = (self.clock + step) % Dragging.cycle
    for _, half in ipairs(self.halves) do
        if self.automatic and not half.manual then
            local hand = half.doll:body('lowerArmRight')
            if self.clock < 3 then
                if not self:holding(half) then
                    self:take(half, hand.x, hand.y, 4)
                end
                local progress = math.min(1, self.clock / 2.4)
                local top = half.steps[Dragging.steps].top
                self:pull(half, half.origin - 230 + progress * 430, Dragging.ground - 60 - progress * (Dragging.ground - top + 70))
            elseif self:holding(half) then
                self:release(half)
            end
        end
        self:simulate(half.world, step)
    end
end

-- The pull draws as a rope from the hand that holds to where it pulls.
function Dragging:drawPull(half)
    local fromX, fromY, toX, toY
    if half.grabber and half.hand.holding then
        local handle, target = half.hand.handle, half.hand.target
        fromX, fromY, toX, toY = handle.x, handle.y, target.x, target.y
    elseif half.joint and half.held.valid then
        local target = half.joint.target
        fromX, fromY, toX, toY = half.held.x, half.held.y, target.x, target.y
    end
    if fromX then
        local unit = Look.pixel
        self.rope[1], self.rope[2], self.rope[3], self.rope[4] = fromX, fromY, toX, toY
        Look.rope(self.rope, 7, 20)
        Look.pin(toX, toY, 10 * unit, 21, '#FFFFC93C')
    end
end

function Dragging:draw(area)
    Scenery.sky(self.camera, Dragging.ground)
    Scenery.ground(Dragging.ground)
    self:compare('A mouse joint sized to the limb', 'A grabber sized to the figure', 0, -300)
    for index, half in ipairs(self.halves) do
        self:drawStairs(half)
        Look.draw(half.crate, {layer = 1})
        Figure.draw(half.doll, half.outfit, 2 + index)
        if self.outlines then
            half.world:debugDraw({layer = 10})
        end
        self:drawPull(half)
    end
    Dragging.drawPillar(-785, 30)
    Dragging.drawPillar(0, 60)
    Dragging.drawPillar(785, 30)
end

-- The stairs are laid of stone blocks in rows a step high whose joints alternate, each row reaching out to the edge of the side.
function Dragging:drawStairs(half)
    local width, height = Dragging.stepWidth, Dragging.stepHeight
    local right = half.steps[Dragging.steps].right
    for row = 1, Dragging.steps do
        local y = Dragging.ground - (row - 0.5) * height
        local x = half.steps[row].left
        if row % 2 == 0 then
            Look.block(x + width / 4, y, width / 2, height, 'stone', 0)
            x = x + width / 2
        end
        while x < right - 1 do
            local block = math.min(width, right - x)
            Look.block(x + block / 2, y, block, height, 'stone', 0)
            x = x + block
        end
    end
end

-- A stone pillar with a cap, from the ground up to the height of the pillars.
function Dragging.drawPillar(x, width)
    local top, ground = Dragging.pillarTop, Dragging.ground
    Look.block(x, (top + ground) / 2, width, ground - top, 'stone', 0)
    Look.block(x, top - 10, width + 24, 22, 'stone', 0)
end

return Dragging
