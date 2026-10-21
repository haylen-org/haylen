-- Ready-made tweens: move, scale, rotate, fade, tint, jump, path, Bezier, blink, shake and punch, each looping on a painted sprite whose properties the engine animates natively, over a faint copy of where it starts and the path, the arc or the size it heads for.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TweenTest = require('categories.tween.tween-test')

local ReadyMade = haylen.class('ReadyMade', TweenTest)

local kColumns = 4
local kBand = 46
local kKinds = {
    {kind = 'move', title = 'Move', icon = 'coin'},
    {kind = 'scale', title = 'Scale', icon = 'gem'},
    {kind = 'rotate', title = 'Rotate', icon = 'key'},
    {kind = 'fade', title = 'Fade', icon = 'potion'},
    {kind = 'tint', title = 'Tint', icon = 'star'},
    {kind = 'jump', title = 'Jump', icon = 'coin'},
    {kind = 'path', title = 'Path', image = 'physics/ship.png', width = 1.3, height = 1},
    {kind = 'bezier', title = 'Bezier', icon = 'gem'},
    {kind = 'blink', title = 'Blink', icon = 'shield'},
    {kind = 'shake', title = 'Shake', icon = 'heart'},
    {kind = 'punch', title = 'Punch', icon = 'star'},
}
local kCode = [[
tween.move(sprite, 1.2, {x + 90, y}, {loopMode = 'yoyo', repeatCount = -1})
tween.rotate(sprite, 0.6, math.pi / 2, {loopMode = 'incremental', repeatCount = -1})
tween.jump(sprite, 1.2, {x + 80, y}, {power = 70, jumps = 3})
tween.path(sprite, 3, points, {closed = true, orient = true})
tween.bezier(sprite, 1.4, {control1, control2, finish})
tween.punch(sprite, 0.6, {0, -50}, {elasticity = 0.6})  -- And scale, fade, tint, blink and shake alike.]]

function ReadyMade:enter()
    self.sprites = {}
    for index, entry in ipairs(kKinds) do
        local texture = entry.image and assets.texture(entry.image, {filter = 'linear'}) or TweenTest.icon(entry.icon)
        self.sprites[index] = graphics2d.newSprite(texture, {layer = 2})
    end
    self:frame({
        hint = 'Every sprite property animates in C++ without running Lua each frame. The faint copy marks where each tween starts, and R or the X button replays them.',
        code = kCode,
        controls = {ui.button{id = 'replay', text = 'Replay', variant = 'primary', onClick = function() self:play() end}},
        focus = 'replay',
    })
end

