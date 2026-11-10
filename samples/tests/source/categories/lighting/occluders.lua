-- Occluders built from data: the walls of a Tiled object layer and the shapes of physics bodies, whose occluders follow them every frame. The walls are stone over a brick hall, the bodies wooden crates.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local tiled = require('haylen.tiled')
local ui = require('haylen.ui')

local LightingTest = require('categories.lighting.lighting-test')
local Room = require('categories.lighting.room')

local Occluders = haylen.class('Occluders', LightingTest)

Occluders.code = [[
local walls = lighting2d.occludersFromMap(map, 'walls')  -- One per object of the layer.
local shapes = lighting2d.occludersFromBody(world, crate)
for _, shape in ipairs(shapes) do
    shape.x, shape.y, shape.rotation = crate.x, crate.y, crate.rotation  -- Follows the body.
end
graphics2d.drawOccluder(walls[1])  -- Casts the shadows of the lights drawn with shadows = true.]]

Occluders.maxCrates = 40
Occluders.brick = 96
Occluders.crate = 92
Occluders.stoneTile = 300
-- Where the first crates fall from: onto the ledge, the shelf, the beam and the floor.
Occluders.drops = {{330, 120}, {1520, 260}, {1180, 420}, {860, 700}, {1480, 760}, {1660, 860}}

-- Culls the edges of closed occluders that face the light, whatever their winding, so walls and crates stay lit and only what lies behind them is dark.
function Occluders.shadeBehind(occluders)
    for _, occluder in ipairs(occluders) do
        if occluder.closed then
            occluder.cull = m.polygonSignedArea(occluder.points) > 0 and 'counterClockwise' or 'clockwise'
        end
    end
    return occluders
end

function Occluders:init(entry)
    Occluders.super.init(self, entry)
    self.ambientLight = '#FF2A2E40'
    self.map = tiled.newMapRenderer(assets.load('lighting/walls.tmj'))
    self.world = physics2d.newWorld({owner = self})
    self.map:buildCollision(self.world)
    self.walls = Occluders.shadeBehind(lighting2d.occludersFromMap(self.map, 'walls'))
    self.lamp = lighting2d.newLight({radius = 1100, color = '#FFFFE4B8', intensity = 1.3, shadows = true, shadowFilter = 'pcf5', shadowSmoothness = 1})
    self.bricks = Room.bricks()
    self.stone = assets.texture(Room.floor, {filter = 'linear', wrap = 'repeat', mipmaps = true})
    self.plank = Occluders.plank()
    self.crateTexture = assets.texture(Room.tileset, {filter = 'linear'})
    self.floorTop = self:findFloorTop()
    self.ground = {vertices = {{}, {}, {}, {}}, indices = {1, 2, 3, 1, 3, 4}, edge = {0, 0, 0, 0}, stroke = {width = 6, color = '#FF1E1B33', layer = 2}}
    self.crates = {}
    for _, drop in ipairs(Occluders.drops) do
        self:drop(drop[1], drop[2])
    end
end

function Occluders:enter()
    local bounds = self.map.pixelBounds
    self:frame{
        hint = 'Move the light with the mouse, a finger, the arrows, WASD or a stick. A click, a tap, E, Enter or the south button drops a crate at the cursor.',
        code = Occluders.code,
        cursor = 'lamp',
        view = {bounds.height, bounds.height},
        controls = {ui.button{id = 'remove', text = 'Remove the crates', onClick = function()
            for _, crate in ipairs(self.crates) do
                crate.body:destroy()
            end
            self.crates = {}
        end}},
    }
    self.camera.position = {bounds.x + bounds.width / 2, bounds.y + bounds.height / 2}
end

