-- A rope net of small bodies in a grid joined by distance joints and hung from a wooden bar, which catches what falls into it and tears where a joint passes its `breakForce`, as `world.onJointBreak` reports. Two balls fall into it when the test opens.
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Burst = require('categories.physics.burst')
local Grab = require('categories.physics.grab')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Cloth = haylen.class('Cloth', PhysicsTest)

Cloth.code = [[
knot:addCircle(6, {group = -1, density = 0.5})
world:createJoint('distance', a, b, {ax = a.x, ay = a.y, bx = b.x, by = b.y, breakForce = 3000})
world:createJoint('revolute', bar, knot, {ax = knot.x, ay = knot.y, breakForce = 9000})  -- Hangs the top row.
world.onJointBreak = function(joint, info) tear(info.bodyB.x, info.bodyB.y) end
world:readTransforms(knots, buffer)  -- Every knot in one call to draw the threads.]]

Cloth.columns, Cloth.rows, Cloth.spacing = 31, 12, 30
Cloth.left, Cloth.top = -450, -260
Cloth.ground = 390
Cloth.hangEvery = 5
Cloth.hangStrength = 3
Cloth.maxLoads = 12
Cloth.thread = 5
Cloth.firstLoads = {{0.6, 'ball', -120}, {1.6, 'ball', 140}}

