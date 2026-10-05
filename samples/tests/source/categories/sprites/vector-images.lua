-- Vector images: six SVG icons with fills, strokes, gradients and transforms, in a grid of sizes from 10 to 144, a swarm of 300 icons of random sizes that turn, and one icon that grows and shrinks between 16 and 400. The engine rasterizes each icon at the size it covers on the screen, on worker threads, and keeps the rasters in an atlas, so every icon stays sharp and all of them batch into few draw calls. A texture rasterized once from an icon draws like any sprite.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local Test = require('harness.test')

local VectorImages = haylen.class('VectorImages', Test)

VectorImages.names = {'heart', 'star', 'gear', 'leaf', 'bolt', 'chat'}
VectorImages.sizes = {10, 14, 20, 28, 40, 56, 80, 112, 144}
VectorImages.tints = {'#FFFFFFFF', '#FFFFD070', '#FF8FE0FF', '#FFFF9090'}
VectorImages.swarm = 300
VectorImages.view = {1800, 1000}
VectorImages.code = [[
local gear = assets.vectorImage('sprites/vector/gear.svg')
graphics2d.drawVector(gear, x, y, {width = 40, height = 40, rotation = 0.3, color = '#FFFFD070'})
local texture = gear:rasterize(4):await()  -- A texture of 256 x 256 pixels.]]

function VectorImages:enter()
    self.icons = {}
    for index, name in ipairs(VectorImages.names) do
        self.icons[index] = assets.vectorImage('sprites/vector/' .. name .. '.svg')
    end
    local random = m.random(7)
    self.swarm = {}
    for index = 1, VectorImages.swarm do
        self.swarm[index] = {icon = self.icons[random:integer(1, #self.icons)], x = random:range(-160, 360), y = random:range(-470, 470), size = random:range(10, 48), turn = random:range(-2, 2), tint = VectorImages.tints[random:integer(1, #VectorImages.tints)]}
    end
    self.time = 0
    self.moving = true
    self:frame{
        code = VectorImages.code,
        view = VectorImages.view,
        hint = 'Every icon must stay sharp at every size, with no blur while the big icon grows and no clipped edges on the small ones.',
        controls = {ui.checkbox{id = 'moving', text = 'Turn and grow the icons', checked = true, onChange = function(event) self.moving = event.checked end}},
        focus = 'moving',
    }
    scene.spawn(self, function()
        self.raster = self.icons[3]:rasterize(4):await()
    end)
end

function VectorImages:update(dt)
    VectorImages.super.update(self, dt)
    if self.moving then
        self.time = self.time + dt
    end
    local stats = graphics2d.stats()
    self:status(string.format('Icons %d   Draw calls %d   Sprites %d', #VectorImages.sizes * #self.icons + VectorImages.swarm + 1, stats.drawCalls, stats.sprites))
end

function VectorImages:draw(area)
    local left, top = -880, -470
    for row, icon in ipairs(self.icons) do
        local x = left
        for _, size in ipairs(VectorImages.sizes) do
            graphics2d.drawVector(icon, x + size / 2, top + (row - 0.5) * 156, {width = size, height = size, color = VectorImages.tints[(row - 1) % #VectorImages.tints + 1]})
            x = x + size + 12
        end
    end

    for _, icon in ipairs(self.swarm) do
        graphics2d.drawVector(icon.icon, icon.x, icon.y, {width = icon.size, height = icon.size, rotation = self.time * icon.turn, color = icon.tint})
    end

    local grow = 16 + (400 - 16) * (0.5 - 0.5 * math.cos(self.time * 0.5))
    graphics2d.drawVector(self.icons[1], 640, 40, {width = grow, height = grow})
    Test.caption(string.format('Heart at %.0f units', grow), 520, 440, {size = 26, color = Test.ink})
    if self.raster then
        graphics2d.draw(self.raster, 800, -380, {width = 128, height = 128})
        Test.caption('Rasterized once at 4x', 700, -300, {size = 26, color = Test.ink})
    end
end

return VectorImages
