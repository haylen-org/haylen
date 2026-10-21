-- Lit render targets: a render target canvas with ambient light renders lit and post-processed offscreen, and its texture shows anywhere, here as two monitors on a wall, one watching a cellar lit by a torch and one a garden lit by fireflies.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local lighting2d = require('haylen.lighting2d')

local LightingTest = require('categories.lighting.lighting-test')
local Room = require('categories.lighting.room')

local RenderTargets = haylen.class('RenderTargets', LightingTest)

RenderTargets.code = [[
local garden = graphics.newRenderTarget(800, 450, {filter = 'linear'})
local options = {ambientLight = '#FF303C5C', postProcess = {vignetteStrength = 0.7}}
graphics2d.beginTarget(garden, camera, options)
graphics2d.drawLight(firefly)  -- Lit offscreen.
graphics2d.beginWorld(camera)  -- Then shown in another canvas.
graphics2d.draw(garden.texture, 80, -260, {pivotX = 0, pivotY = 0})]]

RenderTargets.width = 800
RenderTargets.height = 450
RenderTargets.grass = {2, 2, 128, 128}

function RenderTargets:init(entry)
    RenderTargets.super.init(self, entry)
    local width, height = RenderTargets.width, RenderTargets.height
    self.tiles = assets.texture(Room.tileset, {filter = 'linear'})
    self.cellar = graphics.newRenderTarget(width, height, {filter = 'linear'})
    self.garden = graphics.newRenderTarget(width, height, {filter = 'linear'})
    self.screens = {{target = self.cellar, title = 'Cellar', x = -880, y = -250}, {target = self.garden, title = 'Garden', x = 80, y = -250}}
    self.targetCamera = graphics2d.newCamera()
    self.targetCamera.position = {width / 2, height / 2}
    self.torch = lighting2d.newLight({x = 400, y = 225, radius = 520, color = '#FFFFB060', intensity = 1.4, shadows = true, shadowFilter = 'pcf13', shadowSmoothness = 1})
    self.crates = {}
    for _, place in ipairs({{150, 120, 90}, {300, 320, 76}, {540, 140, 104}, {660, 330, 80}}) do
        local x, y, size = place[1], place[2], place[3]
        local half = size / 2
        self.crates[#self.crates + 1] = {x = x, y = y, size = size, occluder = lighting2d.newOccluder({points = {x - half, y - half, x + half, y - half, x + half, y + half, x - half, y + half}, cull = 'counterClockwise'})}
    end
    self.bushes = {{110, 360, 150}, {280, 90, 120}, {470, 400, 170}, {640, 110, 140}, {740, 300, 120}}
    self.fireflies = {}
    for index = 1, 14 do
        self.fireflies[index] = {phase = index * 0.7, light = lighting2d.newLight({radius = 90, color = '#FFC8FF60', intensity = 1.2})}
    end
    self.focus = {x = 400, y = 225}
    self.time = 0
end

function RenderTargets:enter()
    self:frame{
        hint = 'Point at a monitor with the mouse, a finger, the arrows, WASD or a stick: the torch of the cellar and the fireflies of the garden follow the cursor inside it.',
        code = RenderTargets.code,
        cursor = true,
        view = {1980, 760},
    }
end

-- Returns the cursor inside a screen in the pixels of its target, or nothing when it points elsewhere.
function RenderTargets:pointInside(screen)
    local x, y = self.cursorX - screen.x, self.cursorY - screen.y
    if x >= 0 and y >= 0 and x <= RenderTargets.width and y <= RenderTargets.height then
        return x, y
    end
end

function RenderTargets:update(dt)
    RenderTargets.super.update(self, dt)
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
    self:status(string.format('Two lit render targets of %d by %d pixels, and %d canvases drew this frame.', RenderTargets.width, RenderTargets.height, graphics2d.stats().canvases))
end

-- Repeats a tile of the tileset over a target.
function RenderTargets:tileFloor(source, size)
    for y = 0, RenderTargets.height, size do
        for x = 0, RenderTargets.width, size do
            graphics2d.draw(self.tiles, x, y, {source = source, width = size, height = size, pivotX = 0, pivotY = 0})
        end
    end
end

function RenderTargets:renderCellar()
    graphics2d.beginTarget(self.cellar, self.targetCamera, {ambientLight = '#FF0C0C14', clear = '#FF000000'})
    self:tileFloor(Room.sources.floor, 120)
    for _, crate in ipairs(self.crates) do
        local size = crate.size / Room.fill.crate
        graphics2d.draw(self.tiles, crate.x, crate.y, {source = Room.sources.crate, width = size, height = size, layer = 1})
        graphics2d.drawOccluder(crate.occluder)
    end
    Room.drawLamp(self.torch.x, self.torch.y, self.torch.color, 22)
    graphics2d.drawLight(self.torch)
end

function RenderTargets:renderGarden()
    graphics2d.beginTarget(self.garden, self.targetCamera, {ambientLight = '#FF303C5C', clear = '#FF000000', postProcess = {vignetteStrength = 0.7, saturation = 1.3}})
    self:tileFloor(RenderTargets.grass, 160)
    for _, bush in ipairs(self.bushes) do
        local size = bush[3] / Room.fill.bush
        graphics2d.draw(self.tiles, bush[1], bush[2], {source = Room.sources.bush, width = size, height = size, layer = 1})
    end
    for _, firefly in ipairs(self.fireflies) do
        graphics2d.draw(graphics2d.lightTexture(), firefly.light.x, firefly.light.y, {width = 28, height = 28, color = '#FFE8FF90', blend = 'additive', layer = 2, emission = 1.5})
        graphics2d.drawCircle(firefly.light.x, firefly.light.y, 3.5, '#FFF4FFC0', {layer = 3, emission = 2})
        graphics2d.drawLight(firefly.light)
    end
end

-- The wall of the monitors is not lit itself.
function RenderTargets:canvas()
    return {}
end

-- The targets draw before the wall that shows them.
function RenderTargets:render()
    if self.area then
        self:renderCellar()
        self:renderGarden()
    end
    RenderTargets.super.render(self)
end

-- Draws a monitor around a screen: a slate case with a dark bezel, a stand and the name of what it watches.
function RenderTargets.drawMonitor(screen)
    local width, height = RenderTargets.width, RenderTargets.height
    local x, y = screen.x, screen.y
    graphics2d.drawPolygon({{x + width / 2 - 70, y + height + 30}, {x + width / 2 + 70, y + height + 30}, {x + width / 2 + 100, y + height + 110}, {x + width / 2 - 100, y + height + 110}}, '#FF1E2138', {layer = 1})
    graphics2d.drawShape({x + width / 2 - 170, y + height + 100, 340, 26}, {radius = 13, color = '#FF3A4066', borderWidth = 4, borderColor = '#FF15172A', layer = 1})
    graphics2d.drawShape({x - 30, y - 30, width + 60, height + 60}, {radius = 28, color = '#FF2F3352', borderWidth = 6, borderColor = '#FF15172A', layer = 2})
    graphics2d.drawShape({x - 8, y - 8, width + 16, height + 16}, {radius = 10, color = '#FF15172A', layer = 3})
    graphics2d.draw(screen.target.texture, x, y, {pivotX = 0, pivotY = 0, layer = 4})
    graphics2d.drawCircle(x + width - 14, y + height + 16, 5, '#FF6FDCA0', {layer = 4})
    graphics2d.drawText(nil, screen.title, x + width / 2, y - 60, {size = 44, anchor = {0.5, 1}, color = '#FFE8EAF2', layer = 4})
end

function RenderTargets:draw(area)
    local canvas = graphics2d.canvasBounds()
    graphics2d.drawRect(canvas, '#FF232838')
    graphics2d.drawRect({canvas.x, 320, canvas.width, canvas:bottom() - 320}, '#FF1B1F2E')
    for _, screen in ipairs(self.screens) do
        RenderTargets.drawMonitor(screen)
    end
end

return RenderTargets
