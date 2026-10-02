-- Ready-made tweens: move, scale, rotate, fade, tint, jump, path, Bezier, blink, shake and punch, each looping on a sprite whose properties the engine animates natively.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local sample = require('sample')

local ReadyMade = haylen.class('ReadyMade', sample.Test)

local kColumns = 4
local kKinds = {'move', 'scale', 'rotate', 'fade', 'tint', 'jump', 'path', 'bezier', 'blink', 'shake', 'punch'}
local kCode = [[
tween.move(sprite, 1.2, {x + 90, y})  tween.scale(sprite, 0.8, 1.6)  tween.rotate(sprite, 0.6, math.pi / 2, {loopMode = 'incremental'})
tween.fade(sprite, 0.8, 0.1)  tween.tint(sprite, 1, '#FFFF4060')  tween.jump(sprite, 1.2, {x + 80, y}, {power = 70, jumps = 3})
tween.path(sprite, 3, points, {closed = true, orient = true})  tween.bezier(sprite, 1.4, {control1, control2, finish})
tween.blink(sprite, 1.2, 5)  tween.shake(sprite, 0.6, 14, {vibrato = 20})  tween.punch(sprite, 0.6, {0, -50}, {elasticity = 0.6})]]

-- A white arrow that points right, built from code, so rotations and paths that orient the sprite show which way it faces.
local function arrowTexture()
    local size = 64
    local pixels = {}
    for y = 0, size - 1 do
        for x = 0, size - 1 do
            local shaft = x >= 6 and x < 38 and y >= 24 and y < 40
            local head = x >= 36 and x < 60 and math.abs(y + 0.5 - 32) <= (60 - x) * 0.9
            pixels[#pixels + 1] = (shaft or head) and '\255\255\255\255' or '\0\0\0\0'
        end
    end
    return graphics.newTexture(size, size, {pixels = table.concat(pixels), filter = 'linear'})
end

function ReadyMade:enter()
    local texture = arrowTexture()
    self.sprites = {}
    for index = 1, #kKinds do
        self.sprites[index] = graphics2d.newSprite(texture, {width = 64, height = 64})
    end
    self:frame({
        hint = 'Every sprite property animates in C++ without running Lua each frame.',
        code = kCode,
        controls = {ui.button{id = 'replay', text = 'Replay', variant = 'primary', onClick = function() self:play() end}},
        focus = 'replay',
    })
end

function ReadyMade:cell(index)
    local rows = math.ceil(#kKinds / kColumns)
    local width, height = self.area.width / kColumns, self.area.height / rows
    local column, row = (index - 1) % kColumns, (index - 1) // kColumns
    return column * width + width / 2, row * height + height / 2 + 14, width, height
end

function ReadyMade:resize(area)
    self:play()
end

function ReadyMade:play()
    for index, kind in ipairs(kKinds) do
        local sprite = self.sprites[index]
        local x, y = self:cell(index)
        tween.killTarget(sprite)
        sprite.x, sprite.y, sprite.scaleX, sprite.scaleY, sprite.rotation, sprite.color = x, y, 1, 1, 0, sample.accent
        local loop = {owner = self, loopMode = 'yoyo', repeatCount = -1, repeatDelay = 0.2, ease = 'sineInOut'}
        local again = {owner = self, repeatCount = -1, repeatDelay = 0.6}

        if kind == 'move' then
            sprite.x = x - 90
            tween.move(sprite, 1.2, {x + 90, y}, loop)
        elseif kind == 'scale' then
            tween.scale(sprite, 0.8, 1.6, loop)
        elseif kind == 'rotate' then
            tween.rotate(sprite, 0.6, math.pi / 2, {owner = self, loopMode = 'incremental', repeatCount = -1, repeatDelay = 0.2, ease = 'backOut'})
        elseif kind == 'fade' then
            tween.fade(sprite, 0.8, 0.1, loop)
        elseif kind == 'tint' then
            tween.tint(sprite, 1, '#FFFF4060', loop)
        elseif kind == 'jump' then
            sprite.x = x - 80
            tween.jump(sprite, 1.2, {x + 80, y}, {owner = self, power = 70, jumps = 3, loopMode = 'yoyo', repeatCount = -1, repeatDelay = 0.2})
        elseif kind == 'path' then
            local points = {{x + 90, y - 40}, {x, y + 40}, {x - 90, y - 40}, {x, y - 60}}
            sprite.x, sprite.y = x, y - 60
            tween.path(sprite, 3, points, {owner = self, closed = true, orient = true, repeatCount = -1})
        elseif kind == 'bezier' then
            sprite.x = x - 90
            tween.bezier(sprite, 1.4, {{x - 40, y - 110}, {x + 40, y + 110}, {x + 90, y}}, loop)
        elseif kind == 'blink' then
            tween.blink(sprite, 1.2, 5, again)
        elseif kind == 'shake' then
            tween.shake(sprite, 0.6, 14, {owner = self, vibrato = 20, repeatCount = -1, repeatDelay = 0.6})
        elseif kind == 'punch' then
            tween.punch(sprite, 0.6, {0, -50}, {owner = self, elasticity = 0.6, repeatCount = -1, repeatDelay = 0.6})
        end
    end
end

function ReadyMade:update(dt)
    ReadyMade.super.update(self, dt)
    if input.pressed('replay') then
        self:play()
    end
    self:status(string.format('%d tweens play natively', tween.size()))
end

function ReadyMade:draw(area)
    for index, kind in ipairs(kKinds) do
        local x, y, width, height = self:cell(index)
        graphics2d.drawRectOutline({x - width / 2 + 6, y - height / 2 - 8, width - 12, height - 12}, 1, sample.line)
        graphics2d.drawText(nil, kind:sub(1, 1):upper() .. kind:sub(2), x, y - height / 2 + 14, {size = 24, color = sample.ink, anchor = {0.5, 0.5}})
        self.sprites[index]:draw()
    end
end

return ReadyMade
