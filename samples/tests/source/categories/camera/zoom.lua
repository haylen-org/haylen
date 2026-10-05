-- Zoom around a point: `camera:zoomAt` keeps the world point under a screen point in place, which is how the wheel and a pinch zoom toward the pointer, within the zoom limits.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local Test = require('harness.test')
local World = require('categories.camera.world')
local actions = require('categories.camera.actions')

local Zoom = haylen.class('Zoom', Test)

function Zoom:init(entry)
    Zoom.super.init(self, entry)
    self.world = World()
    self.camera = graphics2d.newCamera()
    self.camera.minZoom = 0.25
    self.camera.maxZoom = 4
end

function Zoom:enter()
    self:loadActions(actions)
    self:frame{
        hint = 'Zoom with the wheel or a pinch toward the pointer, or with Z and X, minus and plus or the shoulders and triggers toward the middle. Pan with WASD, the arrows, the left stick or by dragging.',
        play = true,
        controls = {ui.button{id = 'reset', text = 'Reset the view', onClick = function()
            self.camera.zoom = {1, 1}
            self.camera.position = {0, 0}
        end}},
    }
end

function Zoom:update(dt)
    Zoom.super.update(self, dt)
    local stage = self.stage
    if not stage then
        return
    end
    local camera = self.camera
    local mouseX, mouseY = input.mousePosition()
    local _, wheel = input.mouseScroll()
    if wheel ~= 0 and stage:contains({mouseX, mouseY}) and not ui.usingPointer() then
        camera:zoomAt(1.15 ^ wheel, mouseX, mouseY)
    end

    -- A pinch reports its spread since the second finger landed, so each frame zooms by the change since the last one.
    local pinched = false
    for _, gesture in ipairs(input.gestures()) do
        if gesture.type == 'pinch' and stage:contains({gesture.x, gesture.y}) then
            camera:zoomAt(gesture.scale / (self.pinch or gesture.scale), gesture.x, gesture.y)
            self.pinch = gesture.scale
            pinched = true
        end
    end
    if not pinched and #input.touches() < 2 then
        self.pinch = nil
    end

    local keys = input.value('zoom')
    if keys ~= 0 then
        camera:zoomAt(1 + keys * dt * 1.5, stage.x + stage.width / 2, stage.y + stage.height / 2)
    end

    local moveX, moveY = input.vector('move')
    local dragX, dragY = 0, 0
    local touches = input.touches()
    if input.down('point') and #touches == 0 and stage:contains({mouseX, mouseY}) then
        dragX, dragY = input.mouseDelta()
    elseif #touches == 1 and not ui.usingPointer() and stage:contains({touches[1].x, touches[1].y}) then
        dragX, dragY = touches[1].dx, touches[1].dy
    end
    camera.x = camera.x + moveX * 700 * dt / camera.zoom.x - dragX / camera.zoom.x
    camera.y = camera.y + moveY * 700 * dt / camera.zoom.y - dragY / camera.zoom.y
    self:status(string.format('Zoom %.2f between %.2f and %.2f   Center %.0f, %.0f', camera.zoom.x, camera.minZoom, camera.maxZoom, camera.x, camera.y))
end

function Zoom:draw(area)
    self.world:draw()
    local width = 3 * graphics2d.canvasUnitSize()
    for x = -2400, 2400, 400 do
        graphics2d.drawLine(x, -1400, x, 1400, width, '#40FFFFFF', {layer = 5})
    end
    for y = -1400, 1400, 400 do
        graphics2d.drawLine(-2400, y, 2400, y, width, '#40FFFFFF', {layer = 5})
    end
end

return Zoom
