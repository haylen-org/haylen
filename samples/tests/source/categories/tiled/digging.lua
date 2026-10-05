-- Digging and building the walls of a map with `map:setTile` while balls bounce around: `map:buildCollision` merges the solid tiles that touch into chain loops, one around each region and one around each hole, and every change traces the loops of the regions it touches again on the same body.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local tiled = require('haylen.tiled')
local ui = require('haylen.ui')

local MapTest = require('categories.tiled.map-test')

local Digging = haylen.class('Digging', MapTest)

local kWallLayer = 'walls'
local kBalls = 24
local kRadius = 9

function Digging:enter()
    self.random = m.random(17)
    self:build()
    local bounds = self.map.pixelBounds
    self:frame{
        hint = 'Tap or click a wall to dig it away and an empty cell to build a wall there. The lines are the collision loops, traced again around the regions each change touches. R or the X button restores the map.',
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
    self.map = tiled.newMapRenderer(assets.load('tiled/maps/collision.tmj'))
    self.wallGid = self.map:tile(kWallLayer, 0, 0)
    self.world = physics2d.newWorld({gravity = {0, 0}})
    self.bodies = self.map:buildCollision(self.world)
    -- The bodies come in map order, one per tile layer that collides and one per object layer with collision objects, so the walls layer gives the second one.
    self.walls = self.bodies[2]
    self.changes = 0
    self.balls = {}
    for index = 1, kBalls do
        local angle = index / kBalls * math.pi * 2
        local ball = self.world:createBody({x = 28 + index * 36, y = 64, vx = math.cos(angle) * 360, vy = math.sin(angle) * 360, bullet = true})
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
    self:status(string.format('Collision loops and shapes of the walls %d, changes %d, balls %d, step %.2f ms', #self.walls:outlines(), self.changes, #self.balls, self.world:stats().stepMilliseconds))
end

function Digging:fixedUpdate(step)
    self.world:step(step)
end

function Digging:draw(area)
    self.map:draw(self.camera)
    self.world:debugDraw({layer = 5})
    for _, ball in ipairs(self.balls) do
        graphics2d.drawCircle(ball.x, ball.y, kRadius, '#FFFFD54F', {layer = 6})
    end
end

return Digging
