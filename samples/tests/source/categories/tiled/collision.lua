-- Physics collision built by `map:buildCollision` from the collision shapes of tiles and from collision objects, with the pond and the lava pit as sensors and fences in a category of their own. The collision draws as the merged outlines of the tile layers and the outlines of the objects, and a rope gives the polyline its art.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local MapTest = require('categories.tiled.map-test')
local outlines = require('categories.tiled.outlines')

local Collision = haylen.class('Collision', MapTest)

Collision.code = [[
local world = physics2d.newWorld({gravity = {0, 0}})
local walls = map:buildCollision(world)  -- One static body per tile layer that collides and per object layer with collision.
world.onSensorBegin = function(sensor, visitor) visitor.data.wet = true end  -- The pond and the pit are sensors.
local ball = world:createBody({x = x, y = y, vx = 320, linearDamping = 0.3})
ball:addCircle(10, {restitution = 0.8, mask = ~2})  -- Passes the fences of category 2.]]

local kFenceCategory = 2
local kMaxBalls = 60
local kRadius = 10
local kColors = {red = '#FFF0473E', blue = '#FF2F86F6', wet = '#FFFFC93C'}

function Collision:enter()
    self.map = MapTest.loadMap('tiled/maps/collision.tmj')
    local bounds = self.map.pixelBounds
    self:frame{
        hint = 'Tap or click to throw a ball from there. Blue balls pass the fences, whose layer puts them in their own category. Balls in the pond or the lava pit turn gold while they overlap the sensor. R or the X button clears the balls.',
        code = Collision.code,
        controls = {
            ui.checkbox{id = 'bodies', text = 'Show the collision', checked = true, onChange = function(event) self.showBodies = event.checked end},
            ui.button{id = 'rain', text = 'Throw ten balls', onClick = function() self:rain() end},
            ui.button{id = 'reset', text = 'Clear the balls', onClick = function() self:build() end},
        },
        view = {bounds.width + 64, bounds.height + 64},
        focus = 'bodies',
    }
    self.showBodies = true
    self.random = m.random(113)
    self.rope = assets.texture('sprites/images/rope.png', {filter = 'linear', wrap = 'repeat'})
    self.shapes = self.map:objects('shapes')
    self.camera:snapTo(bounds.width / 2, bounds.height / 2)
    self:build()
    self:rain()
end

function Collision:build()
    self.world = physics2d.newWorld({gravity = {0, 0}})
    self.walls = self.map:buildCollision(self.world)
    self.balls = {}
    -- Only balls carry data, while the shapes of the map that touch a sensor count for nothing.
    self.world.onSensorBegin = function(sensor, visitor)
        if visitor.data then
            visitor.data.wet = visitor.data.wet + 1
        end
    end
    self.world.onSensorEnd = function(sensor, visitor)
        if visitor and visitor.data then
            visitor.data.wet = visitor.data.wet - 1
        end
    end
end

-- Balls fly in any direction on the top-down map, and blue ones leave the fence category out of their mask.
function Collision:throw(x, y)
    local blue = self.random:chance(0.5)
    local angle = self.random:range(0, math.pi * 2)
    local ball = self.world:createBody({x = x, y = y, vx = math.cos(angle) * 320, vy = math.sin(angle) * 320, linearDamping = 0.3})
    ball:addCircle(kRadius, {restitution = 0.8, friction = 0.1, mask = blue and ~kFenceCategory or -1})
    ball.data = {blue = blue, wet = 0}
    self.balls[#self.balls + 1] = ball
    if #self.balls > kMaxBalls then
        table.remove(self.balls, 1):destroy()
    end
end

function Collision:rain()
    for _ = 1, 10 do
        self:throw(self.random:range(64, self.map.pixelBounds.width - 64), self.random:range(48, 96))
    end
end

function Collision:update(dt)
    Collision.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    if self.pointer.pressed then
        self:throw(self.pointer.worldX, self.pointer.worldY)
    end
    local wet = 0
    for _, ball in ipairs(self.balls) do
        wet = wet + (ball.data.wet > 0 and 1 or 0)
    end
    self:status(string.format('The map built %d bodies, one per tile layer that collides and per object layer with collision. Of the %d balls, %d are in a sensor.', #self.walls, #self.balls, wet))
end

function Collision:fixedUpdate(step)
    self.world:step(step)
end

-- The outlines of what collides: the merged loops of the tile layers, and each collision object as the shape it was drawn with, with the sensors filled.
function Collision:drawCollision()
    local unit = graphics2d.canvasUnitSize()
    for index = 1, #self.walls - 1 do
        for _, outline in ipairs(self.walls[index]:outlines()) do
            graphics2d.drawPolyline(outline.points, 5 * unit, MapTest.ink, outline.closed, {layer = 5})
            graphics2d.drawPolyline(outline.points, 2.5 * unit, '#FF8FE3FF', outline.closed, {layer = 6})
        end
    end
    for _, object in ipairs(self.shapes) do
        local points, closed = outlines.of(self.map, object)
        if object.properties.sensor then
            graphics2d.drawPolygon(points, '#402FB3C4', {layer = 5})
        end
        graphics2d.drawPolyline(points, 5 * unit, MapTest.ink, closed, {layer = 5})
        graphics2d.drawPolyline(points, 2.5 * unit, object.properties.sensor and '#FFFFC93C' or '#FF8FE3FF', closed, {layer = 6})
    end
end

function Collision:draw(area)
    self.map:draw(self.camera)
    MapTest.frameMap(self.map.pixelBounds)
    for _, object in ipairs(self.shapes) do
        if object.shape == 'polyline' then
            local points = {}
            for _, point in ipairs(outlines.of(self.map, object)) do
                points[#points + 1] = point[1]
                points[#points + 1] = point[2]
            end
            graphics2d.drawStroke(points, {width = 12, texture = self.rope, textureMode = 'tile', join = 'round', cap = 'round', layer = 4})
        end
    end
    if self.showBodies then
        self:drawCollision()
    end
    for _, ball in ipairs(self.balls) do
        local color = ball.data.wet > 0 and kColors.wet or (ball.data.blue and kColors.blue or kColors.red)
        graphics2d.drawShape({ball.x - kRadius, ball.y - kRadius, kRadius * 2, kRadius * 2}, {kind = 'ellipse', color = color, borderWidth = 2.5, borderColor = MapTest.ink, layer = 7})
        graphics2d.drawShape({ball.x - kRadius * 0.55, ball.y - kRadius * 0.6, kRadius * 0.6, kRadius * 0.45}, {kind = 'ellipse', color = '#B3FFFFFF', layer = 8})
    end
end

return Collision
