-- Recoloring by parts: one adventurer whose hat, shirt, trousers and boots a mask marks in red, green, blue and yellow, drawn in many outfits from the same two images. The colors travel with each sprite and the GPU recolors every pixel, so the whole crowd of a sprite batch draws in one call whatever its colors, and the shading of the art stays under every color.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local Test = require('harness.test')

local Recolor = haylen.class('Recolor', Test)

Recolor.view = {1800, 1000}
Recolor.crowd = 120
Recolor.palette = {'#FFE04848', '#FF3FA34D', '#FF3D6FD9', '#FFE8C547', '#FF8A4FC9', '#FFF08A30', '#FF2BB5B0', '#FF5A3A28', '#FF30343C', '#FFF2F0E8', '#FFD9577F', '#FF7FB33F'}
Recolor.code = [[
local hero = graphics2d.newSprite(assets.texture('sprites/images/outfit_base.png'), {partMask = assets.texture('sprites/images/outfit_mask.png')})
hero.partColors = {red = '#FF8A4FC9', green = '#FFF08A30', blue = '#FF30343C', yellow = '#FF5A3A28'}
crowd:draw({partMask = mask})  -- Every sprite of the batch keeps its own part colors.]]

function Recolor:enter()
    self.base = assets.texture('sprites/images/outfit_base.png', {filter = 'linear'})
    self.mask = assets.texture('sprites/images/outfit_mask.png', {filter = 'linear'})
    self.random = m.random(11)
    self.outfits = {}
    for index = 1, 8 do
        self.outfits[index] = self:outfit()
    end
    self.outfits[1] = {red = '#FFFFFFFF', green = '#FFFFFFFF', blue = '#FFFFFFFF', yellow = '#FFFFFFFF'}
    self.crowd = graphics2d.newSpriteBatch(self.base)
    self.walkers = {}
    for index = 1, Recolor.crowd do
        local walker = {x = self.random:range(-880, 880), y = self.random:range(160, 470), speed = self.random:range(-90, 90)}
        walker.id = self.crowd:add({x = walker.x, y = walker.y, width = 64, height = 64, partColors = self:outfit()})
        self.walkers[index] = walker
    end
    self:frame{
        code = Recolor.code,
        view = Recolor.view,
        hint = 'The first adventurer keeps the base art, which is grey where the parts are. Every other outfit keeps the shading and outlines of the art, and the crowd draws in one call.',
        controls = {ui.button{id = 'shuffle', text = 'New outfits', onClick = function() self:shuffle() end}},
        focus = 'shuffle',
    }
end

-- Picks four different colors of the palette, one for each part.
function Recolor:outfit()
    local colors = {}
    for index = 1, #Recolor.palette do
        colors[index] = Recolor.palette[index]
    end
    self.random:shuffle(colors)
    return {red = colors[1], green = colors[2], blue = colors[3], yellow = colors[4]}
end

function Recolor:shuffle()
    for index = 2, #self.outfits do
        self.outfits[index] = self:outfit()
    end
    for _, walker in ipairs(self.walkers) do
        self.crowd:set(walker.id, {partColors = self:outfit()})
    end
end

function Recolor:update(dt)
    Recolor.super.update(self, dt)
    for _, walker in ipairs(self.walkers) do
        walker.x = walker.x + walker.speed * dt
        if walker.x < -900 or walker.x > 900 then
            walker.speed = -walker.speed
        end
        self.crowd:set(walker.id, {x = walker.x, flipHorizontal = walker.speed < 0})
    end
    local stats = graphics2d.stats()
    self:status(string.format('Outfits %d   Crowd %d   Draw calls %d', #self.outfits, Recolor.crowd, stats.drawCalls))
end

function Recolor:draw(area)
    for index, colors in ipairs(self.outfits) do
        local x = -790 + (index - 1) * 225
        graphics2d.draw(self.base, x, -230, {width = 220, height = 220, partMask = self.mask, partColors = colors})
    end
    Test.caption('Base art', -840, -110, {size = 24, color = Test.ink})
    self.crowd:draw({partMask = self.mask})
end

return Recolor