function Cloth:enter()
    self:frame{
        code = Cloth.code,
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
    self.camera.position = {0, 30}
    self.random = m.random(53)
    self.breakForce = 3000
    self.snaps = Burst('hits/hit-light', {count = 6, scale = 0.4, layer = 30})
    self:build()
    for _, load in ipairs(Cloth.firstLoads) do
        timer.after(load[1], function() self:drop(load[2], load[3]) end, {owner = self})
    end
end

function Cloth:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.loads, self.torn = {}, 0

    local ground = self.world:createBody({type = 'static', x = 0, y = Cloth.ground + 20})
    ground:addBox(4000, 40)
    self.bar = self.world:createBody({type = 'static'})

    self.nodes, self.links, self.hooks = {}, {}, {}
    local columns, spacing = Cloth.columns, Cloth.spacing
    for row = 0, Cloth.rows - 1 do
        for column = 0, columns - 1 do
            local node = self.world:createBody({x = Cloth.left + column * spacing, y = Cloth.top + row * spacing, linearDamping = 0.3})
            node:addCircle(6, {group = -1, density = 0.5})
            node.data = {index = #self.nodes + 1}
            self.nodes[#self.nodes + 1] = node
            if column > 0 then
                self:link(#self.nodes - 1, #self.nodes, self.breakForce)
            end
            if row > 0 then
                self:link(#self.nodes - columns, #self.nodes, self.breakForce)
            end
            if row == 0 and column % Cloth.hangEvery == 0 then
                self:link(0, #self.nodes, self.breakForce * Cloth.hangStrength, self.bar)
                self.hooks[#self.hooks + 1] = node.x
            end
        end
    end
    self.buffer = collections.newFloatBuffer(#self.nodes * 3)
    self.knots = {}
    for index = 1, #self.nodes * 2 do
        self.knots[index] = 0
    end

    self.world.onJointBreak = function(joint, info)
        local a, b = info.bodyA.data and info.bodyA.data.index or 0, info.bodyB.data.index
        local link = self.links[math.min(a, b) * 10000 + math.max(a, b)]
        link.broken = true
        self.torn = self.torn + 1
        self.snaps:fire(info.bodyB.x, info.bodyB.y)
    end
end

-- Joins two knots, or the bar and a knot, and files the link under the pair of indices that the break callback finds it by, where the bar counts as index 0.
function Cloth:link(first, second, breakForce, bar)
    local a, b = bar or self.nodes[first], self.nodes[second]
    local joint
    if bar then
        joint = self.world:createJoint('revolute', a, b, {ax = b.x, ay = b.y, breakForce = breakForce})
    else
        joint = self.world:createJoint('distance', a, b, {ax = a.x, ay = a.y, bx = b.x, by = b.y, breakForce = breakForce})
    end
    self.links[first * 10000 + second] = {joint = joint, first = first, second = second, hook = bar ~= nil, broken = false}
end

function Cloth:setBreakForce(force)
    self.breakForce = force
    for _, link in pairs(self.links) do
        if not link.broken then
            link.joint.breakForce = link.hook and force * Cloth.hangStrength or force
        end
    end
end

function Cloth:drop(kind, x)
    local body = self.world:createBody({x = x or self.random:range(-350, 350), y = -420, rotation = self.random:range(-0.4, 0.4)})
    if kind == 'ball' then
        body:addCircle(26, {restitution = 0.3})
        Look.dress(body, 'red', {spin = true})
    else
        body:addBox(70, 70, {density = 3})
        Look.dress(body, 'woodCrate')
    end
    self.loads[#self.loads + 1] = body
    if #self.loads > Cloth.maxLoads then
        table.remove(self.loads, 1):destroy()
    end
end

function Cloth:update(dt)
    Cloth.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    self.snaps:update(dt)
    local torn = self.torn == 0 and 'none has torn yet' or string.format('%d %s torn', self.torn, self.torn == 1 and 'has' or 'have')
    self:status(string.format('The net holds %d knots on threads that snap past a force of %d, and %s.', #self.nodes, self.breakForce, torn))
end

function Cloth:fixedUpdate(step)
    self:simulate(self.world, step)
end

-- The net hangs from a wooden bar on two posts, with a steel hook wherever a knot of its top row hangs.
function Cloth:drawFrame()
    local left, right = Cloth.left - 70, Cloth.left + (Cloth.columns - 1) * Cloth.spacing + 70
    local bar = Cloth.top - 22
    for _, x in ipairs({left, right}) do
        Look.block(x, (bar + Cloth.ground) / 2, 26, Cloth.ground - bar, 'darkWood', -2)
    end
    Look.block((left + right) / 2, bar, right - left + 40, 24, 'darkWood', -1)
    for _, x in ipairs(self.hooks) do
        Look.line(x, bar + 8, x, Cloth.top, 4, '#FFA9BCD3', -1)
        Look.pin(x, bar, 7, -1)
    end
end

-- Every thread that holds is a rope with a dark edge, and the knots sit on top as small dark beads.
function Cloth:drawNet()
    local unit = Look.pixel
    local buffer, knots = self.buffer, self.knots
    local edge = Cloth.thread + 3 * unit
    for _, link in pairs(self.links) do
        if not link.broken and not link.hook then
            local a, b = (link.first - 1) * 3, (link.second - 1) * 3
            graphics2d.drawLine(buffer[a + 1], buffer[a + 2], buffer[b + 1], buffer[b + 2], edge, '#FF7A4724', {layer = 0})
        end
    end
    for _, link in pairs(self.links) do
        if not link.broken and not link.hook then
            local a, b = (link.first - 1) * 3, (link.second - 1) * 3
            graphics2d.drawLine(buffer[a + 1], buffer[a + 2], buffer[b + 1], buffer[b + 2], Cloth.thread, '#FFE6B877', {layer = 0})
        end
    end
    for index = 0, #self.nodes - 1 do
        knots[index * 2 + 1], knots[index * 2 + 2] = buffer[index * 3 + 1], buffer[index * 3 + 2]
    end
    graphics2d.drawPoints(knots, {radius = 4, color = '#FFB8743A', layer = 0})
end

function Cloth:draw(area)
    Scenery.sky(self.camera, Cloth.ground)
    Scenery.ground(Cloth.ground)
    self:drawFrame()
    self.world:readTransforms(self.nodes, self.buffer)
    self:drawNet()
    Look.drawAll(self.loads, {layer = 2})
    self.snaps:draw()
    self.grab:draw()
end

return Cloth
