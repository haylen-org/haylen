-- Images and runtime atlases: a medal composed on the CPU off the frame thread from the pieces of one image and the hero, a meadow of sprites drawn from four separate textures or from one atlas packed at run time, which turns hundreds of draw calls into one, and graph paper drawn small with and without mipmaps.
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local Panels = require('categories.sprites.panels')
local SpriteTest = require('categories.sprites.sprite-test')
local Terrain = require('categories.sprites.terrain')

local RuntimeAtlases = haylen.class('RuntimeAtlases', SpriteTest)

RuntimeAtlases.images = {'hero', 'rock', 'tree', 'bunny'}
-- The height of each image in the meadow, in units, and its share of the sprites.
RuntimeAtlases.heights = {hero = 44, rock = 24, tree = 62, bunny = 26}
RuntimeAtlases.shares = {hero = 0.3, rock = 0.25, tree = 0.15, bunny = 0.3}
RuntimeAtlases.count = 600
RuntimeAtlases.medal = {0, 0, 324, 277}
RuntimeAtlases.ribbon = {328, 0, 324, 99}
RuntimeAtlases.layouts = {
    landscape = {view = {1700, 800}, cards = {meadow = {-830, -380, 980, 760}, badge = {170, -380, 660, 410}, mipmaps = {170, 50, 660, 330}}},
    portrait = {view = {900, 1700}, cards = {meadow = {-430, -830, 860, 1000}, badge = {-430, 190, 420, 640}, mipmaps = {10, 190, 420, 640}}},
}

RuntimeAtlases.code = [[
local builder = animation2d.newAtlasBuilder()
builder:add('hero', assets.loadAsync('sprites/images/hero.png', 'image'):await())
local atlas = builder:buildAsync({padding = 4, extrude = 2, filter = 'linear'}):await()
badge = badge:blitAsync(hero, 74, 46, {blend = true}):await()  -- Composed on the task pool.
graphics.newTexture(paper, {filter = 'linear', mipmaps = true})]]

-- Graph paper with thin lines every 4 pixels and bold ones every 32, whose fine lines break into stripes and moiré when drawn small without mipmaps.
function RuntimeAtlases.paper()
    local image = graphics.newImage(512, 512, '#FFF6EFD8')
    for line = 0, 511, 4 do
        image:fill('#FF9CB3D1', {line, 0, 1, 512})
        image:fill('#FF9CB3D1', {0, line, 512, 1})
    end
    for line = 0, 511, 32 do
        image:fill('#FF3E5C96', {line, 0, 2, 512})
        image:fill('#FF3E5C96', {0, line, 512, 2})
    end
    return image
end

-- Returns the image whose share of the sprites holds a number from 0 to 1.
function RuntimeAtlases.pick(value)
    for index, name in ipairs(RuntimeAtlases.images) do
        value = value - RuntimeAtlases.shares[name]
        if value <= 0 then
            return index
        end
    end
    return #RuntimeAtlases.images
end

function RuntimeAtlases:enter()
    self.useAtlas = true
    self.textures = {}
    for index, name in ipairs(RuntimeAtlases.images) do
        self.textures[index] = SpriteTest.texture('images/' .. name .. '.png')
    end
    local paper = RuntimeAtlases.paper()
    self.plain = graphics.newTexture(paper, {filter = 'linear'})
    self.mipmapped = graphics.newTexture(paper, {filter = 'linear', mipmaps = true})
    self.time = 0
    self.layout = RuntimeAtlases.layouts.landscape

    -- Every sprite of the meadow takes a cell of a jittered grid, so the meadow fills evenly, and the sprites draw from the back to the front.
    local random = m.random(5)
    local columns = 30
    local rows = math.ceil(RuntimeAtlases.count / columns)
    self.spots = {}
    for index = 0, RuntimeAtlases.count - 1 do
        local column, row = index % columns, index // columns
        self.spots[index + 1] = {x = (column + random:range(0.1, 0.9)) / columns, y = (row + random:range(0.1, 0.9)) / rows, which = RuntimeAtlases.pick(random:range(0, 1))}
    end
    table.sort(self.spots, function(a, b) return a.y < b.y end)
    self:frame{
        code = RuntimeAtlases.code,
        view = self.layout.view,
        hint = 'Switch between separate textures and the packed atlas and compare the draw calls.',
        controls = {ui.toggle{id = 'atlas', text = 'Draw from the packed atlas', checked = true, onChange = function(event) self.useAtlas = event.checked end}},
        focus = 'atlas',
    }

    -- The images load and compose on the task pool, and the atlas packs there too.
    scene.spawn(self, function()
        local builder = animation2d.newAtlasBuilder()
        local images = {}
        for _, name in ipairs(RuntimeAtlases.images) do
            images[name] = assets.loadAsync('sprites/images/' .. name .. '.png', 'image'):await()
            builder:add(name, images[name])
        end
        self.packedAtlas = builder:buildAsync({padding = 4, extrude = 2, filter = 'linear'}):await()

        local parts = assets.loadAsync('sprites/images/badge-parts.png', 'image'):await()
        local badge = graphics.newImage(340, 350)
        badge = badge:blitAsync(parts, 8, 0, {source = RuntimeAtlases.medal, blend = true}):await()
        badge = badge:blitAsync(images.hero, 74, 46, {blend = true}):await()
        badge = badge:blitAsync(parts, 8, 251, {source = RuntimeAtlases.ribbon, blend = true}):await()
        badge:bleed()
        self.badge = graphics.newTexture(badge, {filter = 'linear', mipmaps = true})
    end)
