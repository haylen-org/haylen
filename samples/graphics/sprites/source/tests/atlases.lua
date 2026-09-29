-- Atlases and sheets: a TexturePacker atlas with trimmed frames and a coin made of named frames, an Aseprite atlas with tagged animations and a nine-slice bubble, and a walker cut from a grid sheet.
local animation2d = require('haylen.animation2d')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local timer = require('haylen.timer')

local sample = require('sample')

local Atlases = haylen.class('Atlases', sample.Test)

local kIcons = {'gem_red.png', 'gem_green.png', 'gem_blue.png', 'star.png', 'heart.png'}
local kTags = {'idle', 'hop', 'splat'}
local kRunFrames = {9, 10, 11, 12, 13, 14, 15, 16}
local kCode = [[
local gems = assets.load('atlases/gems.json', 'atlas')  gems:apply(sprite, 'gem_red.png')
local coin = gems:animationFromFrames({'coin_0.png', 'coin_1.png', ...}, {fps = 10})
local slime = assets.load('atlases/slime.json', 'atlas')  animator:add('hop', slime:animation('hop'))  local bubble = slime:slice('bubble')
local run = animation2d.grid(assets.texture('sheets/walker.png'), {frameWidth = 64, frameHeight = 64, frames = {9, 10, 11, 12, 13, 14, 15, 16}, fps = 12})]]

-- An animator that plays one animation on its own sprite.
local function player(texture, name, animation)
    local animator = animation2d.newAnimator()
    animator:add(name, animation)
    animator:play(name)
    return {sprite = graphics2d.newSprite(texture), animator = animator}
end

function Atlases:enter()
    self.gems = sample.atlas('atlases/gems.json')
    self.slime = sample.atlas('atlases/slime.json')
    self.sheet = sample.texture('sheets/walker.png')
    self.bubble = self.slime:slice('bubble')

    self.icons = {}
    for index, name in ipairs(kIcons) do
        self.icons[index] = graphics2d.newSprite(self.gems.texture, {scaleX = 1.4, scaleY = 1.4})
        self.gems:apply(self.icons[index], name)
    end
    local coinFrames = {}
    for index = 0, 5 do
        coinFrames[#coinFrames + 1] = 'coin_' .. index .. '.png'
    end
    self.players = {coin = player(self.gems.texture, 'spin', self.gems:animationFromFrames(coinFrames, {fps = 10}))}
    for _, tag in ipairs(kTags) do
        self.players[tag] = player(self.slime.texture, tag, self.slime:animation(tag))
    end
    self.players.walker = player(self.sheet, 'run', animation2d.grid(self.sheet, {frameWidth = 64, frameHeight = 64, frames = kRunFrames, fps = 12}))

    -- The splat plays once, so it starts over a moment after it finishes.
    local splat = self.players.splat.animator
    splat.onFinish = function()
        timer.after(0.8, function() splat:play('splat', true) end, {owner = self})
    end

    for _, entry in pairs(self.players) do
        entry.sprite.scaleX, entry.sprite.scaleY = 2.5, 2.5
    end
    self.players.walker.sprite.scaleX, self.players.walker.sprite.scaleY = 2, 2
    self:frame({hint = 'Outlines on the images mark the frames each file describes.', code = kCode})
end

function Atlases:update(dt)
    Atlases.super.update(self, dt)
    for _, entry in pairs(self.players) do
        entry.animator:update(dt)
        entry.animator:apply(entry.sprite)
    end
    local frame = self.gems:frame('coin_' .. (self.players.coin.animator.frame - 1) .. '.png')
    self:status(string.format('coin frame %d: %.0f x %.0f of %.0f x %.0f at offset %.0f, %.0f   walker cell %d', self.players.coin.animator.frame, frame.source.width, frame.source.height, frame.originalSize.x, frame.originalSize.y, frame.offset.x, frame.offset.y, kRunFrames[self.players.walker.animator.frame]))
end

-- Draws a texture at a scale with an outline around every source rectangle of `sources`.
local function drawSheet(texture, x, y, scale, sources, highlight)
    graphics2d.drawRect({x, y, texture.width * scale, texture.height * scale}, '#FF232A3A')
    graphics2d.draw(texture, x, y, {pivotX = 0, pivotY = 0, scaleX = scale, scaleY = scale, layer = 1})
    for index, source in ipairs(sources) do
        local color = index == highlight and sample.warm or '#804C7DFF'
        graphics2d.drawRectOutline({x + source.x * scale, y + source.y * scale, source.width * scale, source.height * scale}, index == highlight and 3 or 1, color, {layer = 2})
    end
end

local function place(entry, x, y)
    entry.sprite.x, entry.sprite.y = x, y
    entry.sprite:draw()
end

function Atlases:draw(area)
    local column = area.width / 3
    local titles = {'TexturePacker, hash, trimmed', 'Aseprite, array, tags, slice', 'Grid sheet'}
    for index, title in ipairs(titles) do
        graphics2d.drawText(nil, title, column * (index - 0.5), 24, {size = 26, color = sample.warm, anchor = {0.5, 0}})
    end
    graphics2d.drawLine(column, 0, column, area.height, 1, sample.line)
    graphics2d.drawLine(column * 2, 0, column * 2, area.height, 1, sample.line)

    local gemSources = {}
    for index, name in ipairs(self.gems:frameNames()) do
        gemSources[index] = self.gems:source(name)
    end
    drawSheet(self.gems.texture, 30, 70, 1.4, gemSources)
    for index, icon in ipairs(self.icons) do
        icon.x, icon.y = 50 + (index - 1) * 80, area.height * 0.62
        icon:draw()
    end
    place(self.players.coin, column / 2, area.height * 0.84)

    local slimeSources = {}
    for index, name in ipairs(self.slime:frameNames()) do
        slimeSources[index] = self.slime:source(name)
    end
    drawSheet(self.slime.texture, column + 30, 70, 1.1, slimeSources)
    for index, tag in ipairs(kTags) do
        local x = column + column * (index - 0.5) / 3
        place(self.players[tag], x, area.height * 0.72)
        graphics2d.drawText(nil, tag, x, area.height * 0.72 + 80, {size = 22, color = sample.muted, anchor = {0.5, 0.5}})
    end
    local bubble = {column + column / 2 - 110, area.height * 0.47, 220, 90}
    graphics2d.drawNineSlice(self.bubble, bubble, '#FFFFFFFF', nil, 2)
    graphics2d.drawText(nil, 'A slice!', bubble[1] + 110, bubble[2] + 38, {size = 28, color = '#FF1B1E2B', anchor = {0.5, 0.5}, layer = 1})

    local cells = {}
    for row = 0, 2 do
        for columnIndex = 0, 7 do
            cells[#cells + 1] = {x = columnIndex * 64, y = row * 64, width = 64, height = 64}
        end
    end
    drawSheet(self.sheet, column * 2 + 30, 70, (column - 60) / self.sheet.width, cells, kRunFrames[self.players.walker.animator.frame])
    place(self.players.walker, column * 2.5, area.height * 0.7)
end

return Atlases
