-- Timelines: a small cutscene built with `append`, `join`, a pause, callbacks, a label and inserts at a label and at a time. The door of a cottage opens, the hero fades in and runs to a training dummy, waves, and hops at it while the sky turns to dusk, over a chart of the tweens and calls in time with a cursor at the time of the timeline.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TweenTest = require('categories.tween.tween-test')

local Timelines = haylen.class('Timelines', TweenTest)

local kView = {1400, 760}
local kGround = 470
local kHouse = {source = {262, 4, 272, 280}, door = {129, 162, 61, 76}, x = 60}
local kFrame = 256
local kHeroSize = 170
local kHeroLeft = 220
local kLogLines = 9
-- The tweens and calls of the cutscene in time, the rows of the chart under the scene.
local kRows = {
    {title = 'Door', spans = {{0, 0.6, 'Opens'}}},
    {title = 'Hero', spans = {{0.6, 1.8, 'Runs'}, {0.6, 1.0, 'Fades in'}, {2.1, 3.1, 'Hops'}}},
    {title = 'Sky', spans = {{2.1, 3.1, 'Dusk'}}},
    {title = 'Calls', calls = {{1.0, 'The door creaks'}, {2.1, 'The hero waves'}}},
}
local kFight = 2.1
local kCode = [[
local cutscene = tween.timeline({onStep = logStep, onComplete = logDone})
cutscene:append(tween.to(door, 0.6, {open = 1}))
cutscene:append(tween.to(hero, 1.2, {x = 700})):join(tween.to(hero, 0.4, {alpha = 1}))
cutscene:append(0.3):append(function() log('The hero waves') end):addLabel('fight')
cutscene:append(tween.to(hero, 0.25, {hop = 1}, {loopMode = 'yoyo', repeatCount = 3}))
cutscene:insert('fight', tween.to(sky, 1, {light = 0.3})):insert(1, function() log('The door creaks') end)]]

-- A sky texture that blends its stops from the top down.
local function sky(stops)
    local image = graphics.newImage(4, 256)
    image:fillGradient({angle = math.pi / 2, stops = stops})
    return graphics.newTexture(image, {filter = 'linear'})
end

function Timelines:enter()
    self.lines = {}
    self.props = assets.texture('camera/props.png', {filter = 'linear'})
    self.hills = assets.texture('camera/hills.png', {filter = 'linear'})
    self.ground = assets.texture('camera/ground.png', {filter = 'linear'})
    self.walker = assets.texture('sprites/sheets/walker.png', {filter = 'linear'})
    self.dummy = assets.texture('animation/dummy.png', {filter = 'linear'})
    self.day = sky({{0, '#FF5FB4EA'}, {1, '#FFCDEFFC'}})
    self.dusk = sky({{0, '#FF2A2E5E'}, {0.7, '#FF8A5A8E'}, {1, '#FFF2A066'}})
    self:frame({
        hint = 'Restart, pause, reverse or jump to the "fight" label. Seeking never runs callbacks, and R or the X button restarts.',
        code = kCode,
        controls = {
            ui.button{id = 'restart', text = 'Restart', variant = 'primary', onClick = function() self:play() end},
            ui.row{wrap = true, gap = 12,
                ui.button{id = 'pause', text = 'Pause', grow = 1, onClick = function() self.cutscene:pause() end},
                ui.button{id = 'resume', text = 'Resume', grow = 1, onClick = function() self.cutscene:resume() end},
            },
            ui.row{wrap = true, gap = 12,
                ui.button{id = 'reverse', text = 'Reverse', grow = 1, onClick = function() self.cutscene:reverse() end},
                ui.button{id = 'fight', text = 'Seek "fight"', grow = 1, onClick = function() self.cutscene:seek('fight') end},
            },
            ui.sectionTitle{text = 'Steps and calls'},
            ui.label{id = 'log', text = '', font = 'caption'},
        },
        view = kView,
        focus = 'restart',
    })
    self.camera:snapTo(kView[1] / 2, kView[2] / 2)
    self:play()
end

function Timelines:note(text)
    table.insert(self.lines, string.format('At %.2f s, %s', self.cutscene.time, text))
    if #self.lines > kLogLines then
        table.remove(self.lines, 1)
    end
    self:set('log', {text = table.concat(self.lines, '\n')})
end