end

-- Picks the layout that draws the cards larger in the play area and lays the grass of the meadow under its card.
function RuntimeAtlases:resize(area)
    local layout = Panels.layout(RuntimeAtlases.layouts, area)
    self.view = layout.view
    if layout == self.layout and self.grass then
        return
    end
    self.layout = layout
    local rect = layout.cards.meadow
    local inner = {rect[1] + 20, rect[2] + 66, rect[3] - 40, rect[4] - 86}
    local columns, rows = math.floor(inner[3] / 80), math.floor(inner[4] / 80)
    local width, height = inner[3] / columns, inner[4] / rows
    self.meadow = inner
    local size = math.min(width, height)
    local grass = Terrain.batches(columns, rows, size, inner[1] + (inner[3] - columns * size) / 2, inner[2] + (inner[4] - rows * size) / 2, function()
        return Terrain.grass
    end)
    self.grass = grass:bake()
end

function RuntimeAtlases:update(dt)
    RuntimeAtlases.super.update(self, dt)
    self.time = self.time + dt
    local stats = graphics2d.stats()
    local atlas = self.packedAtlas
    local packed = atlas and string.format('The atlas packs %d images into %d by %d pixels', #atlas:frameNames(), atlas.texture.width, atlas.texture.height) or 'The atlas is packing'
    self:status(string.format('%s, and the stage draws in %d calls with %d texture switches.', packed, stats.drawCalls, stats.textureSwitches))
end

-- Sprites that alternate their images break every batch when each image is a texture of its own.
function RuntimeAtlases:drawMeadow(rect)
    Panels.card(rect, 'A meadow of 600 sprites')
    local meadow = self.meadow
    graphics2d.drawStatic(self.grass, 0, 0, {layer = -5})
    local names, heights = RuntimeAtlases.images, RuntimeAtlases.heights
    local atlas = self.useAtlas and self.packedAtlas
    for _, spot in ipairs(self.spots) do
        local name = names[spot.which]
        local texture = self.textures[spot.which]
        local scale = heights[name] / texture.height
        local x = meadow[1] + 22 + spot.x * (meadow[3] - 44)
        local y = meadow[2] + 66 + spot.y * (meadow[4] - 72)
        if atlas then
            graphics2d.draw(atlas.texture, x, y, {source = atlas:source(name), scaleX = scale, scaleY = scale, pivotY = 1})
        else
            graphics2d.draw(texture, x, y, {scaleX = scale, scaleY = scale, pivotY = 1})
        end
    end
end

-- The medal with the hero, composed on the task pool, and a name on its ribbon drawn as text.
function RuntimeAtlases:drawBadge(rect)
    local inner = Panels.card(rect, 'Composed off the frame thread')
    if not self.badge then
        return
    end
    local height = math.min(inner[4], inner[3] * 350 / 340)
    local scale = height / 350
    local x, top = inner[1] + inner[3] / 2, inner[2] + (inner[4] - height) / 2
    graphics2d.draw(self.badge, x, top, {width = 340 * scale, height = height, pivotY = 0, layer = 1})
    graphics2d.drawText(nil, 'Champion', x, top + 290 * scale, {size = 34 * scale, color = '#FFFFFFFF', outlineWidth = 2 * scale, outlineColor = '#FF8A1F1A', anchor = {0.5, 0.5}, layer = 2})
end

-- The same paper drawn small with and without mipmaps, slowly changing its size so the patterns of the plain one crawl.
function RuntimeAtlases:drawMipmaps(rect)
    local inner = Panels.card(rect, 'Graph paper drawn small')
    local vertical = inner[4] > inner[3] * 1.2
    local slot = vertical and inner[4] / 2 or inner[3] / 2
    local largest = math.min(vertical and slot - 52 or inner[4] - 52, (vertical and inner[3] or slot) - 30)
    local size = largest * (0.82 + math.sin(self.time * 0.6) * 0.18)
    for index, texture in ipairs({self.plain, self.mipmapped}) do
        local x = vertical and inner[1] + inner[3] / 2 or inner[1] + slot * (index - 0.5)
        local y = vertical and inner[2] + slot * (index - 1) + (slot - 52) / 2 or inner[2] + (inner[4] - 52) / 2
        graphics2d.draw(texture, x, y, {width = size, height = size, layer = 1})
        Panels.caption(index == 1 and 'Without mipmaps' or 'With mipmaps', x, y + largest / 2 + 12)
    end
end

function RuntimeAtlases:draw(area)
    local cards = self.layout.cards
    self:drawMeadow(cards.meadow)
    self:drawBadge(cards.badge)
    self:drawMipmaps(cards.mipmaps)
end

return RuntimeAtlases
