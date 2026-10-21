-- Digging and building the walls of a map with `map:setTile` while balls bounce around: `map:buildCollision` merges the solid tiles that touch into chain loops, one around each region and one around each hole, and every change traces the loops of the regions it touches again on the same body.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local MapTest = require('categories.tiled.map-test')

local Digging = haylen.class('Digging', MapTest)

Digging.code = [[
local bodies = map:buildCollision(world)  -- Touching solid tiles merge into chain loops.
local column, row = map:worldToCell(x, y)
map:setTile('walls', column, row, 0)  -- Digs the cell, and the loops around it trace again.
map:setTile('walls', column, row, wallGid)  -- Builds a wall back.
print(#bodies[2]:outlines())]]

local kWallLayer = 'walls'
local kBalls = 24
local kRadius = 9

function Digging:enter()
    self.random = m.random(17)
    self:build()
    local bounds = self.map.pixelBounds
    self:frame{
        hint = 'Tap or click a wall to dig it away and an empty cell to build a wall there. The gold lines are the collision loops of the walls, traced again around the regions each change touches. R or the X button restores the map.',
        code = Digging.code,
        controls = {
            ui.button{id = 'dig', text = 'Dig a random wall', onClick = function() self:digRandom() end},
            ui.button{id = 'reset', text = 'Restore the map', onClick = function() self:build() end},
        },
        view = {bounds.width + 64, bounds.height + 64},
        focus = 'dig',
    }
    self.camera:snapTo(bounds.width / 2, bounds.height / 2)
end

function Digging:build()
    self.map = MapTest.loadMap('tiled/maps/collision.tmj')
    self.wallGid = self.map:tile(kWallLayer, 0, 0)
    self.world = physics2d.newWorld({gravity = {0, 0}})
    self.bodies = self.map:buildCollision(self.world)
    -- The bodies come in map order, one per tile layer that collides and one per object layer with collision objects, so the walls layer gives the second one.
    self.walls = self.bodies[2]
    self.changes = 0
    self.balls = {}
    -- The balls start spread over the open cells of the yard, one cell apart, and fly off in every direction.
    local open = {}
    for row = 1, self.map.height - 2 do
        for column = 1, self.map.width - 2 do
            if self.map:tile(kWallLayer, column, row) == 0 and (column + row) % 3 == 0 then
                open[#open + 1] = {column, row}
            end
        end
    end
    self.random:shuffle(open)
    for index = 1, math.min(kBalls, #open) do
        local angle = index / kBalls * math.pi * 2
        local x, y = self.map:cellToWorld(open[index][1], open[index][2])
        local ball = self.world:createBody({x = x + self.map.tileWidth / 2, y = y + self.map.tileHeight / 2, vx = math.cos(angle) * 360, vy = math.sin(angle) * 360, bullet = true})
        ball:addCircle(kRadius, {restitution = 1, friction = 0})
        self.balls[index] = ball
    end
end

function Digging:toggle(column, row)
    local width, height = self.map.width, self.map.height
    if column < 1 or row < 1 or column >= width - 1 or row >= height - 1 then
        return
    end
    local gid = self.map:tile(kWallLayer, column, row)
    self.map:setTile(kWallLayer, column, row, gid == 0 and self.wallGid or 0)
    self.changes = self.changes + 1
end

function Digging:digRandom()
    for _ = 1, 50 do
        local column, row = self.random:integer(1, self.map.width - 2), self.random:integer(1, self.map.height - 2)
        if self.map:tile(kWallLayer, column, row) ~= 0 then
            self:toggle(column, row)
            return
        end
    end
end

function Digging:update(dt)
    Digging.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    if self.pointer.pressed then
        local column, row = self.map:worldToCell(self.pointer.worldX, self.pointer.worldY)
        self:toggle(column, row)
    end
    self:status(string.format('The walls hold %d loops and shapes after %d changes, and a step of the %d balls takes %.2f ms.', #self.walls:outlines(), self.changes, #self.balls, self.world:stats().stepMilliseconds))
end

function Digging:fixedUpdate(step)
    self.world:step(step)
end

-- The collision loops of the walls draw over the map, and the balls as polished marbles.
function Digging:draw(area)
    self.map:draw(self.camera)
    MapTest.frameMap(self.map.pixelBounds)
    local unit = graphics2d.canvasUnitSize()
    for _, outline in ipairs(self.walls:outlines()) do
        graphics2d.drawPolyline(outline.points, 5 * unit, MapTest.ink, outline.closed, {layer = 5})
        graphics2d.drawPolyline(outline.points, 2.5 * unit, '#FFFFC93C', outline.closed, {layer = 6})
    end
    for _, ball in ipairs(self.balls) do
        graphics2d.drawShape({ball.x - kRadius, ball.y - kRadius, kRadius * 2, kRadius * 2}, {kind = 'ellipse', color = '#FF2F86F6', borderWidth = 2.5, borderColor = MapTest.ink, layer = 7})
        graphics2d.drawShape({ball.x - kRadius * 0.55, ball.y - kRadius * 0.6, kRadius * 0.6, kRadius * 0.45}, {kind = 'ellipse', color = '#B3FFFFFF', layer = 8})
    end
end

return Digging