function Timelines:play()
    if self.cutscene then
        self.cutscene:kill()
    end
    self.lines = {}
    self.door, self.hero, self.sky = {open = 0}, {x = 60, alpha = 0, hop = 0}, {light = 1}
    self.waved, self.creaked = nil, nil
    self.cutscene = tween.timeline({owner = self, autoKill = false, onStep = function(step) self:note('step ' .. step .. ' ended.') end, onComplete = function() self:note('the cutscene is complete.') end})
    self.cutscene:append(tween.to(self.door, 0.6, {open = 1}, {ease = 'quadOut'}))
        :append(tween.to(self.hero, 1.2, {x = 700}, {ease = 'sineInOut'}))
        :join(tween.to(self.hero, 0.4, {alpha = 1}))
        :append(0.3)
        :append(function()
            self.waved = haylen.elapsed()
            self:note('the hero waves.')
        end)
        :addLabel('fight')
        :append(tween.to(self.hero, 0.25, {hop = 1}, {ease = 'quadOut', loopMode = 'yoyo', repeatCount = 3}))
        :insert('fight', tween.to(self.sky, 1, {light = 0.3}))
        :insert(1, function()
            self.creaked = haylen.elapsed()
            self:note('the door creaks.')
        end)
end

function Timelines:update(dt)
    Timelines.super.update(self, dt)
    if input.pressed('replay') then
        self:play()
    end
    local hero = self.hero
    self.running = self.lastX ~= nil and math.abs(hero.x - self.lastX) > 0.01
    self.facingLeft = self.running and hero.x < self.lastX or (not self.running and self.facingLeft)
    self.lastX = hero.x
    local line = self.cutscene
    self:status(string.format('The cutscene is at %.2f of %.2f seconds and %s%s.', line.time, line.duration, line.paused and 'paused' or line.completed and 'complete' or 'playing', line.reversed and ', reversed' or ''))
end

