-- A rope bridge of planks from `physics2d.newBridge` across a gorge between two grassy cliffs, with a rope rail on posts, loaded with crates, barrels and a heavy block.
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

local Bridge = haylen.class('Bridge', PhysicsTest)

Bridge.code = [[
local bridge = physics2d.newBridge(world, {from = {-560, -8}, to = {560, -8}, segments = 22, thickness = 16, density = 2, friction = 0.8})
for _, segment in ipairs(bridge:segments()) do
    plank(segment.x, segment.y, segment.rotation, segment.length)
end
local points = bridge:points()  -- The rope rail follows the ends of the planks.]]

Bridge.thickness = 16
Bridge.deck = -8
Bridge.rail = 54
Bridge.river = 330
Bridge.left = {{-1700, 0}, {-574, 0}, {-556, 18}, {-586, 120}, {-570, 230}, {-620, 420}, {-1700, 420}}
Bridge.right = {{574, 0}, {1700, 0}, {1700, 420}, {620, 420}, {570, 230}, {586, 120}, {556, 18}}
-- The far wall of the gorge fades from the haze of its top into the shadow of the depths.
Bridge.gorge = {{-640, 30}, {-430, 10}, {-250, 50}, {-60, 20}, {130, 60}, {340, 24}, {640, 40}}
Bridge.gorgeTop, Bridge.gorgeBottom = '#FFC6C5E4', '#FF5C5890'
Bridge.rock = Look.tint('stone', '#FF9F98C2', '#FF77709C', '#FFCAC4E2')

function Bridge:enter()
    self:frame{
        code = Bridge.code,
        hint = 'Drop loads on the bridge and drag them along it. R or the X button rebuilds the bridge.',
        controls = {
            ui.button{id = 'crate', text = 'Drop a crate', onClick = function() self:drop('crate') end},
            ui.button{id = 'barrel', text = 'Drop a barrel', onClick = function() self:drop('barrel') end},
            ui.button{id = 'block', text = 'Drop a heavy block', onClick = function() self:drop('block') end},
            ui.button{id = 'reset', text = 'Rebuild', onClick = function() self:build() end},
        },
        focus = 'crate',
    }
    self.random = m.random(11)
    self.leftGrass = Scenery.newGrass({{-1700, 0}, {-570, 0}})
    self.rightGrass = Scenery.newGrass({{570, 0}, {1700, 0}})
    self.cliffs = {Look.newShape(Bridge.left, Bridge.rock), Look.newShape(Bridge.right, Bridge.rock)}
    self.rail = {}
    self.gorge = {vertices = {}, indices = {}}
    for index, point in ipairs(Bridge.gorge) do
        self.gorge.vertices[#self.gorge.vertices + 1] = {x = point[1], y = point[2], color = Bridge.gorgeTop}
        self.gorge.vertices[#self.gorge.vertices + 1] = {x = point[1], y = 420, color = Bridge.gorgeBottom}
        if index > 1 then
            local a = index * 2 - 3
            for _, corner in ipairs({a, a + 1, a + 2, a + 1, a + 3, a + 2}) do
                self.gorge.indices[#self.gorge.indices + 1] = corner
            end
        end
    end
    self:build()
end

function Bridge:build()
    self.world = physics2d.newWorld({owner = self})
    self.grab = Grab(self.world)
    self.loads = {}

    for _, outline in ipairs({Bridge.left, Bridge.right}) do
        self.world:createBody({type = 'static'}):addPolygon(outline)
    end
    self.bridge = physics2d.newBridge(self.world, {from = {-560, Bridge.deck}, to = {560, Bridge.deck}, segments = 22, thickness = Bridge.thickness, density = 2, friction = 0.8})

    for index = 1, 3 do
        self:drop('crate', -300 + index * 150, -300)
    end
end

function Bridge:drop(kind, x, y)
    local body = self.world:createBody({x = x or self.random:range(-400, 400), y = y or -340, rotation = self.random:range(-0.3, 0.3)})
    if kind == 'crate' then
        body:addBox(70, 70)
        Look.dress(body, 'woodCrate')
    elseif kind == 'barrel' then
        body:addCircle(32)
        Look.dress(body, 'darkWood', {spin = true})
    else
        body:addBox(140, 80, {density = 6})
        Look.dress(body, 'iron')
    end
    self.loads[#self.loads + 1] = body
end

function Bridge:update(dt)
    Bridge.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    local lowest = Bridge.deck
    for _, point in ipairs(self.bridge:points()) do
        lowest = math.max(lowest, point.y)
    end
    self:status(string.format('%d planks hold %d loads and sag %d units in the middle.', #self.bridge:bodies(), #self.loads, math.floor(lowest - Bridge.deck + 0.5)))
end

function Bridge:fixedUpdate(step)
    self:simulate(self.world, step)
end

-- The rail is a rope on the posts at both ends that follows the planks above them, with a hanger down to every other plank.
function Bridge:drawRail(points)
    local rail = self.rail
    local unit = Look.pixel
    for index, point in ipairs(points) do
        rail[index * 2 - 1], rail[index * 2] = point.x, point.y - Bridge.rail
        if index % 2 == 1 and index > 1 and index < #points then
            Look.line(point.x, point.y - Bridge.rail, point.x, point.y - Bridge.thickness / 2, 3 * unit, '#FFE8C78E', 0)
        end
    end
    Look.rope(rail, 9, 1)
    for _, x in ipairs({-584, 584}) do
        Look.block(x, Bridge.deck - Bridge.rail / 2 - 4, 16, Bridge.rail + 20, 'darkWood', 2)
    end
end

function Bridge:draw(area)
    Scenery.sky(self.camera, 30)
    graphics2d.drawMesh(nil, self.gorge.vertices, self.gorge.indices, {layer = -14})
    Scenery.water(-900, 900, Bridge.river, {layer = -12})
    for _, cliff in ipairs(self.cliffs) do
        Look.drawShape(cliff, -8)
    end
    Scenery.drawGrass(self.leftGrass, -7)
    Scenery.drawGrass(self.rightGrass, -7)
    for _, segment in ipairs(self.bridge:segments()) do
        Look.block(segment.x, segment.y, segment.length + 1, Bridge.thickness, 'wood', 1, segment.rotation)
    end
    self:drawRail(self.bridge:points())
    Look.drawAll(self.loads, {layer = 2})
    self.grab:draw()
end

return Bridge
