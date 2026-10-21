-- The base of the component tests: a world of 1920 by 1080 around the origin, a pointer that follows the mouse, a finger, the arrows, WASD, the directional pad and the sticks, and targets drawn from the sprites of the test project that a click, a tap or the fire button hits.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')

local Pointer = require('harness.pointer')
local Test = require('harness.test')

local ComponentTest = haylen.class('ComponentTest', Test)

ComponentTest.view = {1920, 1080}
-- The line the targets stand on, and the painted strip of grass and earth under it, whose grass top sits this share of its height above that line.
ComponentTest.groundLine = 300
ComponentTest.groundHeight = 260
ComponentTest.groundTop = 0.3
ComponentTest.earth = '#FF813F1E'
ComponentTest.actions = {actions = {
    {name = 'cursor', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'stick:right'}},
    {name = 'press', type = 'button', bindings = {'axis:rightTrigger+'}},
    {name = 'fire', type = 'button', bindings = {'key:e', 'key:enter', 'key:space', 'button:south'}},
}}

function ComponentTest:init(entry)
    ComponentTest.super.init(self, entry)
    self.pointer = Pointer({confined = true})
    self.order = {pivotY = 1, scaleX = 1, scaleY = 1, layer = 1}
    self.ground = assets.texture('camera/ground.png', {filter = 'linear'})
    self.groundOptions = {width = 0, height = ComponentTest.groundHeight, pivotX = 0, pivotY = 0, layer = -2}
    local sky = graphics.newImage(4, 256)
    sky:fillGradient({angle = math.pi / 2, stops = {{0, '#FF151A28'}, {1, '#FF2B3554'}}})
    self.sky = graphics.newTexture(sky, {filter = 'linear'})
end

-- Draws the painted ground from edge to edge of the visible area, with earth below it to the bottom, so the targets stand on grass whatever the shape of the play area.
function ComponentTest:drawGround()
    local area = graphics2d.canvasBounds()
    local height = ComponentTest.groundHeight
    local width = height * self.ground.width / self.ground.height
    local top = ComponentTest.groundLine - height * ComponentTest.groundTop
    self.groundOptions.width = width
    graphics2d.drawShape({area.x, area.y, area.width, top - area.y + 40}, {texture = self.sky, layer = -4})
    for x = area.x - (area.x % width), area:right(), width do
        graphics2d.draw(self.ground, x, top, self.groundOptions)
    end
    graphics2d.drawRect({area.x, top + height - 1, area.width, math.max(0, area:bottom() - top - height + 1)}, ComponentTest.earth, {layer = -3})
end

-- Draws a caption under a target, large enough to read at any size of the play area.
function ComponentTest.name(text, x, y)
    graphics2d.drawText(nil, text, x, y, {size = 34, anchor = {0.5, 0}, outlineWidth = 4, outlineColor = '#FF1E1B33', layer = 20})
end

function ComponentTest:enter()
    self:loadActions(ComponentTest.actions)
end

-- Returns a target: a sprite of the project standing at `x`, `y` at a scale, which shakes for a moment when it is hit.
function ComponentTest.target(name, x, y, scale)
    local texture = assets.texture('sprites/images/' .. name .. '.png', {filter = 'linear'})
    return {texture = texture, x = x, y = y, scale = scale, shake = 0, width = texture.width * scale, height = texture.height * scale}
end

function ComponentTest:update(dt)
    ComponentTest.super.update(self, dt)
    self.pointer:update(dt, self)
end

-- Tells whether a click, a tap or the fire button pressed this frame.
function ComponentTest:pressed()
    return self.pointer.pressed or input.pressed('fire')
end

-- Returns the target under the pointer, or nothing.
function ComponentTest:under(targets)
    local x, y = self.pointer.worldX, self.pointer.worldY
    for _, target in ipairs(targets) do
        if math.abs(x - target.x) < target.width / 2 and y < target.y and y > target.y - target.height then
            return target
        end
    end
end

function ComponentTest.hit(target)
    target.shake = 0.25
end

function ComponentTest:drawTarget(target, dt)
    target.shake = math.max(0, target.shake - dt)
    local offset = target.shake > 0 and math.sin(target.shake * 90) * 10 * target.shake / 0.25 or 0
    self.order.scaleX, self.order.scaleY = target.scale, target.scale
    graphics2d.draw(target.texture, target.x + offset, target.y, self.order)
    graphics2d.drawEllipse(target.x, target.y, target.width * 0.42, 14, '#40000000')
end

function ComponentTest:renderUi()
    if self.stage then
        self.pointer:draw()
    end
end

return ComponentTest
