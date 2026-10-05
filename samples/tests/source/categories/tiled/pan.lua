-- Moves the camera of a test: dragging the pointer pans it, the `move` action pans it with the keys, the directional pad or a stick, and the mouse wheel zooms it.
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')

local Pan = haylen.class('Pan')

Pan.speed = 700
Pan.wheelStep = 1.1
Pan.minScale, Pan.maxScale = 0.4, 4

function Pan:init(test)
    self.test = test
end

function Pan:zoom(factor)
    self.test.zoomScale = m.clamp(self.test.zoomScale * factor, Pan.minScale, Pan.maxScale)
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
    camera.x = camera.x + x * Pan.speed * dt / zoom
    camera.y = camera.y + y * Pan.speed * dt / zoom

    local _, scroll = input.mouseScroll()
    if scroll ~= 0 then
        self:zoom(Pan.wheelStep ^ scroll)
    end
end

return Pan