-- The cells fill rows of `kColumns`, and the last row centers the cells it holds. Returns the center of the room under the title of the cell, its rectangle and the scale that fits the motions into it.
function ReadyMade:cell(index)
    local rows = math.ceil(#kKinds / kColumns)
    local width, height = self.area.width / kColumns, self.area.height / rows
    local column, row = (index - 1) % kColumns, (index - 1) // kColumns
    local inRow = math.min(kColumns, #kKinds - row * kColumns)
    local left = (self.area.width - inRow * width) / 2 + column * width
    local top = row * height
    local scale = math.min(width / 360, (height - kBand) / 170)
    return left + width / 2, top + kBand + (height - kBand) / 2, {left, top, width, height}, scale
end

function ReadyMade:resize(area)
    self:play()
end

function ReadyMade:play()
    self.shapes = {}
    for index, entry in ipairs(kKinds) do
        local sprite = self.sprites[index]
        local x, y, _, s = self:cell(index)
        tween.killTarget(sprite)
        local size = 64 * s
        sprite.width, sprite.height = size * (entry.width or 1), size * (entry.height or 1)
        sprite.x, sprite.y, sprite.scaleX, sprite.scaleY, sprite.rotation, sprite.color = x, y, 1, 1, 0, '#FFFFFFFF'
        local loop = {owner = self, loopMode = 'yoyo', repeatCount = -1, repeatDelay = 0.2, ease = 'sineInOut'}
        local again = {owner = self, repeatCount = -1, repeatDelay = 0.6}
        local shape = {start = {x, y}}
        local kind = entry.kind

        if kind == 'move' then
            shape.start = {x - 90 * s, y}
            shape.line = {x - 90 * s, y, x + 90 * s, y}
            sprite.x = x - 90 * s
            tween.move(sprite, 1.2, {x + 90 * s, y}, loop)
        elseif kind == 'scale' then
            shape.ring = 1.6 * size / 2
            tween.scale(sprite, 0.8, 1.6, loop)
        elseif kind == 'rotate' then
            shape.turn = size * 0.75
            tween.rotate(sprite, 0.6, math.pi / 2, {owner = self, loopMode = 'incremental', repeatCount = -1, repeatDelay = 0.2, ease = 'backOut'})
        elseif kind == 'fade' then
            tween.fade(sprite, 0.8, 0.25, loop)
        elseif kind == 'tint' then
            shape.swatches = {'#FFFFFFFF', '#FFFF4060'}
            tween.tint(sprite, 1, '#FFFF4060', loop)
        elseif kind == 'jump' then
            shape.start = {x - 80 * s, y + 30 * s}
            sprite.x, sprite.y = x - 80 * s, y + 30 * s
            shape.hops = {x - 80 * s, x + 80 * s, y + 30 * s, 70 * s, 3}
            tween.jump(sprite, 1.2, {x + 80 * s, y + 30 * s}, {owner = self, power = 70 * s, jumps = 3, loopMode = 'yoyo', repeatCount = -1, repeatDelay = 0.2})
        elseif kind == 'path' then
            local points = {{x + 90 * s, y - 20 * s}, {x, y + 50 * s}, {x - 90 * s, y - 20 * s}, {x, y - 50 * s}}
            shape.start = {x, y - 50 * s}
            shape.spline = m.spline({{x, y - 50 * s}, points[1], points[2], points[3]}, {closed = true})
            sprite.x, sprite.y = x, y - 50 * s
            tween.path(sprite, 3, points, {owner = self, closed = true, orient = true, repeatCount = -1})
        elseif kind == 'bezier' then
            local first, second, finish = {x - 40 * s, y - 80 * s}, {x + 40 * s, y + 80 * s}, {x + 90 * s, y}
            shape.start = {x - 90 * s, y}
            shape.spline = m.spline({{x - 90 * s, y}, first, second, finish}, {kind = 'bezier'})
            shape.handles = {{x - 90 * s, y, first[1], first[2]}, {finish[1], finish[2], second[1], second[2]}}
            sprite.x = x - 90 * s
            tween.bezier(sprite, 1.4, {first, second, finish}, loop)
        elseif kind == 'blink' then
            tween.blink(sprite, 1.2, 5, again)
        elseif kind == 'shake' then
            shape.cross = 14 * s
            tween.shake(sprite, 0.6, 14 * s, {owner = self, vibrato = 20, repeatCount = -1, repeatDelay = 0.6})
        elseif kind == 'punch' then
            shape.target = {x, y - 50 * s}
            tween.punch(sprite, 0.6, {0, -50 * s}, {owner = self, elasticity = 0.6, repeatCount = -1, repeatDelay = 0.6})
        end
        shape.size = {sprite.width, sprite.height}
        self.shapes[index] = shape
    end
end

function ReadyMade:update(dt)
    ReadyMade.super.update(self, dt)
    if input.pressed('replay') then
        self:play()
    end
    self:status(string.format('The engine plays %d tweens natively.', tween.size()))
end

-- The guide of a cell: the faint start, and the line, path, arc, ring or mark that tells where the tween goes.
function ReadyMade:drawGuide(index)
    local shape, sprite = self.shapes[index], self.sprites[index]
    local guide = m.color(Test.accent):withAlpha(0.7)
    if shape.line then
        graphics2d.drawStroke(shape.line, {width = 3, color = guide, cap = 'round', dashes = {10, 8}})
    end
    if shape.spline then
        graphics2d.drawStroke(shape.spline, {width = 3, color = guide, cap = 'round', dashes = {10, 8}})
    end
    for _, handle in ipairs(shape.handles or {}) do
        graphics2d.drawLine(handle[1], handle[2], handle[3], handle[4], 2, Test.muted)
        graphics2d.drawShape({handle[3] - 7, handle[4] - 7, 14, 14}, {radius = 3, color = Test.warm, borderWidth = 2, borderColor = '#FF1E1B33'})
    end
    if shape.hops then
        local from, to, ground, power, count = table.unpack(shape.hops)
        local points = {}
        for step = 0, count * 16 do
            local t = step / (count * 16)
            local hop = (t * count) % 1
            points[#points + 1] = from + (to - from) * t
            points[#points + 1] = ground - power * 4 * hop * (1 - hop)
        end
        graphics2d.drawStroke(points, {width = 3, color = guide, join = 'round', cap = 'round', dashes = {10, 8}})
    end
    if shape.ring then
        graphics2d.drawShape({shape.start[1] - shape.ring, shape.start[2] - shape.ring, shape.ring * 2, shape.ring * 2}, {kind = 'ellipse', color = '#00000000', borderWidth = 3, borderColor = guide})
    end
    if shape.turn then
        graphics2d.drawArc(shape.start[1], shape.start[2], shape.turn, 4, -math.pi * 0.85, -math.pi * 0.15, guide)
    end
    if shape.swatches then
        for side, color in ipairs(shape.swatches) do
            local x = shape.start[1] + (side == 1 and -1 or 1) * shape.size[1] * 1.3
            graphics2d.drawShape({x - 14, shape.start[2] - 14, 28, 28}, {radius = 8, color = color, borderWidth = 3, borderColor = '#FF1E1B33'})
        end
    end
    if shape.cross then
        local x, y, r = shape.start[1], shape.start[2], shape.size[1] * 0.75
        graphics2d.drawShape({x - r, y - r, r * 2, r * 2}, {kind = 'ellipse', color = '#00000000', borderWidth = 3, borderColor = guide})
    end
    if shape.target then
        graphics2d.drawStroke({shape.start[1], shape.start[2], shape.target[1], shape.target[2]}, {width = 3, color = guide, cap = 'round', dashes = {8, 7}})
        graphics2d.drawShape({shape.target[1] - 8, shape.target[2] - 8, 16, 16}, {kind = 'ellipse', color = guide})
    end
    graphics2d.draw(sprite.texture, shape.start[1], shape.start[2], {width = shape.size[1], height = shape.size[2], color = m.color(1, 1, 1, 0.22), layer = 1})
end

function ReadyMade:draw(area)
    for index, entry in ipairs(kKinds) do
        local _, _, rect = self:cell(index)
        graphics2d.drawShape({rect[1] + 6, rect[2] + 6, rect[3] - 12, rect[4] - 12}, {radius = 16, color = '#FF1C2130', borderWidth = 2, borderColor = Test.line})
        graphics2d.drawText(nil, entry.title, rect[1] + 26, rect[2] + 16, {size = 24, color = Test.ink})
        self:drawGuide(index)
        self.sprites[index]:draw()
    end
end

return ReadyMade
