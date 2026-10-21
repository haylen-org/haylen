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
local walls = lighting2d.occludersFromMap(map, 'walls')  -- One per object of the layer, in world coordinates.
local shapes = lighting2d.occludersFromBody(world, crate)
for _, shape in ipairs(shapes) do shape.x, shape.y, shape.rotation = crate.x, crate.y, crate.rotation end  -- Follows the body.
graphics2d.drawOccluder(walls[1])
graphics2d.drawLight({x = x, y = y, radius = 1100, shadows = true, shadowFilter = 'pcf5'})]]

Occluders.maxCrates = 40
Occluders.brick = 96
Occluders.quad = {1, 2, 3, 1, 3, 4}

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
    self.ambientLight = '#FF1A1E2A'
    self.map = tiled.newMapRenderer(assets.load('lighting/walls.tmj'))
    self.world = physics2d.newWorld()
    self.map:buildCollision(self.world)
    self.walls = Occluders.shadeBehind(lighting2d.occludersFromMap(self.map, 'walls'))
    self.lamp = lighting2d.newLight({radius = 1100, color = '#FFFFE4B8', intensity = 1.3, shadows = true, shadowFilter = 'pcf5', shadowSmoothness = 1})
    self.bricks = Room.bricks()
    self.backdrop = {}
    local tiles = assets.load(Room.tileset, 'image')
    self.stone = graphics.newTexture(tiles:crop(Room.sources.floor), {filter = 'linear', wrap = 'repeat'})
    self.crateTexture = assets.texture(Room.tileset, {filter = 'linear'})
    self.crates = {}
    for index = 1, 6 do
        self:drop(780 + index * 70, 80 + index * 60)
    end
end

function Occluders:enter()
    local bounds = self.map.pixelBounds
    self:frame{
        hint = 'Move the light with the mouse, a finger, the arrows, WASD or a stick. A click, a tap, E, Enter or the south button drops a crate at the cursor.',
        code = Occluders.code,
        cursor = true,
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
    body:addBox(64, 64, {friction = 0.7})
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

-- Covers the canvas with a brick wall whose tile repeats every `Occluders.brick` units, as one mesh whose texture coordinates run past 1.
function Occluders:drawBricks()
    local canvas = graphics2d.canvasBounds()
    local size, color = Occluders.brick, '#FF9A90A0'
    local left, top, right, bottom = canvas.x, canvas.y, canvas:right(), canvas:bottom()
    self.backdrop[1] = {x = left, y = top, u = left / size, v = top / size, color = color}
    self.backdrop[2] = {x = right, y = top, u = right / size, v = top / size, color = color}
    self.backdrop[3] = {x = right, y = bottom, u = right / size, v = bottom / size, color = color}
    self.backdrop[4] = {x = left, y = bottom, u = left / size, v = bottom / size, color = color}
    graphics2d.drawMesh(self.bricks, self.backdrop, Occluders.quad)
end

-- Draws a closed occluder as stone that repeats across it, with a dark outline, and an open one as a wooden rail.
function Occluders:drawWall(occluder)
    local points = occluder:worldPoints()
    local flat = {}
    for _, point in ipairs(points) do
        flat[#flat + 1] = point.x
        flat[#flat + 1] = point.y
    end
    if not occluder.closed then
        graphics2d.drawStroke(flat, {width = 26, color = '#FF1E1B33', join = 'round', cap = 'round', layer = 1})
        graphics2d.drawStroke(flat, {width = 16, color = '#FFA8683A', join = 'round', cap = 'round', layer = 2})
        return
    end
    local vertices, indices = {}, {}
    for index, point in ipairs(points) do
        vertices[index] = {x = point.x, y = point.y, u = point.x / 160, v = point.y / 160}
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
    for _, wall in ipairs(self.walls) do
        self:drawWall(wall)
        graphics2d.drawOccluder(wall)
    end
    for _, crate in ipairs(self.crates) do
        graphics2d.draw(self.crateTexture, crate.body.x, crate.body.y, {source = Room.sources.crate, width = 64 / Room.fill.crate, height = 64 / Room.fill.crate, rotation = crate.body.rotation, layer = 3})
        for _, occluder in ipairs(crate.occluders) do
            graphics2d.drawOccluder(occluder)
        end
    end
    Room.drawLamp(self.lamp.x, self.lamp.y, self.lamp.color, 28)
    graphics2d.drawLight(self.lamp)
end

return Occluders
