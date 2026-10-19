-- A scene with a color, a title and a caption over the whole screen or over the play area of a test, which the scene tests push, replace and pop. It reports its hooks to a listener, can take a while to load and can leave by itself.
local async = require('async')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local scene = require('haylen.scene')
local timer = require('haylen.timer')

local Card = haylen.class('Card', scene.Scene)

-- Options: `title`, `caption`, `color`, `listener` (a function of the card, the hook name and a detail), `work` (seconds the load takes, in ten steps), `fail` (a message the load fails with), `stay` (seconds before the card pops itself), `leave` (the transition it pops with) and `stage` (the screen rectangle of a play area, which makes the card a transparent panel over it, so the test and its controls stay visible and usable under the card).
function Card:init(options)
    self.title = options.title
    self.caption = options.caption or ''
    self.color = options.color
    self.listener = options.listener
    self.work = options.work or 0
    self.fail = options.fail
    self.stay = options.stay
    self.leave = options.leave
    self.stage = options.stage
    self.transparent = self.stage ~= nil
end

function Card:report(hook, detail)
    if self.listener then
        self.listener(self, hook, detail)
    end
end

-- Loads in ten steps that report their progress, which a loading view shows, and fails halfway when the card was made to fail.
function Card:load(context)
    self:report('load', context.params)
    for step = 1, 10 do
        if self.work > 0 then
            async.sleep(self.work * 100):await()
        end
        if self.fail and step == 5 then
            error(self.fail, 0)
        end
        context:progress(step / 10, string.format('Building %s, step %d of 10', self.title, step))
    end
end

function Card:unload()
    self:report('unload')
end

function Card:enter(params)
    self:report('enter', params)
    if self.stay then
        timer.after(self.stay, function()
            if scene.top() == self and not scene.transitioning() then
                scene.pop(self.leave)
            end
        end, {owner = self})
    end
end

function Card:exit()
    self:report('exit')
end

function Card:pause()
    self:report('pause')
end

function Card:resume()
    self:report('resume')
end

function Card:exitTransitionStarted()
    self:report('exitTransitionStarted')
end

function Card:enterTransitionFinished()
    self:report('enterTransitionFinished')
end

function Card:update(dt)
    if input.pressed('back') and not scene.transitioning() then
        scene.pop(self.leave)
    end
end

-- Everything scales with the area, which is 1920 units wide when the card covers the screen.
function Card:render()
    graphics2d.beginScreen()
    local area = self.stage or graphics2d.canvasBounds()
    local scale = math.min(area.width / 1920, area.height / 1080)
    local centerX, centerY = area.x + area.width / 2, area.y + area.height / 2
    graphics2d.drawRect(area, self.color)
    local time = haylen.elapsed()
    for index = 0, 5 do
        local angle = time * 0.4 + index * math.pi / 3
        local x, y = centerX + math.cos(angle) * 520 * scale, area.y + area.height * 0.55 + math.sin(angle) * 260 * scale
        graphics2d.draw(graphics.whiteTexture(), x, y, {width = 120 * scale, height = 120 * scale, rotation = angle, color = '#30FFFFFF'})
    end
    graphics2d.drawText(nil, self.title, centerX, centerY, {size = 120 * scale, anchor = {0.5, 0.5}, outlineWidth = 5 * scale, outlineColor = '#80000000', layer = 1})
    graphics2d.drawText(nil, self.caption, centerX, centerY + 110 * scale, {size = 40 * scale, anchor = {0.5, 0.5}, color = '#E0FFFFFF', layer = 1})
end

return Card
