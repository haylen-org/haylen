-- Lit render targets: a render target canvas with ambient light renders lit and post-processed offscreen, and its texture shows anywhere, here as two screens on a wall.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')

local sample = require('sample')

local RenderTargets = haylen.class('RenderTargets', sample.Test)

RenderTargets.hints = 'Point at a screen with the mouse, a finger, WASD or the left stick: the torch of the cellar and the fireflies of the garden follow the cursor inside it.'

RenderTargets.width = 640
RenderTargets.height = 360

function RenderTargets:init(entry)
    RenderTargets.super.init(self, entry)
    self.cursor = sample.Cursor()
    self.cellar = graphics.newRenderTarget(RenderTargets.width, RenderTargets.height, {filter = 'linear'})
    self.garden = graphics.newRenderTarget(RenderTargets.width, RenderTargets.height, {filter = 'linear'})
    self.screens = {{target = self.cellar, x = 180, y = 380}, {target = self.garden, x = 1100, y = 380}}
    self.camera = graphics2d.newCamera()
    self.camera.position = {RenderTargets.width / 2, RenderTargets.height / 2}
    self.torch = lighting2d.newLight({x = 320, y = 180, radius = 420, color = '#FFFFB060', intensity = 1.4, shadows = true, shadowFilter = 'pcf13', shadowSmoothness = 1})
    self.crates = {}
    for index = 0, 3 do
        local x, y = 120 + index * 130, 110 + (index % 2) * 140
        self.crates[#self.crates + 1] = {x = x, y = y, occluder = lighting2d.newOccluder({points = {x, y, x + 50, y, x + 50, y + 50, x, y + 50}, cull = 'counterClockwise'})}
    end
    self.fireflies = {}
    for index = 1, 12 do
        self.fireflies[index] = {phase = index * 0.7, light = lighting2d.newLight({radius = 70, color = '#FFC8FF60', intensity = 1.2})}
    end
    self.focus = {x = 320, y = 180}
    self.time = 0
end

-- Returns the cursor inside a screen in the pixels of its target, or `nil` when it points elsewhere.
function RenderTargets:pointInside(screen)
    local x, y = self.cursor.x - screen.x, self.cursor.y - screen.y
    if x >= 0 and y >= 0 and x <= RenderTargets.width and y <= RenderTargets.height then
        return x, y
    end
end

function RenderTargets:update(dt)
    self.cursor:update(dt)
    self.time = self.time + dt

    local x, y = self:pointInside(self.screens[1])
    if x then
        self.torch.x, self.torch.y = x, y
    end
    x, y = self:pointInside(self.screens[2])
    if x then
        self.focus.x, self.focus.y = x, y
    end
    for _, firefly in ipairs(self.fireflies) do
        local angle = self.time * 0.8 + firefly.phase
        firefly.light.x = self.focus.x + math.cos(angle) * (60 + firefly.phase * 12)
        firefly.light.y = self.focus.y + math.sin(angle * 1.3) * (40 + firefly.phase * 6)
    end
    self:setStatus(string.format('two %dx%d lit render targets, %d canvases this frame', RenderTargets.width, RenderTargets.height, graphics2d.stats().canvases))
end

function RenderTargets:renderCellar()
    graphics2d.beginTarget(self.cellar, self.camera, {ambientLight = '#FF0C0C14', clear = '#FF000000'})
    graphics2d.drawRect({0, 0, RenderTargets.width, RenderTargets.height}, '#FF6A625A')
    for _, crate in ipairs(self.crates) do
        graphics2d.drawRect({crate.x, crate.y, 50, 50}, '#FF8A5A34', {layer = 1})
        graphics2d.drawOccluder(crate.occluder)
    end
    graphics2d.drawLight(self.torch)
end

function RenderTargets:renderGarden()
    graphics2d.beginTarget(self.garden, self.camera, {ambientLight = '#FF303C5C', clear = '#FF000000', postProcess = {vignetteStrength = 0.7, saturation = 1.3}})
    graphics2d.drawRect({0, 0, RenderTargets.width, RenderTargets.height}, '#FF3A6A3A')
    for index = 0, 9 do
        graphics2d.drawCircle(40 + index * 64, 300 - (index % 3) * 20, 34, '#FF2A5A2A', {layer = 1})
    end
    for _, firefly in ipairs(self.fireflies) do
        graphics2d.drawCircle(firefly.light.x, firefly.light.y, 4, '#FFE8FF90', {layer = 2, emission = 2})
        graphics2d.drawLight(firefly.light)
    end
end

function RenderTargets:render()
    self:renderCellar()
    self:renderGarden()
    graphics2d.beginScreen()
    graphics2d.drawRect(graphics2d.canvasBounds(), '#FF2A2630')
    for _, screen in ipairs(self.screens) do
        graphics2d.drawRect({screen.x - 20, screen.y - 20, RenderTargets.width + 40, RenderTargets.height + 40}, '#FF101014', {layer = 1})
        graphics2d.draw(screen.target.texture, screen.x, screen.y, {pivotX = 0, pivotY = 0, layer = 2})
    end
end

function RenderTargets:renderUi()
    graphics2d.beginScreen()
    self.cursor:draw()
end

return RenderTargets
