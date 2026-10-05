-- Jelly blobs made of a ring of small bodies held around a center by springy distance joints, which squash when they land and spring back, read in one call each with `world:readTransforms` and stiffened while they run.
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

local SoftBodies = haylen.class('SoftBodies', PhysicsTest)

local kNodes = 16
local kRadius = 70
local kNodeRadius = 9
local kMaxBlobs = 6
local kColors = {'#FF81C784', '#FFBA68C8', '#FFFFB74D', '#FF4DD0E1', '#FFE57373', '#FF64B5F6'}
local kRestArea = kNodes * kRadius ^ 2 * math.sin(math.pi * 2 / kNodes) / 2

function SoftBodies:enter()
    self:frame{
        hint = 'Drag a blob by its rim or its middle and throw it: it squashes on impact and springs back. The stiffness changes the springs of every blob while they run. R or the X button starts over.',
        controls = {
            ui.button{id = 'drop', text = 'Drop a blob', onClick = function() self:blob(self.random:range(-600, 500), -300) end},
            ui.label{text = 'Spring stiffness in hertz', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'stiffness', value = 5, min = 1, max = 10, step = 0.5, showValue = true, decimals = 1, onChange = function(event) self:setStiffness(event.value) end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        focus = 'drop',
    }
    self.random = m.random(52)
    self.hertz = 5
    self:build()
end

function SoftBodies:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.blobs, self.groups = {}, 0

    local ground = self.world:createBody({type = 'static'})
    parts.box(ground, 1600, 40, {offsetY = 410})
    parts.polygon(ground, {{-790, -40}, {-330, 250}, {-330, 390}, {-790, 390}})
    parts.box(ground, 180, 180, {offsetX = 520, offsetY = 300})
    for _, peg in ipairs({{-140, 120}, {20, 210}, {180, 120}}) do
        parts.circle(ground, 22, {offsetX = peg[1], offsetY = peg[2]})
    end
    self.statics = {ground}

    self:blob(-620, -280)
    self:blob(20, -300)
    self:blob(500, -260)
end

-- The parts of one blob share a negative group, so they never collide with each other, and every node pulls toward the center and toward its neighbors.
function SoftBodies:blob(x, y)
    self.groups = self.groups + 1
    local group = -self.groups
    local center = self.world:createBody({x = x, y = y})
    center:addCircle(22, {group = group})
    local nodes = {}
    for index = 1, kNodes do
        local angle = (index - 1) / kNodes * math.pi * 2
        local node = self.world:createBody({x = x + math.cos(angle) * kRadius, y = y + math.sin(angle) * kRadius})
        node:addCircle(kNodeRadius, {group = group, friction = 0.8})
        nodes[index] = node
    end
    local joints = {}
    local chord = 2 * kRadius * math.sin(math.pi / kNodes)
    for index, node in ipairs(nodes) do
        local neighbor = nodes[index % kNodes + 1]
        joints[#joints + 1] = self.world:createJoint('distance', center, node, {ax = center.x, ay = center.y, bx = node.x, by = node.y, enableSpring = true, hertz = self.hertz, dampingRatio = 0.4, enableLimit = true, lower = kRadius * 0.4, upper = kRadius * 1.25})
        joints[#joints + 1] = self.world:createJoint('distance', node, neighbor, {ax = node.x, ay = node.y, bx = neighbor.x, by = neighbor.y, enableSpring = true, hertz = self.hertz * 2, dampingRatio = 0.4, enableLimit = true, lower = chord * 0.5, upper = chord * 1.3})
    end
    nodes[kNodes + 1] = center

    local blob = {bodies = nodes, joints = joints, buffer = collections.newFloatBuffer((kNodes + 1) * 3), color = kColors[self.groups % #kColors + 1], squash = 0}
    self.world:readTransforms(nodes, blob.buffer)
    self.blobs[#self.blobs + 1] = blob
    if #self.blobs > kMaxBlobs then
        for _, body in ipairs(table.remove(self.blobs, 1).bodies) do
            body:destroy()
        end
    end
end

function SoftBodies:setStiffness(hertz)
    self.hertz = hertz
    for _, blob in ipairs(self.blobs) do
        for index, joint in ipairs(blob.joints) do
            joint.hertz = index % 2 == 1 and hertz or hertz * 2
        end
    end
end

function SoftBodies:update(dt)
    SoftBodies.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    local squash = 0
    for _, blob in ipairs(self.blobs) do
        self.world:readTransforms(blob.bodies, blob.buffer)
        local buffer, area = blob.buffer, 0
        for index = 0, kNodes - 1 do
            local following = (index + 1) % kNodes
            area = area + buffer[index * 3 + 1] * buffer[following * 3 + 2] - buffer[following * 3 + 1] * buffer[index * 3 + 2]
        end
        blob.squash = math.max(0, 1 - area / 2 / kRestArea)
        squash = math.max(squash, blob.squash)
    end
    self:status(string.format('Blobs %d, joints %d, stiffness %.1f hertz, most squashed %.0f%%, step %.2f ms', #self.blobs, #self.blobs * kNodes * 2, self.hertz, squash * 100, self:stepTime()))
end

function SoftBodies:fixedUpdate(step)
    self:simulate(self.world, step)
end

-- The skin passes outside every node, pushed away from the center by the radius of the node.
function SoftBodies:drawBlob(blob)
    local buffer = blob.buffer
    local cx, cy, rotation = buffer:get(kNodes * 3 + 1, 3)
    local skin = {}
    for index = 0, kNodes - 1 do
        local x, y = buffer[index * 3 + 1], buffer[index * 3 + 2]
        local dx, dy = x - cx, y - cy
        local length = math.max(math.sqrt(dx * dx + dy * dy), 0.001)
        skin[index + 1] = {x + dx / length * kNodeRadius, y + dy / length * kNodeRadius}
    end
    graphics2d.drawPolygon(skin, m.color(blob.color):lerp(m.color(1, 1, 1), blob.squash), {layer = 1})
    graphics2d.drawPolyline(skin, 4, '#66000000', true, {layer = 1})
    local cos, sin = math.cos(rotation), math.sin(rotation)
    for _, side in ipairs({-1, 1}) do
        local ex, ey = side * 16, -10
        local x, y = cx + ex * cos - ey * sin, cy + ex * sin + ey * cos
        graphics2d.drawCircle(x, y, 10, '#FFFFFFFF', {layer = 2})
        graphics2d.drawCircle(x + cos * 3, y + sin * 3, 5, '#FF263238', {layer = 2})
    end
end

function SoftBodies:draw(area)
    parts.drawAll(self.statics)
    for _, blob in ipairs(self.blobs) do
        self:drawBlob(blob)
    end
    self.grab:draw()
end

return SoftBodies
