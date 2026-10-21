-- Atlases and sheets: an atlas in the hash layout with trimmed frames and a coin made of named frames, an atlas in the array layout with tagged animations and a nine-slice bubble, and a walker cut from a grid sheet, each sheet above what it makes.
local animation2d = require('haylen.animation2d')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local timer = require('haylen.timer')

local SpriteTest = require('categories.sprites.sprite-test')
local Test = require('harness.test')

local Atlases = haylen.class('Atlases', SpriteTest)

Atlases.icons = {'gem-red.png', 'gem-green.png', 'gem-blue.png', 'star.png', 'heart.png'}
Atlases.tags = {{tag = 'idle', label = 'Idle'}, {tag = 'hop', label = 'Hop'}, {tag = 'splat', label = 'Splat'}}
Atlases.runCells = {9, 10, 11, 12, 13, 14, 15, 16}
Atlases.titles = {'Hash atlas, trimmed frames', 'Array atlas, tags and a slice', 'Grid sheet of cells'}
-- The sheets draw at most this tall above their samples.
Atlases.sheetHeight = 210
-- The samples spread over at most this much room under their sheet, so a tall stage keeps them close to it.
Atlases.room = 420
Atlases.ink = '#FFE8EAF2'
Atlases.frameLine = '#FF6C7FC8'

Atlases.code = [[
local gems = assets.load('sprites/atlases/gems.json', 'atlas')
gems:apply(sprite, 'gem-red.png')  -- A trimmed frame keeps the center of its whole frame.
local coin = gems:animationFromFrames(coinFrames, {framesPerSecond = 10})
animator:add('hop', assets.load('sprites/atlases/slime.json', 'atlas'):animation('hop'))
graphics2d.drawNineSlice(slime:slice('bubble'), {x, y, 270, 120}, '#FFFFFFFF', {borderScale = 1.4})
local run = animation2d.fromGrid(walker, {frameWidth = 256, frameHeight = 256, cells = runCells})]]

-- An animator that plays one animation on its own sprite.
function Atlases.player(texture, name, animation, scale)
    local animator = animation2d.newAnimator()
    animator:add(name, animation)
    animator:play(name)
    return {sprite = graphics2d.newSprite(texture, {scaleX = scale, scaleY = scale, layer = 2}), animator = animator}
end

