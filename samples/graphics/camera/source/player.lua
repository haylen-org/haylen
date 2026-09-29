-- A walker the tests follow with their cameras. It moves with a vector action, or toward the point the mouse button or a finger holds.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local ui = require('haylen.ui')

local World = require('world')

local Player = haylen.class('Player')

Player.speed = 560

-- Takes the start position, the color and the vector action that steers it, which a player moved by code leaves out.
function Player:init(x, y, color, action)
    self.x, self.y = x, y
    self.vx, self.vy = 0, 0
    self.color = color
    self.action = action
    self.heading = 0
end

-- Returns the screen point held with the left mouse button or a finger outside the interface, or nothing.
function Player.heldPoint()
    local touch = input.touches()[1]
    if touch and touch.phase ~= 'ended' and touch.phase ~= 'cancelled' and not ui.wantsPointer() then
        return touch.x, touch.y
    end
    if input.down('point') then
        return input.mousePosition()
    end
end

-- Moves for a frame. With a camera, a pointer held inside its view steers the player toward the point of the world under it.
function Player:update(dt, camera)
    local dx, dy = input.vector(self.action)
    local screenX, screenY = Player.heldPoint()
    local inside = camera and screenX and (camera.viewport == nil or camera.viewport:contains({screenX, screenY}))
    if inside and dx == 0 and dy == 0 then
        local x, y = camera:screenToWorld(screenX, screenY)
        local direction = m.vec2(x - self.x, y - self.y)
        if direction:length() > 20 then
            dx, dy = direction:normalized():unpack()
        end
    end

    self.vx, self.vy = dx * Player.speed, dy * Player.speed
    local bounds = World.bounds
    self.x = m.clamp(self.x + self.vx * dt, bounds.x, bounds:right())
    self.y = m.clamp(self.y + self.vy * dt, bounds.y, bounds:bottom())
    if dx ~= 0 or dy ~= 0 then
        self.heading = math.atan(dy, dx)
    end
end

function Player:draw()
    graphics2d.drawCircle(self.x + 6, self.y + 10, 30, '#50000000', {layer = 1})
    graphics2d.drawCircle(self.x, self.y, 30, self.color, {layer = 1})
    graphics2d.drawCircle(self.x + math.cos(self.heading) * 18, self.y + math.sin(self.heading) * 18, 9, '#FFFFFFFF', {layer = 1})
end

return Player
