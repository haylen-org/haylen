-- Atlases and sheets: an atlas in the hash layout with trimmed frames and a coin made of named frames, an atlas in the array layout with tagged animations and a nine-slice bubble, and a walker cut from a grid sheet.
local animation2d = require('haylen.animation2d')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local timer = require('haylen.timer')

local SpriteTest = require('categories.sprites.sprite-test')
local Test = require('harness.test')

local Atlases = haylen.class('Atlases', SpriteTest)

Atlases.icons = {'gem_red.png', 'gem_green.png', 'gem_blue.png', 'star.png', 'heart.png'}
Atlases.tags = {'idle', 'hop', 'splat'}
Atlases.runCells = {9, 10, 11, 12, 13, 14, 15, 16}
Atlases.titles = {'Hash atlas, trimmed frames', 'Array atlas, tags and a slice', 'Grid sheet'}

Atlases.code = [[
local gems = assets.load('sprites/atlases/gems.json', 'atlas')  gems:apply(sprite, 'gem_red.png')
local coin = gems:animationFromFrames({'coin_0.png', 'coin_1.png', ...}, {framesPerSecond = 10})
local slime = assets.load('sprites/atlases/slime.json', 'atlas')  animator:add('hop', slime:animation('hop'))  local bubble = slime:slice('bubble')
local run = animation2d.fromGrid(assets.texture('sprites/sheets/walker.png'), {frameWidth = 64, frameHeight = 64, cells = {9, 10, 11, 12, 13, 14, 15, 16}, framesPerSecond = 12})]]

-- An animator that plays one animation on its own sprite.
function Atlases.player(texture, name, animation)
    local animator = animation2d.newAnimator()
    animator:add(name, animation)
    animator:play(name)
    return {sprite = graphics2d.newSprite(texture), animator = animator}
end

function Atlases:enter()
    self.gems = SpriteTest.atlas('atlases/gems.json')
    self.slime = SpriteTest.atlas('atlases/slime.json')
    self.sheet = SpriteTest.texture('sheets/walker.png')
    self.bubble = self.slime:slice('bubble')

    self.icons = {}
    for index, name in ipairs(Atlases.icons) do
        self.icons[index] = graphics2d.newSprite(self.gems.texture, {scaleX = 1.4, scaleY = 1.4})
        self.gems:apply(self.icons[index], name)
    end
    local coinFrames = {}
    for index = 0, 5 do
        coinFrames[#coinFrames + 1] = 'coin_' .. index .. '.png'
    end
    self.players = {coin = Atlases.player(self.gems.texture, 'spin', self.gems:animationFromFrames(coinFrames, {framesPerSecond = 10}))}
    for _, tag in ipairs(Atlases.tags) do
        self.players[tag] = Atlases.player(self.slime.texture, tag, self.slime:animation(tag))
    end
    self.players.walker = Atlases.player(self.sheet, 'run', animation2d.fromGrid(self.sheet, {frameWidth = 64, frameHeight = 64, cells = Atlases.runCells, framesPerSecond = 12}))

    -- The splat plays once, so it starts over a moment after it finishes.
    local splat = self.players.splat.animator
    splat.onFinish = function()
        timer.after(0.8, function() splat:play('splat', true) end, {owner = self})
    end

    for _, entry in pairs(self.players) do
        entry.sprite.scaleX, entry.sprite.scaleY = 2.5, 2.5
    end
    self.players.walker.sprite.scaleX, self.players.walker.sprite.scaleY = 2, 2
    self:frame{code = Atlases.code, hint = 'Outlines on the images mark the frames each file describes.'}
end

function Atlases:update(dt)
    Atlases.super.update(self, dt)
    for _, entry in pairs(self.players) do
        entry.animator:update(dt)
        entry.animator:apply(entry.sprite)
    end
    local frame = self.gems:frame('coin_' .. (self.players.coin.animator.frame - 1) .. '.png')
    self:status(string.format('Coin frame %d: %.0f x %.0f of %.0f x %.0f at offset %.0f, %.0f   Walker cell %d', self.players.coin.animator.frame, frame.source.width, frame.source.height, frame.originalSize.x, frame.originalSize.y, frame.offset.x, frame.offset.y, Atlases.runCells[self.players.walker.animator.frame]))
end

-- Draws a texture at a scale with an outline around every source rectangle of `sources`.
function Atlases.drawSheet(texture, x, y, scale, sources, highlight)
    graphics2d.drawRect({x, y, texture.width * scale, texture.height * scale}, '#FF232A3A')
    graphics2d.draw(texture, x, y, {pivotX = 0, pivotY = 0, scaleX = scale, scaleY = scale, layer = 1})
    for index, source in ipairs(sources) do
        local color = index == highlight and Test.warm or '#804C7DFF'
        graphics2d.drawRectOutline({x + source.x * scale, y + source.y * scale, source.width * scale, source.height * scale}, index == highlight and 3 or 1, color, {layer = 2})
    end
end

function Atlases.place(entry, x, y)
    entry.sprite.x, entry.sprite.y = x, y
    entry.sprite:draw()
end

function Atlases:draw(area)
    local column = area.width / 3
    for index, title in ipairs(Atlases.titles) do
        Test.caption(title, column * (index - 0.5), 24, {size = 26, color = Test.warm, anchor = {0.5, 0}})
    end
    graphics2d.drawLine(column, 0, column, area.height, 1, Test.line)
    graphics2d.drawLine(column * 2, 0, column * 2, area.height, 1, Test.line)

    local gemSources = {}
    for index, name in ipairs(self.gems:frameNames()) do
        gemSources[index] = self.gems:source(name)
    end
    Atlases.drawSheet(self.gems.texture, 30, 70, 1.4, gemSources)
    for index, icon in ipairs(self.icons) do
        icon.x, icon.y = 50 + (index - 1) * 80, area.height * 0.62
        icon:draw()
    end
    Atlases.place(self.players.coin, column / 2, area.height * 0.84)

    local slimeSources = {}
    for index, name in ipairs(self.slime:frameNames()) do
        slimeSources[index] = self.slime:source(name)
    end
    Atlases.drawSheet(self.slime.texture, column + 30, 70, 1.1, slimeSources)
    for index, tag in ipairs(Atlases.tags) do
        local x = column + column * (index - 0.5) / 3
        Atlases.place(self.players[tag], x, area.height * 0.72)
        Test.caption('Tag "' .. tag .. '"', x, area.height * 0.72 + 80, {size = 22, anchor = {0.5, 0.5}})
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
    Atlases.drawSheet(self.sheet, column * 2 + 30, 70, (column - 60) / self.sheet.width, cells, Atlases.runCells[self.players.walker.animator.frame])
    Atlases.place(self.players.walker, column * 2.5, area.height * 0.7)
end

return Atlases
