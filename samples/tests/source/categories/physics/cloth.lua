-- A net of small bodies in a grid joined by distance joints and pinned at the top, which catches what falls into it and tears where a joint passes its `breakForce`, as `world.onJointBreak` reports.
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Cloth = haylen.class('Cloth', PhysicsTest)

local kColumns, kRows, kSpacing = 21, 10, 30
local kLeft, kTop = -300, -300
local kPinEvery = 5
local kPinStrength = 3
local kTearLife = 0.6
local kMaxLoads = 12

function Cloth:enter()
    self:frame{
        hint = 'Drop balls and crates into the net or drag a knot to pull it: a joint that holds more than the break force snaps, and the net tears there. R or the X button weaves it again.',
        controls = {
            ui.button{id = 'ball', text = 'Drop a ball', onClick = function() self:drop('ball') end},
            ui.button{id = 'crate', text = 'Drop a heavy crate', onClick = function() self:drop('crate') end},
            ui.label{text = 'Break force of the knots', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'breakForce', value = 3000, min = 1000, max = 8000, step = 100, showValue = true, decimals = 0, onChange = function(event) self:setBreakForce(event.value) end},
            ui.button{id = 'reset', text = 'Weave it again', onClick = function() self:build() end},
        },
        focus = 'ball',
    }
    self.random = m.random(53)
    self.breakForce = 3000
    self:build()
end

function Cloth:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.loads, self.tears, self.torn = {}, {}, 0

    local ground = self.world:createBody({type = 'static', x = 0, y = 410})
    parts.box(ground, 1600, 40)
    self.statics = {ground}

    self.nodes, self.links = {}, {}
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            local node = self.world:createBody({x = kLeft + column * kSpacing, y = kTop + row * kSpacing, linearDamping = 0.3})
            node:addCircle(6, {group = -1, density = 0.5})
            node.data = {index = #self.nodes + 1}
            self.nodes[#self.nodes + 1] = node
            if column > 0 then
                self:link(#self.nodes - 1, #self.nodes, self.breakForce)
            end
            if row > 0 then
                self:link(#self.nodes - kColumns, #self.nodes, self.breakForce)
            end
            if row == 0 and column % kPinEvery == 0 then
                local peg = self.world:createBody({type = 'static', x = node.x, y = node.y})
                parts.circle(peg, 10)
                self.statics[#self.statics + 1] = peg
                self:link(0, #self.nodes, self.breakForce * kPinStrength, peg)
            end
        end
    end
    self.buffer = collections.newFloatBuffer(#self.nodes * 3)

    self.world.onJointBreak = function(joint, info)
        local a, b = info.bodyA.data.index or 0, info.bodyB.data.index
        local link = self.links[math.min(a, b) * 10000 + math.max(a, b)]
        link.broken = true
        self.torn = self.torn + 1
        self.tears[#self.tears + 1] = {x = info.bodyB.x, y = info.bodyB.y, life = kTearLife}
    end
end

-- Joins two knots, or a peg and a knot, and files the link under the pair of indices that the break callback finds it by, where a peg counts as index 0.
function Cloth:link(first, second, breakForce, peg)
    local a, b = peg or self.nodes[first], self.nodes[second]
    local joint
    if peg then
        joint = self.world:createJoint('revolute', a, b, {ax = b.x, ay = b.y, breakForce = breakForce})
    else
        joint = self.world:createJoint('distance', a, b, {ax = a.x, ay = a.y, bx = b.x, by = b.y, breakForce = breakForce})
    end
    self.links[first * 10000 + second] = {joint = joint, first = first, second = second, pin = peg ~= nil, broken = false}
end

function Cloth:setBreakForce(force)
    self.breakForce = force
    for _, link in pairs(self.links) do
        if not link.broken then
            link.joint.breakForce = link.pin and force * kPinStrength or force
        end
    end
end

function Cloth:drop(kind)
    local body = self.world:createBody({x = self.random:range(-250, 250), y = -420, rotation = self.random:range(-0.4, 0.4)})
    if kind == 'ball' then
        parts.circle(body, 26, {restitution = 0.3})
    else
        parts.box(body, 70, 70, {density = 3})
        parts.paint(body, '#FF8D6E63')
    end
    self.loads[#self.loads + 1] = body
    if #self.loads > kMaxLoads then
        table.remove(self.loads, 1):destroy()
    end
end

function Cloth:update(dt)
    Cloth.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    for index = #self.tears, 1, -1 do
        local tear = self.tears[index]
        tear.life = tear.life - dt
        if tear.life <= 0 then
            table.remove(self.tears, index)
        end
    end
    self:status(string.format('Knots %d, joints torn %d, break force %.0f, step %.2f ms', #self.nodes, self.torn, self.breakForce, self:stepTime()))
end

function Cloth:fixedUpdate(step)
    self:simulate(self.world, step)
end

function Cloth:draw(area)
    parts.drawAll(self.statics)
    self.world:readTransforms(self.nodes, self.buffer)
    local buffer = self.buffer
    for _, link in pairs(self.links) do
        if not link.broken and not link.pin then
            local a, b = (link.first - 1) * 3, (link.second - 1) * 3
            graphics2d.drawLine(buffer[a + 1], buffer[a + 2], buffer[b + 1], buffer[b + 2], 3, '#FFE0E0E0')
        end
    end
    for index = 0, #self.nodes - 1 do
        graphics2d.drawCircle(buffer[index * 3 + 1], buffer[index * 3 + 2], 4, '#FFFFD54F', {layer = 1})
    end
    parts.drawAll(self.loads, {layer = 2})
    for _, tear in ipairs(self.tears) do
        local age = 1 - tear.life / kTearLife
        graphics2d.drawRing(tear.x, tear.y, 8 + age * 30, 3, m.color(1, 0.4, 0.3, 1 - age), {layer = 3})
    end
    self.grab:draw()
end

return Cloth
