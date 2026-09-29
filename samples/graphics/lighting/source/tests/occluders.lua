-- Occluders built from data: the walls of a Tiled object layer and the shapes of physics bodies, whose occluders follow them every frame.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local tiled = require('haylen.tiled')
local ui = require('haylen.ui')

local sample = require('sample')

local Occluders = haylen.class('Occluders', sample.Test)

Occluders.hints = 'Move the light with the mouse, a finger, WASD or the left stick. Click, tap, E or the X button drops a crate at the cursor.'

Occluders.maxCrates = 40

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
    self.map = tiled.newMapRenderer(assets.load('maps/walls.tmj'))
    self.world = physics2d.newWorld()
    self.map:buildCollision(self.world)
    self.walls = Occluders.shadeBehind(lighting2d.occludersFromMap(self.map, 'walls'))

    local bounds = self.map.pixelBounds
    self.camera = graphics2d.newCamera()
    self.camera.position = {bounds.x + bounds.width / 2, bounds.y + bounds.height / 2}
    self.cursor = sample.Cursor()
    self.lamp = lighting2d.newLight({radius = 1100, color = '#FFFFE4B8', intensity = 1.3, shadows = true, shadowFilter = 'pcf5', shadowSmoothness = 1})
    self.crates = {}
    for index = 1, 6 do
        self:drop(1220 + index * 90, 120 + index * 40)
    end
end

function Occluders:controls()
    return {ui.button{text = 'Remove the crates', onClick = function()
        for _, crate in ipairs(self.crates) do
            crate.body:destroy()
        end
        self.crates = {}
    end}}
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
    self.cursor:update(dt)
    local x, y = self.cursor:world(self.camera)
    self.lamp.x, self.lamp.y = x, y
    if sample.pressed() then
        self:drop(x, y)
    end

    for _, crate in ipairs(self.crates) do
        for _, occluder in ipairs(crate.occluders) do
            occluder.x, occluder.y, occluder.rotation = crate.body.x, crate.body.y, crate.body.rotation
        end
    end
    self:setStatus(string.format('%d wall occluders from the map, %d crates, %d occluders drawn', #self.walls, #self.crates, graphics2d.stats().occluders))
end

-- Draws an occluder as the shape it outlines: filled when it is closed and as a thick line when it is open.
function Occluders.drawOutline(occluder, color)
    local points = occluder:worldPoints()
    if occluder.closed then
        graphics2d.drawPolygon(points, color, {layer = 1})
    else
        graphics2d.drawPolyline(points, 14, color, false, {layer = 1})
    end
end

function Occluders:render()
    graphics2d.beginWorld(self.camera, {ambientLight = '#FF1A1E2A'})
    graphics2d.drawRect(self.map.pixelBounds, '#FF5A6068')
    for _, wall in ipairs(self.walls) do
        Occluders.drawOutline(wall, '#FF4A7A9A')
        graphics2d.drawOccluder(wall)
    end
    for _, crate in ipairs(self.crates) do
        for _, occluder in ipairs(crate.occluders) do
            Occluders.drawOutline(occluder, '#FFA0683A')
            graphics2d.drawOccluder(occluder)
        end
    end
    graphics2d.drawLight(self.lamp)
end

function Occluders:renderUi()
    graphics2d.beginScreen()
    self.cursor:draw()
end

return Occluders