-- The frame of the walker sheet: running while the hero moves, swinging while it hops at the dummy, and standing otherwise.
function Timelines:heroFrame()
    local time = haylen.elapsed()
    local frame = 1 + math.floor(time * 6) % 4
    if self.hero.hop > 0.01 then
        frame = 17 + math.floor(time * 12) % 6
    elseif self.running then
        frame = 9 + math.floor(time * 14) % 8
    end
    return {(frame - 1) % 8 * kFrame, (frame - 1) // 8 * kFrame, kFrame, kFrame}
end

function Timelines:drawScene()
    local light = self.sky.light
    local dusk = (1 - light) / 0.7
    graphics2d.draw(self.day, 0, 0, {width = kView[1], height = 560, pivotX = 0, pivotY = 0})
    graphics2d.draw(self.dusk, 0, 0, {width = kView[1], height = 560, pivotX = 0, pivotY = 0, color = m.color(1, 1, 1, dusk)})
    local shade = m.color(1 - dusk * 0.45, 1 - dusk * 0.5, 1 - dusk * 0.3, 1)
    for copy = 0, 1 do
        graphics2d.draw(self.hills, copy * 877, kGround + 10, {width = 877, height = 430, pivotX = 0, pivotY = 1, color = shade, layer = 1})
        graphics2d.draw(self.ground, copy * 860 - 40, kGround - 36, {width = 860, height = 205, pivotX = 0, pivotY = 0, color = shade, layer = 2})
    end

    local house, door = kHouse.source, kHouse.door
    local top = kGround - house[4] + 12
    graphics2d.draw(self.props, kHouse.x, top, {source = house, width = house[3], height = house[4], pivotX = 0, pivotY = 0, color = shade, layer = 3})
    local doorway = {kHouse.x + door[1], top + door[2], door[3], door[4]}
    graphics2d.drawShape(doorway, {radius = {28, 28, 0, 0}, color = '#FF1A1210', layer = 4})
    local open = self.door.open
    if open < 0.98 then
        local width = door[3] * (1 - open)
        graphics2d.pushMask(function()
            graphics2d.drawShape({doorway[1], doorway[2], width, doorway[4]}, {radius = {math.min(28, width / 2), math.min(28, width / 2), 0, 0}})
        end, {layer = 5})
        graphics2d.draw(self.props, doorway[1], doorway[2], {source = {house[1] + door[1], house[2] + door[2], door[3], door[4]}, width = width, height = door[4], pivotX = 0, pivotY = 0, color = shade})
        graphics2d.popMask()
    end
    if self.creaked and haylen.elapsed() - self.creaked < 0.9 then
        graphics2d.drawText(nil, 'Creak!', doorway[1] + door[3] + 16, doorway[2] - 10, {size = 28, color = '#FFFFF3C4', outlineWidth = 4, outlineColor = '#FF1E1B33', rotation = -0.15, layer = 6})
    end

    graphics2d.draw(self.dummy, 1150, kGround + 18, {width = 100, height = 237, pivotX = 0.5, pivotY = 1, color = shade, layer = 5})
    local hero = self.hero
    local x, y = kHeroLeft + hero.x, kGround + 22 - hero.hop * 80
    graphics2d.drawShape({x - 46, kGround + 10, 92, 20}, {kind = 'ellipse', color = m.color(0, 0, 0, 0.25 * hero.alpha * (1 - hero.hop * 0.5)), layer = 5})
    graphics2d.draw(self.walker, x, y, {source = self:heroFrame(), width = kHeroSize, height = kHeroSize, pivotX = 0.5, pivotY = 0.94, flipHorizontal = self.facingLeft, color = m.color(1, 1, 1, hero.alpha), layer = 6})
    if self.waved and haylen.elapsed() - self.waved < 1.2 then
        local bubble = {x - 20, y - kHeroSize - 30, 140, 52}
        graphics2d.drawShape(bubble, {radius = 18, color = '#FFFFFBEF', borderWidth = 4, borderColor = '#FF1E1B33', layer = 7})
        graphics2d.drawText(nil, 'Hello!', bubble[1] + 70, bubble[2] + 26, {size = 26, color = '#FF3B2A1E', anchor = {0.5, 0.5}, layer = 8})
    end
end

-- The chart of the cutscene: a row of spans and calls for each target, the label "fight", and the cursor at the time of the timeline.
function Timelines:drawChart()
    local line = self.cutscene
    local card = {0, 572, kView[1], kView[2] - 572}
    graphics2d.drawShape(card, {radius = 18, color = '#FF1C2130', borderWidth = 2, borderColor = Test.line})
    local left, right = 150, kView[1] - 40
    local function toX(seconds)
        return left + (right - left) * seconds / line.duration
    end
    for second = 0, math.floor(line.duration * 2) do
        local x = toX(second / 2)
        graphics2d.drawLine(x, 594, x, 728, 1, '#FF2A3043')
        if second % 2 == 0 then
            graphics2d.drawText(nil, string.format('%d s', second // 2), x, 730, {size = 16, color = Test.muted, anchor = {0.5, 0}})
        end
    end
    for index, row in ipairs(kRows) do
        local y = 598 + (index - 1) * 34
        graphics2d.drawText(nil, row.title, 28, y + 15, {size = 21, color = Test.ink, anchor = {0, 0.5}})
        for _, span in ipairs(row.spans or {}) do
            local from, to = toX(span[1]), toX(span[2])
            local thin = span[3] == 'Fades in'
            local rect = thin and {from, y + 22, to - from, 8} or {from, y + 2, to - from, 26}
            graphics2d.drawShape(rect, {radius = rect[4] / 2, color = thin and Test.violet or Test.accent, layer = thin and 2 or 1})
            if not thin then
                graphics2d.drawText(nil, span[3], from + 14, y + 15, {size = 16, color = '#FF14213D', anchor = {0, 0.5}, layer = 2})
            end
        end
        for _, call in ipairs(row.calls or {}) do
            local x = toX(call[1])
            graphics2d.drawPolygon({{x, y + 4}, {x + 11, y + 15}, {x, y + 26}, {x - 11, y + 15}}, Test.warm, {layer = 3})
            graphics2d.drawText(nil, call[2], x + 16, y + 15, {size = 16, color = Test.warm, anchor = {0, 0.5}, layer = 3})
        end
    end
    local fight = toX(kFight)
    graphics2d.drawStroke({fight, 588, fight, 724}, {width = 2, color = Test.warm, dashes = {6, 5}, layer = 3})
    graphics2d.drawText(nil, 'Label "fight"', fight + 8, 582, {size = 17, color = Test.warm, anchor = {0, 0}, layer = 3})
    local cursor = toX(line.time)
    graphics2d.drawShape({cursor - 2, 584, 4, 148}, {radius = 2, color = Test.ink, layer = 4})
    graphics2d.drawShape({cursor - 8, 578, 16, 16}, {kind = 'ellipse', color = Test.ink, borderWidth = 3, borderColor = '#FF1E1B33', layer = 4})
end

function Timelines:draw(area)
    graphics2d.pushClip({0, 0, kView[1], 560})
    self:drawScene()
    graphics2d.popClip()
    self:drawChart()
end

return Timelines
