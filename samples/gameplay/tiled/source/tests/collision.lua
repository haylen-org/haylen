-- Physics collision built by map:buildCollision from the collision shapes of tiles and from collision objects, with water tiles and a pit as sensors and fences in a category of their own.
local haylen = require('haylen')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local tiled = require('haylen.tiled')
local ui = require('haylen.ui')

local sample = require('sample')

local Collision = haylen.class('Collision', sample.Test)

local kFenceCategory = 2
local kMaxBalls = 60

function Collision:enter()
    self.map = tiled.newMapRenderer(assets.load('maps/collision.tmj'))
    local bounds = self.map.pixelBounds
    Collision.super.enter(self, {
        hint = 'Tap or click to throw a ball from there. Blue balls pass the fences, whose layer puts them in their own category. Balls in the water or the pit turn gold while they overlap the sensor.',
        controls = {
            ui.checkbox{id = 'bodies', text = 'Show the collision', checked = true, onChange = function(event) self.showBodies = event.checked end},
            ui.button{id = 'rain', text = 'Throw ten balls', onClick = function() self:rain() end},
            ui.button{id = 'reset', text = 'Clear the balls', onClick = function() self:build() end},
        },
        stats = true,
        view = {bounds.width + 64, bounds.height + 64},
        focus = 'bodies',
    })
    self.showBodies = true
    self.random = m.random(113)
    self.camera:snapTo(bounds.width / 2, bounds.height / 2)
    self:build()
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
    ball:addCircle(10, {restitution = 0.8, friction = 0.1, mask = blue and ~kFenceCategory or -1})
    ball.data = {blue = blue, wet = 0}
    self.balls[#self.balls + 1] = ball
    if #self.balls > kMaxBalls then
        table.remove(self.balls, 1):destroy()
    end
end

function Collision:rain()
    for _ = 1, 10 do
        self:throw(self.random:range(64, self.map.pixelBounds.width - 64), self.random:range(64, 160))
    end
end

function Collision:exit()
    Collision.super.exit(self)
    self.map, self.world, self.walls, self.balls = nil, nil, nil, nil
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
    self:showStats(string.format('bodies from the map %d\none per layer with shapes\nballs %d\nin a sensor %d', #self.walls, #self.balls, wet))
end

function Collision:fixedUpdate(step)
    self.world:step(step)
end

function Collision:render()
    self:beginWorld()
    self.map:draw(self.camera)
    if self.showBodies then
        self.world:debugDraw({layer = 5})
    end
    for _, ball in ipairs(self.balls) do
        local color = ball.data.wet > 0 and '#FFFFD54F' or (ball.data.blue and '#FF4FC3F7' or '#FFE57373')
        graphics2d.drawCircle(ball.x, ball.y, 10, color, {layer = 6})
    end
end

return Collision
