-- Jelly blobs made of a ring of small bodies held around a center by springy distance joints, which squash when they land and spring back, read in one call each with `world:readTransforms` and stiffened while they run. Their skin is a smooth curve through the ring, and they bounce down a grassy slope, over wooden posts and onto a stone block.
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local SoftBodies = haylen.class('SoftBodies', PhysicsTest)

SoftBodies.code = [[
node:addCircle(9, {group = -1, friction = 0.8})  -- The parts of one blob never collide.
world:createJoint('distance', center, node, {ax = center.x, ay = center.y, bx = node.x, by = node.y, enableSpring = true, hertz = 5, dampingRatio = 0.4, enableLimit = true, lower = 28, upper = 87.5})
world:createJoint('distance', node, neighbor, {ax = node.x, ay = node.y, bx = neighbor.x, by = neighbor.y, enableSpring = true, hertz = 10, dampingRatio = 0.4})
world:readTransforms(blob, buffer)  -- Every node of the blob in one call.
joint.hertz = 8  -- Stiffens the spring while it runs.]]

SoftBodies.nodes = 16
SoftBodies.radius = 70
SoftBodies.nodeRadius = 9
-- Each stretch of the skin between two nodes bends through this many points.
SoftBodies.smoothness = 4
SoftBodies.maxBlobs = 6
SoftBodies.ground = 390
SoftBodies.slope = {{-1600, -40}, {-800, -40}, {-220, 390}, {1600, 390}}
SoftBodies.posts = {{-80, 150}, {80, 230}, {240, 150}}
SoftBodies.block = {520, 300, 180}
SoftBodies.restArea = SoftBodies.nodes * SoftBodies.radius ^ 2 * math.sin(math.pi * 2 / SoftBodies.nodes) / 2

