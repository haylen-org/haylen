-- Screen to world: `camera:screenToWorld` turns the pointer into a world point through zoom and rotation, which picks the object under it, and `camera:worldToScreen` places a screen label over the pick.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local ui = require('haylen.ui')

local Test = require('harness.test')
local actions = require('categories.camera.actions')

local Picking = haylen.class('Picking', Test)

Picking.cursorSpeed = 900

function Picking:init(entry)
    Picking.super.init(self, entry)
    self.camera = graphics2d.newCamera()
    self.camera.zoom = {1.3, 1.3}
    self.camera.rotation = 0.3
    self.worldX, self.worldY = 0, 0
    local random = m.random(8)
    self.objects = {}
    for index = 1, 40 do
        self.objects[index] = {name = 'Object ' .. index, x = random:range(-900, 900), y = random:range(-500, 500), radius = random:range(26, 60), color = m.fromHsv(random:nextFloat(), 0.6, 0.9)}
    end
end

function Picking:enter()
    self:loadActions(actions)
    self:frame{
        hint = 'Point with the mouse, a finger, WASD, the arrows or the left stick, and select with a click, a tap, F or the X button. Q and E turn the view, Z and X zoom it.',
        play = true,
    }
end

-- Returns the topmost object whose circle holds the world point.
function Picking:objectAt(x, y)
    for index = #self.objects, 1, -1 do
        local object = self.objects[index]
        if (object.x - x) ^ 2 + (object.y - y) ^ 2 <= object.radius ^ 2 then
            return object
        end
    end
end

-- Moves the cursor, a screen point inside the play area, with the mouse, the first finger and the move action, and tells whether it selected this frame.
function Picking:moveCursor(dt, stage)
    if not self.cursorX then
        self.cursorX, self.cursorY = stage.x + stage.width / 2, stage.y + stage.height / 2
    end
    local selected = input.pressed('place')
    local touch = input.touches()[1]
    local mouseX, mouseY = input.mouseDelta()
    if touch and not ui.usingPointer() and stage:contains({touch.x, touch.y}) then
        self.cursorX, self.cursorY = touch.x, touch.y
        selected = selected or touch.phase == 'began'
    elseif mouseX ~= 0 or mouseY ~= 0 or input.mousePressed('left') then
        local x, y = input.mousePosition()
        if stage:contains({x, y}) and not ui.usingPointer() then
            self.cursorX, self.cursorY = x, y
            selected = selected or input.mousePressed('left')
        end
    end

    local moveX, moveY = input.vector('move')
    self.cursorX = m.clamp(self.cursorX + moveX * Picking.cursorSpeed * dt, stage.x, stage:right())
    self.cursorY = m.clamp(self.cursorY + moveY * Picking.cursorSpeed * dt, stage.y, stage:bottom())
    return selected
end

function Picking:update(dt)
    Picking.super.update(self, dt)
    if not self.stage then
        return
    end
    local selected = self:moveCursor(dt, self.stage)
    local camera = self.camera
    camera.rotation = camera.rotation + input.value('rotate') * dt
    local zoom = 1 + input.value('zoom') * dt
    camera.zoom = {camera.zoom.x * zoom, camera.zoom.y * zoom}

    self.worldX, self.worldY = camera:screenToWorld(self.cursorX, self.cursorY)
    self.hovered = self:objectAt(self.worldX, self.worldY)
    if selected then
        self.selected = self.hovered
    end
    self:status(string.format('Screen %.0f, %.0f is world %.1f, %.1f   Under it: %s', self.cursorX, self.cursorY, self.worldX, self.worldY, self.hovered and self.hovered.name or 'nothing'))
end

function Picking:draw(area)
    graphics2d.drawRect({-1000, -600, 2000, 1200}, '#FF2A3040')
    for x = -1000, 1000, 100 do
        graphics2d.drawLine(x, -600, x, 600, 2, '#20FFFFFF')
    end
    for y = -600, 600, 100 do
        graphics2d.drawLine(-1000, y, 1000, y, 2, '#20FFFFFF')
    end
    for _, object in ipairs(self.objects) do
        graphics2d.drawCircle(object.x, object.y, object.radius, object.color, {layer = 1})
        if object == self.hovered or object == self.selected then
            graphics2d.drawRing(object.x, object.y, object.radius + 8, 6, object == self.selected and '#FFFFFFFF' or '#A0FFFFFF', {layer = 2})
        end
    end
    graphics2d.drawCircle(self.worldX, self.worldY, 6, '#FFFF4040', {layer = 3})
end

function Picking:renderUi()
    if not self.cursorX then
        return
    end
    graphics2d.beginScreen()
    graphics2d.pushClip(self.stage)
    local selected = self.selected
    if selected then
        local x, y = self.camera:worldToScreen(selected.x, selected.y)
        graphics2d.drawText(nil, string.format('%s at %.0f, %.0f', selected.name, selected.x, selected.y), x, y - selected.radius * self.camera.zoom.x - 30, {size = 30, anchor = {0.5, 1}, outlineWidth = 3})
    end
    graphics2d.drawRing(self.cursorX, self.cursorY, 14, 3, '#C0FFFFFF', {layer = 100})
    graphics2d.drawCircle(self.cursorX, self.cursorY, 3, '#FFFFFFFF', {layer = 100})
    graphics2d.popClip()
end

return Picking