function Occluders:drop(x, y)
    if #self.crates >= Occluders.maxCrates then
        table.remove(self.crates, 1).body:destroy()
    end
    local body = self.world:createBody({x = x, y = y, rotation = #self.crates * 0.4})
    body:addBox(Occluders.crate, Occluders.crate, {friction = 0.7})
    self.crates[#self.crates + 1] = {body = body, occluders = Occluders.shadeBehind(lighting2d.occludersFromBody(self.world, body))}
end

function Occluders:fixedUpdate(step)
    self.world:step(step)
end

function Occluders:update(dt)
    Occluders.super.update(self, dt)
    self.lamp.x, self.lamp.y = self.cursorX, self.cursorY
    if self:pressed() then
        self:drop(self.cursorX, self.cursorY)
    end
    for _, crate in ipairs(self.crates) do
        for _, occluder in ipairs(crate.occluders) do
            occluder.x, occluder.y, occluder.rotation = crate.body.x, crate.body.y, crate.body.rotation
        end
    end
    self:status(string.format('The map gives %d wall occluders and %d crates follow their bodies, %d occluders in all.', #self.walls, #self.crates, graphics2d.stats().occluders))
end

-- Covers the canvas with a brick wall whose tile repeats every `Occluders.brick` units from the origin of the world, as one sprite whose source reaches over as many tiles as the canvas shows.
function Occluders:drawBricks()
    local canvas = graphics2d.canvasBounds()
    local scale = self.bricks.width / Occluders.brick
    graphics2d.draw(self.bricks, canvas.x, canvas.y, {source = {canvas.x * scale, canvas.y * scale, canvas.width * scale, canvas.height * scale}, width = canvas.width, height = canvas.height, pivotX = 0, pivotY = 0, color = '#FF9A90A0'})
end

-- Returns the height in the world of the top of the floor object of the map, where the stone ground meets the bricks.
function Occluders:findFloorTop()
    for _, object in ipairs(self.map:objects('walls')) do
        if object.name == 'floor' then
            local _, y = self.map:objectToWorld(object.x, object.y)
            return y
        end
    end
    error('The map of the walls has no object named "floor".')
end

-- Carries the stone of the floor under the walls to both edges of the canvas, so the ground outside the hall continues the floor instead of the bricks reaching below it.
function Occluders:drawGround()
    local canvas = graphics2d.canvasBounds()
    local left, right, bottom = canvas.x, canvas.x + canvas.width, canvas.y + canvas.height
    local corners = {{left, self.floorTop}, {right, self.floorTop}, {right, bottom}, {left, bottom}}
    for index, corner in ipairs(corners) do
        local vertex = self.ground.vertices[index]
        vertex.x, vertex.y, vertex.u, vertex.v = corner[1], corner[2], corner[1] / Occluders.stoneTile, corner[2] / Occluders.stoneTile
    end
    graphics2d.drawMesh(self.stone, self.ground.vertices, self.ground.indices)
    local edge = self.ground.edge
    edge[1], edge[2], edge[3], edge[4] = left, self.floorTop, right, self.floorTop
    graphics2d.drawStroke(edge, self.ground.stroke)
end

-- Returns a plank of wood seen from the side and lit from above, with a light top edge, its grain and a shaded bottom edge, which repeats along a stroke.
function Occluders.plank()
    local image = graphics.newImage(64, 32, '#FFB0743F')
    image:fill('#FFD8A060', {0, 0, 64, 7})
    image:fill('#FF9A6236', {0, 13, 38, 2})
    image:fill('#FF9A6236', {26, 19, 38, 2})
    image:fill('#FF7E4C2A', {0, 25, 64, 7})
    return graphics.newTexture(image, {filter = 'linear', wrap = 'repeat'})
end

-- Draws a closed occluder as stone that repeats across it, with a dark outline, and an open one as a wooden beam.
function Occluders:drawWall(occluder)
    local points = occluder:worldPoints()
    local flat = {}
    for _, point in ipairs(points) do
        flat[#flat + 1] = point.x
        flat[#flat + 1] = point.y
    end
    if not occluder.closed then
        graphics2d.drawStroke(flat, {width = 40, color = Room.outlineColor, join = 'round', cap = 'round', layer = 1})
        graphics2d.drawStroke(flat, {width = 30, texture = self.plank, textureMode = 'tile', textureLength = 60, join = 'round', cap = 'round', layer = 2})
        return
    end
    local vertices, indices = {}, {}
    for index, point in ipairs(points) do
        vertices[index] = {x = point.x, y = point.y, u = point.x / Occluders.stoneTile, v = point.y / Occluders.stoneTile}
        if index > 2 then
            indices[#indices + 1], indices[#indices + 2], indices[#indices + 3] = 1, index - 1, index
        end
    end
    graphics2d.drawMesh(self.stone, vertices, indices, {layer = 1})
    flat[#flat + 1], flat[#flat + 2] = flat[1], flat[2]
    graphics2d.drawStroke(flat, {width = 6, color = '#FF1E1B33', join = 'round', layer = 2})
end

function Occluders:draw(area)
    self:drawBricks()
    self:drawGround()
    for _, wall in ipairs(self.walls) do
        self:drawWall(wall)
        graphics2d.drawOccluder(wall)
    end
    for _, crate in ipairs(self.crates) do
        graphics2d.draw(self.crateTexture, crate.body.x, crate.body.y, {source = Room.sources.crate, width = Occluders.crate / Room.fill.crate, height = Occluders.crate / Room.fill.crate, rotation = crate.body.rotation, layer = 3})
        for _, occluder in ipairs(crate.occluders) do
            graphics2d.drawOccluder(occluder)
        end
    end
    Room.drawLamp(self.lamp.x, self.lamp.y, self.lamp.color, 28)
    graphics2d.drawLight(self.lamp)
end

return Occluders
