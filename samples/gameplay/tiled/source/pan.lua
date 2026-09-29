-- Moves the camera of a test: dragging the pointer pans it, the move action pans it with the keys or a stick, and the mouse wheel zooms it.
local input = require('haylen.input')
local m = require('haylen.math')

local Pan = {}
Pan.__index = Pan

local kSpeed = 700
local kWheelStep = 1.1
local kMinScale, kMaxScale = 0.4, 4

function Pan.new(test)
    return setmetatable({test = test}, Pan)
end

function Pan:zoom(factor)
    self.test.zoomScale = m.clamp(self.test.zoomScale * factor, kMinScale, kMaxScale)
end

function Pan:update(dt)
    local test = self.test
    local camera, pointer = test.camera, test.pointer
    local zoom = camera.zoom.x
    if pointer.down and not pointer.pressed then
        camera.x = camera.x - (pointer.x - self.lastX) / zoom
        camera.y = camera.y - (pointer.y - self.lastY) / zoom
    end
    self.lastX, self.lastY = pointer.x, pointer.y

    local x, y = input.vector('move')
    camera.x = camera.x + x * kSpeed * dt / zoom
    camera.y = camera.y + y * kSpeed * dt / zoom

    local _, scroll = input.mouseScroll()
    if scroll ~= 0 then
        self:zoom(kWheelStep ^ scroll)
    end
end

return Pan
