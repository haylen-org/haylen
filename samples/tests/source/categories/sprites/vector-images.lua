-- Vector images: six SVG icons with fills, strokes, gradients and transforms, in a grid of sizes from 10 to 96, a swarm of 300 icons of many sizes that swirl and turn, and one icon that grows and shrinks. The engine rasterizes each icon at the size it covers on the screen, on worker threads, and keeps the rasters in an atlas, so every icon stays sharp and all of them batch into few draw calls. A texture rasterized once from an icon draws like any sprite. The icons keep their own colors, and only the bolt, which paints with `currentColor`, takes the color of its draw.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local Panels = require('categories.sprites.panels')
local Test = require('harness.test')

local VectorImages = haylen.class('VectorImages', Test)

VectorImages.names = {'heart', 'star', 'gear', 'leaf', 'bolt', 'chat'}
VectorImages.bolt = 5
VectorImages.sizes = {10, 14, 20, 28, 40, 56, 72, 96}
VectorImages.boltColors = {'#FFFFD070', '#FFFFB347', '#FF8FE0FF'}
VectorImages.swarm = 300

-- The cards of each layout in world units around the center of the view, which the camera fits into the play area.
VectorImages.layouts = {
    landscape = {view = {1700, 780}, cards = {sizes = {-830, -370, 510, 740}, swarm = {-300, -370, 630, 740}, grow = {350, -370, 480, 460}, raster = {350, 110, 480, 260}}},
    portrait = {view = {1000, 1880}, cards = {sizes = {-480, -920, 960, 700}, swarm = {-480, -200, 960, 680}, grow = {-480, 500, 470, 420}, raster = {10, 500, 470, 420}}},
}

VectorImages.code = [[
local gear = assets.vectorImage('sprites/vector/gear.svg')
graphics2d.drawVector(gear, x, y, {width = 40, height = 40, rotation = 0.3})
-- The bolt paints with currentColor, so the color of the draw tints it.
graphics2d.drawVector(bolt, x, y, {width = 40, height = 40, color = '#FFFFD070'})
local texture = gear:rasterize(4):await()  -- A texture of 256 by 256 pixels.]]

function VectorImages:enter()
    self.icons = {}
    for index, name in ipairs(VectorImages.names) do
        self.icons[index] = assets.vectorImage('sprites/vector/' .. name .. '.svg')
    end
    -- Every icon of the swarm circles the middle of its card on an orbit of its own, slower the farther out it flies, and breathes in and out along it.
    local random = m.random(7)
    self.swarm = {}
    for index = 1, VectorImages.swarm do
        local which = random:integer(1, #self.icons)
        local radius = math.sqrt(random:range(0.02, 1))
        self.swarm[index] = {
            icon = self.icons[which], radius = radius, angle = random:range(0, math.pi * 2), speed = (0.12 + 0.3 * (1 - radius)) * (random:range(0, 1) < 0.85 and 1 or -1),
            breath = random:range(0, math.pi * 2), size = random:range(16, 46), turn = random:range(-1.6, 1.6),
            color = which == VectorImages.bolt and VectorImages.boltColors[random:integer(1, #VectorImages.boltColors)] or nil,
        }
    end
    self.time = 0
    self.moving = true
    self.layout = VectorImages.layouts.landscape
    self:frame{
        code = VectorImages.code,
        view = self.layout.view,
        hint = 'Every icon must stay sharp at every size, with no blur while the heart grows and no clipped edges on the small ones.',
        controls = {ui.checkbox{id = 'moving', text = 'Turn and grow the icons', checked = true, onChange = function(event) self.moving = event.checked end}},
        focus = 'moving',
    }
    scene.spawn(self, function()
        self.raster = self.icons[3]:rasterize(4):await()
    end)
end

-- Picks the layout that draws the cards larger in the play area.
function VectorImages:resize(area)
    self.layout = Panels.layout(VectorImages.layouts, area)
    self.view = self.layout.view
end

function VectorImages:update(dt)
    VectorImages.super.update(self, dt)
    if self.moving then
        self.time = self.time + dt
    end
    local count = #VectorImages.sizes * #self.icons + VectorImages.swarm + (self.raster and 2 or 1)
    self:status(string.format('%d icons draw in %d draw calls.', count, graphics2d.stats().drawCalls))
end

function VectorImages:drawSizes(rect)
    local inner = Panels.card(rect, 'Every size from 10 to 96 units')
    local width = (#VectorImages.sizes - 1) * 14
    for _, size in ipairs(VectorImages.sizes) do
        width = width + size
    end
    local rowHeight = inner[4] / #self.icons
    for row, icon in ipairs(self.icons) do
        local x, y = inner[1] + (inner[3] - width) / 2, inner[2] + (row - 0.5) * rowHeight
        local color = row == VectorImages.bolt and VectorImages.boltColors[1] or nil
        for _, size in ipairs(VectorImages.sizes) do
            graphics2d.drawVector(icon, x + size / 2, y, {width = size, height = size, color = color})
            x = x + size + 14
        end
    end
end

function VectorImages:drawSwarm(rect)
    local inner = Panels.card(rect, 'A swarm of 300 that turn')
    local centerX, centerY = inner[1] + inner[3] / 2, inner[2] + inner[4] / 2
    local spreadX, spreadY = inner[3] / 2 - 26, inner[4] / 2 - 26
    local time = self.time
    for _, icon in ipairs(self.swarm) do
        local angle = icon.angle + time * icon.speed
        local radius = icon.radius * (0.92 + 0.08 * math.sin(time * 0.9 + icon.breath))
        local x, y = centerX + math.cos(angle) * radius * spreadX, centerY + math.sin(angle) * radius * spreadY
        graphics2d.drawVector(icon.icon, x, y, {width = icon.size, height = icon.size, rotation = time * icon.turn, color = icon.color})
    end
end

function VectorImages:drawGrow(rect)
    local inner = Panels.card(rect, 'One icon that grows')
    local largest = math.min(inner[3], inner[4] - 56)
    local grow = 16 + (largest - 16) * (0.5 - 0.5 * math.cos(self.time * 0.5))
    local centerX, centerY = inner[1] + inner[3] / 2, inner[2] + (inner[4] - 56) / 2
    graphics2d.drawVector(self.icons[1], centerX, centerY, {width = grow, height = grow})
    Panels.caption(string.format('Heart at %.0f units', grow), centerX, inner[2] + inner[4] - 32)
end

function VectorImages:drawRaster(rect)
    local inner = Panels.card(rect, 'Rasterized once into a texture')
    if not self.raster then
        return
    end
    local size = math.min(128, inner[4] - 56)
    local centerX = inner[1] + inner[3] / 2
    graphics2d.draw(self.raster, centerX, inner[2] + size / 2, {width = size, height = size})
    Panels.caption(string.format('%d by %d pixels at 4 times its size', self.raster.width, self.raster.height), centerX, inner[2] + inner[4] - 32)
end

function VectorImages:draw(area)
    local cards = self.layout.cards
    self:drawSizes(cards.sizes)
    self:drawSwarm(cards.swarm)
    self:drawGrow(cards.grow)
    self:drawRaster(cards.raster)
end

return VectorImages
