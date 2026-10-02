-- Zoom around a point: `camera:zoomAt` keeps the world point under a screen point in place, which is how the wheel and a pinch zoom toward the pointer, within the zoom limits.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local World = require('world')
local sample = require('sample')

local Zoom = haylen.class('Zoom', sample.Test)

Zoom.hints = 'Zoom with the wheel or a pinch toward the pointer, or with Z and X, minus and plus or the shoulders and triggers toward the middle. Pan with WASD, the left stick or by dragging.'

function Zoom:init(entry)
    Zoom.super.init(self, entry)
    self.world = World()
    self.camera = graphics2d.newCamera()
    self.camera.minZoom = 0.25
    self.camera.maxZoom = 4
    self.pinch = nil
end

function Zoom:controls()
    return {ui.button{text = 'Reset the view', onClick = function()
        self.camera.zoom = {1, 1}
        self.camera.position = {0, 0}
    end}}
end

function Zoom:update(dt)
    local camera = self.camera
    local _, wheel = input.mouseScroll()
    if wheel ~= 0 and not ui.usingPointer() then
        camera:zoomAt(1.15 ^ wheel, input.mousePosition())
    end

    -- A pinch reports its spread since the second finger landed, so each frame zooms by the change since the last one.
    local pinched = false
    for _, gesture in ipairs(input.gestures()) do
        if gesture.type == 'pinch' then
            camera:zoomAt(gesture.scale / (self.pinch or gesture.scale), gesture.x, gesture.y)
            self.pinch = gesture.scale
            pinched = true
        end
    end
    if not pinched and #input.touches() < 2 then
        self.pinch = nil
    end

    local area = viewport.visibleRect()
    local keys = input.value('zoom')
    if keys ~= 0 then
        camera:zoomAt(1 + keys * dt * 1.5, area.x + area.width / 2, area.y + area.height / 2)
    end

    local moveX, moveY = input.vector('move')
    local dragX, dragY = 0, 0
    if input.down('point') and #input.touches() == 0 then
        dragX, dragY = input.mouseDelta()
    elseif #input.touches() == 1 and not ui.usingPointer() then
        dragX, dragY = input.touches()[1].dx, input.touches()[1].dy
    end
    camera.x = camera.x + moveX * 700 * dt / camera.zoom.x - dragX / camera.zoom.x
    camera.y = camera.y + moveY * 700 * dt / camera.zoom.y - dragY / camera.zoom.y
    self:setStatus(string.format('Zoom %.2f between %.2f and %.2f, center %.0f, %.0f', camera.zoom.x, camera.minZoom, camera.maxZoom, camera.x, camera.y))
end

function Zoom:render()
    graphics2d.beginWorld(self.camera)
    self.world:draw()
    for x = -2400, 2400, 400 do
        graphics2d.drawLine(x, -1400, x, 1400, 3 * graphics2d.canvasUnitSize(), '#40FFFFFF', {layer = 5})
    end
    for y = -1400, 1400, 400 do
        graphics2d.drawLine(-2400, y, 2400, y, 3 * graphics2d.canvasUnitSize(), '#40FFFFFF', {layer = 5})
    end
end

return Zoom
