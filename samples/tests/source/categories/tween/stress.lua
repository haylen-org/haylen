-- Stress test: thousands of painted stars, each moved, turned and tinted by a native tween, with a readout in the corner of the play area of the sprites, the tweens, the frames the display shows and the work of a frame.
local assets = require('haylen.assets')
local debugging = require('haylen.debug')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TweenTest = require('categories.tween.tween-test')

local Stress = haylen.class('Stress', TweenTest)

local kCounts = {{id = '1000', text = '1,000 sprites'}, {id = '5000', text = '5,000 sprites'}, {id = '10000', text = '10,000 sprites'}, {id = '20000', text = '20,000 sprites'}}
local kColors = {'#FFFFC93C', '#FF2F86F6', '#FF2FB3C4', '#FF7BC74D', '#FFF0473E', '#FFC9A0FF'}
local kSize = 18
local kCode = [[
for index = 1, count do
    local sprite = graphics2d.newSprite(star, {x = x, y = y, width = 18, height = 18, color = colors[index % 6 + 1]})
    tween.to(sprite, 1 + math.random() * 2, {x = tx, y = ty, rotation = math.pi}, {loopMode = 'yoyo', repeatCount = -1, delay = math.random()})
end]]

function Stress:enter()
    self.count = 5000
    self.sprites = {}
    self.star = assets.texture('interface/themes/fantasy/icons/star.png', {filter = 'linear', mipmaps = true})
    self:frame({
        hint = 'Each sprite has one tween that the engine runs in C++, so the Lua cost is only drawing. The work of a frame is the time from its update to the end of its rendering, and the display may show fewer frames than that work allows.',
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
    local area, random = self.area, math.random
    self.sprites = {}
    for index = 1, self.count do
        local color = m.color(kColors[index % #kColors + 1])
        local sprite = graphics2d.newSprite(self.star, {x = random() * area.width, y = random() * area.height, width = kSize, height = kSize, color = color})
        self.sprites[index] = sprite
        tween.to(sprite, 1 + random() * 2, {x = random() * area.width, y = random() * area.height, rotation = math.pi}, {owner = self, tag = 'stress', loopMode = 'yoyo', repeatCount = -1, delay = random(), ease = 'sineInOut'})
    end
end

-- The readout of the stress test, in a panel at the top-left corner of the play area.
function Stress:drawReadout()
    local frame = debugging.frame()
    local rows = {
        {'Sprites', string.format('%d', #self.sprites)},
        {'Tweens', string.format('%d', tween.size())},
        {'Frames shown per second', string.format('%.0f', frame.fps)},
        {'Work of a frame', string.format('%.2f ms', frame.milliseconds)},
    }
    graphics2d.drawShape({16, 16, 380, 32 + #rows * 36}, {radius = 16, color = '#E61C2130', borderWidth = 2, borderColor = Test.line, layer = 10})
    for index, row in ipairs(rows) do
        local y = 32 + (index - 1) * 36
        graphics2d.drawText(nil, row[1], 36, y, {size = 22, color = Test.muted, layer = 11})
        graphics2d.drawText(nil, row[2], 376, y, {size = 22, color = Test.ink, anchor = {1, 0}, layer = 11})
    end
end

function Stress:draw(area)
    for _, sprite in ipairs(self.sprites) do
        sprite:draw()
    end
    self:drawReadout()
end

return Stress
