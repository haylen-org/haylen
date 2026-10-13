-- Images and runtime atlases: a badge composed from images on the CPU off the frame thread, many sprites drawn from four separate textures or from one atlas packed at run time, which turns thousands of draw calls into one, and a checker drawn small with and without mipmaps.
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local SpriteTest = require('categories.sprites.sprite-test')

local RuntimeAtlases = haylen.class('RuntimeAtlases', SpriteTest)

RuntimeAtlases.images = {'hero', 'rock', 'tree', 'bunny'}
RuntimeAtlases.count = 2000

RuntimeAtlases.code = [[
local builder = animation2d.newAtlasBuilder()
builder:add('hero', assets.loadAsync('sprites/images/hero.png', 'image'):await())
local atlas = builder:buildAsync({filter = 'linear'}):await()
graphics.newTexture(checker, {filter = 'linear', mipmaps = true})]]

-- Makes a checker of two colors in squares of two pixels, which shimmers into patterns when drawn small without mipmaps.
function RuntimeAtlases.checker()
    local image = graphics.newImage(512, 512, '#FF1E2A4A')
    for row = 0, 255 do
        for column = row % 2, 255, 2 do
            image:fill('#FFF2C14E', {column * 2, row * 2, 2, 2})
        end
    end
    return image
end

function RuntimeAtlases:enter()
    self.useAtlas = true
    self.textures = {}
    for index, name in ipairs(RuntimeAtlases.images) do
        self.textures[index] = SpriteTest.texture('images/' .. name .. '.png')
    end
    local checker = RuntimeAtlases.checker()
    self.plain = graphics.newTexture(checker, {filter = 'linear'})
    self.mipmapped = graphics.newTexture(checker, {filter = 'linear', mipmaps = true})
    self.time = 0
    self:frame{
        code = RuntimeAtlases.code,
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

        local badge = graphics.newImage(160, 160)
        badge:fill('#FF2E7D32', {10, 10, 140, 140})
        badge:fill('#FF66BB6A', {20, 20, 120, 120})
        badge = badge:blitAsync(images.hero, 32, 32, {blend = true}):await()
        badge:bleed()
        self.badge = graphics.newTexture(badge, {filter = 'linear'})
    end)
end

function RuntimeAtlases:update(dt)
    RuntimeAtlases.super.update(self, dt)
    self.time = self.time + dt
    local stats = graphics2d.stats()
    local packed = self.packedAtlas and string.format('Atlas %d by %d with %d frames', self.packedAtlas.texture.width, self.packedAtlas.texture.height, #self.packedAtlas:frameNames()) or 'Packing the atlas'
    self:status(string.format('%s   Sprites %d   Draw calls %d   Texture switches %d', packed, RuntimeAtlases.count, stats.drawCalls, stats.textureSwitches))
end

function RuntimeAtlases:draw(area)
    local width, height = area.width, area.height
    local names = RuntimeAtlases.images

    -- Sprites that alternate their images break every batch when each image is a texture of its own.
    for index = 0, RuntimeAtlases.count - 1 do
        local which = index % #names + 1
        local x = width * 0.25 + math.cos(index * 0.37 + self.time * 0.3) * (index % 97) * 2.4
        local y = height * 0.5 + math.sin(index * 0.53 + self.time * 0.3) * (index % 89) * 2.2
        if self.useAtlas and self.packedAtlas then
            graphics2d.draw(self.packedAtlas.texture, x, y, {source = self.packedAtlas:source(names[which]), scaleX = 0.35, scaleY = 0.35})
        else
            graphics2d.draw(self.textures[which], x, y, {scaleX = 0.35, scaleY = 0.35})
        end
    end

    if self.badge then
        graphics2d.draw(self.badge, width * 0.62, height * 0.25, {layer = 1})
    end
    graphics2d.drawText(nil, 'Composed off the frame thread', width * 0.62, height * 0.25 + 95, {size = 18, color = '#FFFFFFFF', align = 'center', layer = 1})

    -- The checker drawn at a third of its size samples a few of its pixels without mipmaps, which shows patterns, and the average of its squares with them.
    local scale = 0.3 + math.sin(self.time * 0.5) * 0.08
    graphics2d.draw(self.plain, width * 0.5, height * 0.72, {scaleX = scale, scaleY = scale, layer = 1})
    graphics2d.draw(self.mipmapped, width * 0.78, height * 0.72, {scaleX = scale, scaleY = scale, layer = 1})
    graphics2d.drawText(nil, 'Without mipmaps', width * 0.5, height * 0.72 + 120, {size = 16, color = '#FFFFFFFF', align = 'center', layer = 1})
    graphics2d.drawText(nil, 'With mipmaps', width * 0.78, height * 0.72 + 120, {size = 16, color = '#FFFFFFFF', align = 'center', layer = 1})
end

return RuntimeAtlases
