-- Stress test: thousands of sprites, each moved, turned and tinted by native tweens, with the tween count and the frame time.
local debugging = require('haylen.debug')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local sample = require('sample')

local Stress = haylen.class('Stress', sample.Test)

local kCounts = {{id = '1000', text = '1,000 sprites'}, {id = '5000', text = '5,000 sprites'}, {id = '10000', text = '10,000 sprites'}, {id = '20000', text = '20,000 sprites'}}
local kCode = [[
for index = 1, count do
    local sprite = graphics2d.newSprite(texture, {x = x, y = y, width = 8, height = 8, color = m.fromHsv(math.random(), 0.7, 1)})
    tween.to(sprite, 1 + math.random() * 2, {x = tx, y = ty, rotation = math.pi}, {loopMode = 'yoyo', repeatCount = -1, delay = math.random()})
end]]

function Stress:enter()
    self.count = 5000
    self.sprites = {}
    self:frame({
        hint = 'Each sprite has one tween that the engine runs in C++, so the Lua cost is only drawing.',
        code = kCode,
        controls = {ui.formField{label = 'Sprites', ui.stepper{id = 'count', items = kCounts, selected = '5000', onChange = function(event)
            self.count = tonumber(event.value)
            self:build()
        end}}},
        focus = 'count',
    })
end

function Stress:resize(area)
    self:build()
end

function Stress:build()
    tween.killTag('stress')
    local area, texture, random = self.area, graphics.whiteTexture(), math.random
    self.sprites = {}
    for index = 1, self.count do
        local sprite = graphics2d.newSprite(texture, {x = random() * area.width, y = random() * area.height, width = 8, height = 8, color = m.fromHsv(random(), 0.7, 1)})
        self.sprites[index] = sprite
        tween.to(sprite, 1 + random() * 2, {x = random() * area.width, y = random() * area.height, rotation = math.pi}, {owner = self, tag = 'stress', loopMode = 'yoyo', repeatCount = -1, delay = random(), ease = 'sineInOut'})
    end
end

function Stress:update(dt)
    Stress.super.update(self, dt)
    local frame = debugging.frame()
    self:status(string.format('%d sprites   %d tweens   %.0f FPS   %.2f ms per frame', #self.sprites, tween.size(), frame.fps, frame.milliseconds))
end

function Stress:draw(area)
    for _, sprite in ipairs(self.sprites) do
        sprite:draw()
    end
end

return Stress
