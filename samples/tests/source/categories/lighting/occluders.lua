-- Occluders built from data: the walls of a Tiled object layer and the shapes of physics bodies, whose occluders follow them every frame.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local tiled = require('haylen.tiled')
local ui = require('haylen.ui')

local LightingTest = require('categories.lighting.lighting-test')

local Occluders = haylen.class('Occluders', LightingTest)

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
    self.ambientLight = '#FF1A1E2A'
    self.map = tiled.newMapRenderer(assets.load('lighting/walls.tmj'))
    self.world = physics2d.newWorld()
    self.map:buildCollision(self.world)
    self.walls = Occluders.shadeBehind(lighting2d.occludersFromMap(self.map, 'walls'))
    self.lamp = lighting2d.newLight({radius = 1100, color = '#FFFFE4B8', intensity = 1.3, shadows = true, shadowFilter = 'pcf5', shadowSmoothness = 1})
    self.crates = {}
    for index = 1, 6 do
        self:drop(1220 + index * 90, 120 + index * 40)
    end
end

function Occluders:enter()
    local bounds = self.map.pixelBounds
    self:frame{
        hint = 'Move the light with the mouse, a finger, the arrows, WASD or a stick. A click, a tap, E, Enter or the south button drops a crate at the cursor.',
        cursor = true,
        view = {bounds.width, bounds.height},
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
    self:status(string.format('Wall occluders from the map %d   Crates %d   Occluders drawn %d', #self.walls, #self.crates, graphics2d.stats().occluders))
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

function Occluders:draw(area)
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

return Occluders