function Atlases:enter()
    self.gems = SpriteTest.atlas('atlases/gems.json')
    self.slime = SpriteTest.atlas('atlases/slime.json')
    self.sheet = SpriteTest.texture('sheets/walker.png')
    self.bubble = self.slime:slice('bubble')

    self.icons = {}
    for index, name in ipairs(Atlases.icons) do
        self.icons[index] = graphics2d.newSprite(self.gems.texture, {scaleX = 0.8, scaleY = 0.8, layer = 2})
        self.gems:apply(self.icons[index], name)
    end
    local coinFrames = {}
    for index = 0, 5 do
        coinFrames[#coinFrames + 1] = 'coin-' .. index .. '.png'
    end
    self.players = {coin = Atlases.player(self.gems.texture, 'spin', self.gems:animationFromFrames(coinFrames, {framesPerSecond = 10}), 1)}
    for _, entry in ipairs(Atlases.tags) do
        self.players[entry.tag] = Atlases.player(self.slime.texture, entry.tag, self.slime:animation(entry.tag), 1.1)
    end
    self.players.walker = Atlases.player(self.sheet, 'run', animation2d.fromGrid(self.sheet, {frameWidth = 256, frameHeight = 256, cells = Atlases.runCells, framesPerSecond = 12}), 1)

    -- The splat plays once, so it starts over a moment after it finishes.
    local splat = self.players.splat.animator
    splat.onComplete = function()
        timer.after(0.8, function() splat:play('splat', true) end, {owner = self})
    end

    self.gemSources, self.slimeSources, self.cells = {}, {}, {}
    for index, name in ipairs(self.gems:frameNames()) do
        self.gemSources[index] = self.gems:source(name)
    end
    for index, name in ipairs(self.slime:frameNames()) do
        self.slimeSources[index] = self.slime:source(name)
    end
    for row = 0, 2 do
        for column = 0, 7 do
            self.cells[#self.cells + 1] = {x = column * 256, y = row * 256, width = 256, height = 256}
        end
    end
    self:frame{code = Atlases.code, hint = 'The outlines on each sheet mark the frames its file describes, and the yellow one the frame the walker shows.'}
end

function Atlases:update(dt)
    Atlases.super.update(self, dt)
    for _, entry in pairs(self.players) do
        entry.animator:update(dt)
        entry.animator:apply(entry.sprite)
    end
    local coin = self.players.coin.animator.frame
    local frame = self.gems:frame('coin-' .. (coin - 1) .. '.png')
    self:status(string.format('The coin shows frame %d, trimmed to %.0f by %.0f of %.0f by %.0f pixels. The walker shows cell %d of its sheet.', coin, frame.source.width, frame.source.height, frame.originalSize.x, frame.originalSize.y, Atlases.runCells[self.players.walker.animator.frame]))
end

-- Draws a texture centered on `x` at the top `y`, at most `width` wide and `Atlases.sheetHeight` tall, on a dark card with an outline around every source rectangle of `sources`, and returns the bottom of the card.
function Atlases.drawSheet(texture, x, y, width, sources, highlight)
    local scale = math.min(width / texture.width, Atlases.sheetHeight / texture.height)
    local drawnWidth, drawnHeight = texture.width * scale, texture.height * scale
    local left = x - drawnWidth / 2
    graphics2d.drawShape({left - 12, y - 12, drawnWidth + 24, drawnHeight + 24}, {radius = 14, color = '#FF232A3A', borderWidth = 2, borderColor = Test.line})
    graphics2d.draw(texture, left, y, {pivotX = 0, pivotY = 0, scaleX = scale, scaleY = scale, layer = 1})
    for index, source in ipairs(sources) do
        local lit = index == highlight
        graphics2d.drawRectOutline({left + source.x * scale, y + source.y * scale, source.width * scale, source.height * scale}, lit and 3 or 1, lit and Test.warm or Atlases.frameLine, {layer = lit and 3 or 2})
    end
    return y + drawnHeight + 12
end

function Atlases.label(text, x, y)
    graphics2d.drawText(nil, text, x, y, {size = 24, color = Atlases.ink, anchor = {0.5, 0}, layer = 2})
end

function Atlases.place(entry, x, y)
    entry.sprite.x, entry.sprite.y = x, y
    entry.sprite:draw()
end

function Atlases:draw(area)
    local column = area.width / 3
    local inner = column - 72
    for index, title in ipairs(Atlases.titles) do
        graphics2d.drawText(nil, title, column * (index - 0.5), 22, {size = 28, color = Test.warm, anchor = {0.5, 0}})
    end
    graphics2d.drawLine(column, 16, column, area.height - 16, 2, Test.line)
    graphics2d.drawLine(column * 2, 16, column * 2, area.height - 16, 2, Test.line)

    -- The free room under each sheet holds its samples: the upper row near the middle of it and the lower row near its end.
    local top = 84
    local bottom = Atlases.drawSheet(self.gems.texture, column * 0.5, top, inner, self.gemSources)
    local room = math.min(area.height - bottom, Atlases.room)
    local upper, lower = bottom + room * 0.3, bottom + room * 0.72
    local step = math.min(88, inner / #self.icons)
    for index, icon in ipairs(self.icons) do
        icon.x, icon.y = column * 0.5 + (index - (#self.icons + 1) / 2) * step, upper
        icon:draw()
    end
    Atlases.label('Five frames by name', column * 0.5, upper + 50)
    Atlases.place(self.players.coin, column * 0.5, lower)
    Atlases.label('A coin from six frames', column * 0.5, lower + 52)

    bottom = Atlases.drawSheet(self.slime.texture, column * 1.5, top, inner, self.slimeSources)
    room = math.min(area.height - bottom, Atlases.room)
    lower = bottom + room * 0.72
    -- The idle slime says the bubble, whose tail points down at it from the corner of the slice, and the body of the bubble ends above the tail.
    local speaker, borderScale = column * 1.5 - inner / 3, 1.4
    local bubble = {speaker - 16, lower - 54 - 120, 270, 120}
    graphics2d.drawNineSlice(self.bubble, bubble, '#FFFFFFFF', {borderScale = borderScale, layer = 2})
    graphics2d.drawText(nil, 'A slice!', bubble[1] + bubble[3] / 2, bubble[2] + bubble[4] / 2 - 8 * borderScale, {size = 32, color = '#FF1E1B33', anchor = {0.5, 0.5}, layer = 3})
    for index, entry in ipairs(Atlases.tags) do
        local x = column * 1.5 + (index - 2) * inner / 3
        Atlases.place(self.players[entry.tag], x, lower)
        Atlases.label(entry.label, x, lower + 52)
    end

    bottom = Atlases.drawSheet(self.sheet, column * 2.5, top, inner, self.cells, Atlases.runCells[self.players.walker.animator.frame])
    local walker = self.players.walker.sprite
    local size = math.min(Atlases.room - 80, area.height - bottom - 80, inner)
    walker.scaleX, walker.scaleY = size / 256, size / 256
    Atlases.place(self.players.walker, column * 2.5, bottom + 20 + size / 2)
    Atlases.label('Run, cells 9 to 16', column * 2.5, bottom + 28 + size)
end

return Atlases
