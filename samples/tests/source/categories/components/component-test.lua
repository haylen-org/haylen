-- The base of the component tests: a world of 1920 by 1080 around the origin, painted land under a day sky for the targets to stand on, a pointer that follows the mouse, a finger, the arrows, WASD, the directional pad and the sticks, and targets drawn from the sprites of the test project that a click, a tap or the fire button hits.
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
-- The clouds drift this many units a second, and the captions of the targets take this style.
ComponentTest.drift = 18
ComponentTest.nameStyle = {size = 40, anchor = {0.5, 0}, outlineWidth = 3, outlineColor = '#FF1E1B33', layer = 20}
ComponentTest.actions = {actions = {
    {name = 'cursor', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'stick:right'}},
    {name = 'press', type = 'button', bindings = {'axis:rightTrigger+'}},
    {name = 'fire', type = 'button', bindings = {'key:e', 'key:enter', 'key:space', 'button:south'}},
}}

function ComponentTest:init(entry)
    ComponentTest.super.init(self, entry)
    self.pointer = Pointer({confined = true})
    self.order = {pivotY = 1, scaleX = 1, scaleY = 1, layer = 1}
    local linear = {filter = 'linear'}
    self.ground = assets.texture('camera/ground.png', linear)
    self.hills = assets.texture('camera/hills.png', linear)
    self.clouds = assets.texture('camera/clouds.png', linear)
    self.groundOptions = {width = 0, height = ComponentTest.groundHeight, pivotX = 0, pivotY = 0, layer = -2}
    self.hillsOptions = {width = 0, height = 0, pivotX = 0, pivotY = 1, layer = -3}
    self.cloudBands = {
        {scale = 0.9, bottom = 0.75, shift = 0, drift = 1, options = {width = 0, height = 0, pivotX = 0, pivotY = 1, color = '#E6FFFFFF', layer = -4}},
        {scale = 0.55, bottom = 1.75, shift = 0.37, drift = 0.6, options = {width = 0, height = 0, pivotX = 0, pivotY = 1, color = '#A0FFFFFF', layer = -4}},
    }
    local sky = graphics.newImage(4, 256)
    sky:fillGradient({angle = math.pi / 2, stops = {{0, '#FF5FB4EC'}, {0.7, '#FFAEDDF6'}, {1, '#FFE4F5FB'}}})
    self.sky = graphics.newTexture(sky, linear)
end

-- Draws the painted land from edge to edge of the visible area: a day sky with drifting clouds, the hills behind and the ground strip with earth below it to the bottom, so the targets stand on grass whatever the shape of the play area.
function ComponentTest:drawGround()
    local area = graphics2d.canvasBounds()
    local height = ComponentTest.groundHeight
    local width = height * self.ground.width / self.ground.height
    local top = ComponentTest.groundLine - height * ComponentTest.groundTop
    graphics2d.drawShape({area.x, area.y, area.width, top - area.y + 40}, {texture = self.sky, layer = -5})

    -- A near band of clouds and a smaller, paler one higher up drift at their own speeds, shifted so their clouds never line up.
    for _, band in ipairs(self.cloudBands) do
        local clouds = band.options
        clouds.height = height * band.scale
        clouds.width = clouds.height * self.clouds.width / self.clouds.height
        local shift = band.shift * clouds.width + haylen.elapsed() * ComponentTest.drift * band.drift
        for x = area.x - (area.x + shift) % clouds.width, area:right(), clouds.width do
            graphics2d.draw(self.clouds, x, top - height * band.bottom, clouds)
        end
    end

    local hills = self.hillsOptions
    hills.height = height * 1.6
    hills.width = hills.height * self.hills.width / self.hills.height
    for x = area.x - area.x % hills.width, area:right(), hills.width do
        graphics2d.draw(self.hills, x, top + height * 0.45, hills)
    end

    self.groundOptions.width = width
    for x = area.x - (area.x % width), area:right(), width do
        graphics2d.draw(self.ground, x, top, self.groundOptions)
    end
    graphics2d.drawRect({area.x, top + height - 1, area.width, math.max(0, area:bottom() - top - height + 1)}, ComponentTest.earth, {layer = -2})
end

-- Draws a caption under a target on a dark plate, large enough to read at any size of the play area and over any ground.
function ComponentTest.name(text, x, y)
    local width, height = graphics2d.measureText(nil, text, ComponentTest.nameStyle)
    graphics2d.drawShape({x - width / 2 - 20, y - 6, width + 40, height + 12}, {radius = (height + 12) / 2, color = '#C81E1B33', layer = 19})
    graphics2d.drawText(nil, text, x, y, ComponentTest.nameStyle)
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