function SoftBodies:enter()
    self:frame{
        code = SoftBodies.code,
        hint = 'Drag a blob by its rim or its middle and throw it: it squashes on impact and springs back. The stiffness changes the springs of every blob while they run. R or the X button starts over.',
        controls = {
            ui.button{id = 'drop', text = 'Drop a blob', onClick = function() self:blob(self.random:range(-600, 500), -260) end},
            ui.label{text = 'Spring stiffness in hertz', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'stiffness', value = 5, min = 1, max = 10, step = 0.5, showValue = true, decimals = 1, onChange = function(event) self:setStiffness(event.value) end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        focus = 'drop',
        view = {1600, 720},
    }
    self.camera.position = {0, 70}
    self.random = m.random(52)
    self.hertz = 5
    self.terrain = Scenery.newTerrain(SoftBodies.slope)
    self.skinStroke = {width = 1, color = Look.ink, join = 'round', closed = true, layer = 1}
    self.eyeShape = {kind = 'ellipse', color = '#FFFFFFFF', borderWidth = 0, borderColor = Look.ink, layer = 2}
    self.eyeRect = {0, 0, 0, 0}
    self:build()
end

function SoftBodies:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.blobs, self.groups = {}, 0

    local ground = self.world:createBody({type = 'static'})
    ground:addChain(SoftBodies.slope, false, {friction = 0.8})
    local block = SoftBodies.block
    ground:addBox(block[3], block[3], {offsetX = block[1], offsetY = block[2]})
    for _, post in ipairs(SoftBodies.posts) do
        ground:addCircle(22, {offsetX = post[1], offsetY = post[2]})
    end

    self:blob(-620, -240)
    self:blob(80, -260)
    self:blob(500, -220)
end

-- The parts of one blob share a negative group, so they never collide with each other, and every node pulls toward the center and toward its neighbors.
function SoftBodies:blob(x, y)
    local count, radius = SoftBodies.nodes, SoftBodies.radius
    self.groups = self.groups + 1
    local group = -self.groups
    local center = self.world:createBody({x = x, y = y})
    center:addCircle(22, {group = group})
    local nodes = {}
    for index = 1, count do
        local angle = (index - 1) / count * math.pi * 2
        local node = self.world:createBody({x = x + math.cos(angle) * radius, y = y + math.sin(angle) * radius})
        node:addCircle(SoftBodies.nodeRadius, {group = group, friction = 0.8})
        nodes[index] = node
    end
    local joints = {}
    local chord = 2 * radius * math.sin(math.pi / count)
    for index, node in ipairs(nodes) do
        local neighbor = nodes[index % count + 1]
        joints[#joints + 1] = self.world:createJoint('distance', center, node, {ax = center.x, ay = center.y, bx = node.x, by = node.y, enableSpring = true, hertz = self.hertz, dampingRatio = 0.4, enableLimit = true, lower = radius * 0.4, upper = radius * 1.25})
        joints[#joints + 1] = self.world:createJoint('distance', node, neighbor, {ax = node.x, ay = node.y, bx = neighbor.x, by = neighbor.y, enableSpring = true, hertz = self.hertz * 2, dampingRatio = 0.4, enableLimit = true, lower = chord * 0.5, upper = chord * 1.3})
    end
    nodes[count + 1] = center

    -- The skin keeps its points from frame to frame: the ring around the nodes, the smooth curve through it, the lit part inside it and the flat list the outline strokes.
    local points = count * SoftBodies.smoothness
    local blob = {bodies = nodes, joints = joints, buffer = collections.newFloatBuffer((count + 1) * 3), material = Look.toys[self.groups % #Look.toys + 1], squash = 0, ring = {}, skin = {}, lit = {}, outline = {}}
    for index = 1, count do
        blob.ring[index] = {0, 0}
    end
    for index = 1, points do
        blob.skin[index], blob.lit[index] = {0, 0}, {0, 0}
        blob.outline[index * 2 - 1], blob.outline[index * 2] = 0, 0
    end
    self.world:readTransforms(nodes, blob.buffer)
    self.blobs[#self.blobs + 1] = blob
    if #self.blobs > SoftBodies.maxBlobs then
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
    local count = SoftBodies.nodes
    local squash = 0
    for _, blob in ipairs(self.blobs) do
        self.world:readTransforms(blob.bodies, blob.buffer)
        local buffer, area = blob.buffer, 0
        for index = 0, count - 1 do
            local following = (index + 1) % count
            area = area + buffer[index * 3 + 1] * buffer[following * 3 + 2] - buffer[following * 3 + 1] * buffer[index * 3 + 2]
        end
        blob.squash = math.max(0, 1 - area / 2 / SoftBodies.restArea)
        squash = math.max(squash, blob.squash)
    end
    local most = math.floor(squash * 100 + 0.5)
    self:status(string.format('%d blobs hold together with %d springs of %.1f hertz, and the most squashed one has lost %d percent of its area.', #self.blobs, #self.blobs * count * 2, self.hertz, most))
end

function SoftBodies:fixedUpdate(step)
    self:simulate(self.world, step)
end

-- Fills `skin` with a Catmull-Rom curve through the points of `ring`, a closed loop, and `outline` with the same points as a flat list.
function SoftBodies.smooth(ring, skin, outline)
    local count, steps = #ring, SoftBodies.smoothness
    for index = 1, count do
        local p0, p1 = ring[(index - 2) % count + 1], ring[index]
        local p2, p3 = ring[index % count + 1], ring[(index + 1) % count + 1]
        for step = 0, steps - 1 do
            local t = step / steps
            local t2, t3 = t * t, t * t * t
            local point = skin[(index - 1) * steps + step + 1]
            for axis = 1, 2 do
                point[axis] = 0.5 * (2 * p1[axis] + (p2[axis] - p0[axis]) * t + (2 * p0[axis] - 5 * p1[axis] + 4 * p2[axis] - p3[axis]) * t2 + (3 * p1[axis] - p0[axis] - 3 * p2[axis] + p3[axis]) * t3)
            end
            outline[((index - 1) * steps + step) * 2 + 1], outline[((index - 1) * steps + step) * 2 + 2] = point[1], point[2]
        end
    end
end

-- The skin passes outside every node, pushed away from the center by the radius of the node, as a smooth curve with the bold outline, its shaded side away from the light, a lit side toward it, a shine and a pair of eyes that turn with the blob.
function SoftBodies:drawBlob(blob)
    local unit = Look.pixel
    local count = SoftBodies.nodes
    local buffer, material = blob.buffer, blob.material
    local cx, cy, rotation = buffer:get(count * 3 + 1, 3)
    for index = 0, count - 1 do
        local x, y = buffer[index * 3 + 1], buffer[index * 3 + 2]
        local dx, dy = x - cx, y - cy
        local length = math.max(math.sqrt(dx * dx + dy * dy), 0.001)
        local point = blob.ring[index + 1]
        point[1], point[2] = x + dx / length * SoftBodies.nodeRadius, y + dy / length * SoftBodies.nodeRadius
    end
    SoftBodies.smooth(blob.ring, blob.skin, blob.outline)
    for index, point in ipairs(blob.skin) do
        local lit = blob.lit[index]
        lit[1], lit[2] = cx + (point[1] - cx) * 0.84 - 5, cy + (point[2] - cy) * 0.84 - 6
    end
    self.skinStroke.width = Look.outlinePixels * unit * 2
    graphics2d.drawStroke(blob.outline, self.skinStroke)
    graphics2d.drawPolygon(blob.skin, material.shade, {layer = 1})
    graphics2d.drawPolygon(blob.lit, material.fill, {layer = 1})
    graphics2d.drawShape({cx - 44, cy - 50, 30, 18}, {kind = 'ellipse', rotation = -0.6, color = material.light, layer = 1})

    local cos, sin = math.cos(rotation), math.sin(rotation)
    local shape, rect = self.eyeShape, self.eyeRect
    for _, side in ipairs({-1, 1}) do
        local ex, ey = side * 16, -8
        local x, y = cx + ex * cos - ey * sin, cy + ex * sin + ey * cos
        rect[1], rect[2], rect[3], rect[4] = x - 9, y - 11, 18, 22
        shape.color, shape.borderWidth = '#FFFFFFFF', 2.5 * unit
        graphics2d.drawShape(rect, shape)
        rect[1], rect[2], rect[3], rect[4] = x - 4 + cos * 2, y - 5 + sin * 2, 8, 10
        shape.color, shape.borderWidth = Look.ink, 0
        graphics2d.drawShape(rect, shape)
    end
end

function SoftBodies:draw(area)
    Scenery.sky(self.camera, SoftBodies.ground)
    Scenery.drawTerrain(self.terrain)
    local block = SoftBodies.block
    Look.block(block[1], block[2], block[3], block[3], 'stone', 0)
    for _, post in ipairs(SoftBodies.posts) do
        Look.block(post[1], (post[2] + SoftBodies.ground) / 2 + 10, 16, SoftBodies.ground - post[2] - 10, 'darkWood', 0)
        Look.disc(post[1], post[2], 22, 'steel', 0)
    end
    for _, blob in ipairs(self.blobs) do
        self:drawBlob(blob)
    end
    self.grab:draw()
end

return SoftBodies
