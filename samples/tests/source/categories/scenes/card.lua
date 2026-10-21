-- A scene that shows the island by day or by night over the whole screen or over the play area of a test, with a title on a plaque, which the scene tests push, replace and pop. It reports its hooks to a listener, can take a while to load and can leave by itself.
local assets = require('haylen.assets')
local async = require('async')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local scene = require('haylen.scene')
local timer = require('haylen.timer')
local viewport = require('haylen.viewport')

local Scenery = require('categories.effects.scenery')

local Card = haylen.class('Card', scene.Scene)

-- The landscape is a world of 1920 by 1080 around the origin, which the card covers its area with.
Card.world = {1920, 1080}

-- Options: `title`, `caption`, `night` (the island at night instead of by day), `listener` (a function of the card, the hook name and a detail), `work` (seconds the load takes, in ten steps), `fail` (a message the load fails with), `stay` (seconds before the card pops itself), `leave` (the transition it pops with) and `stage` (the screen rectangle of a play area, which makes the card a transparent panel over it, so the test and its controls stay visible and usable under the card).
function Card:init(options)
    self.title = options.title
    self.caption = options.caption or ''
    self.night = options.night or false
    self.listener = options.listener
    self.work = options.work or 0
    self.fail = options.fail
    self.stay = options.stay
    self.leave = options.leave
    self.stage = options.stage
    self.transparent = self.stage ~= nil
    self.scenery = Scenery()
    self.font = assets.font('fonts/lilita-one-regular.ttf')
    self.camera = graphics2d.newCamera()
    self.camera.anchor = 'center'
    self.camera.position = {0, 0}
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
        context:progress(step / 10, string.format('Loading the island, step %d of 10', step))
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
    self.scenery:update(dt)
    if input.pressed('back') and not scene.transitioning() then
        scene.pop(self.leave)
    end
end

-- Covers the area with the landscape, and puts the title and the caption on a plaque in its upper half. Everything scales with the area, which is 1920 units wide when the card covers the screen.
function Card:render()
    local area = self.stage or viewport.visibleRect()
    local camera = self.camera
    local zoom = math.max(area.width / Card.world[1], area.height / Card.world[2])
    camera.viewport = area
    camera.zoom = {zoom, zoom}
    graphics2d.beginWorld(camera)
    self.scenery:draw(self.night)

    graphics2d.beginScreen()
    local scale = math.min(area.width / 1920, area.height / 1080)
    local centerX, top = area.x + area.width / 2, area.y + area.height * 0.32
    local titleSize, captionSize = 96 * scale, 34 * scale
    local width = math.max(graphics2d.measureText(self.font, self.title, {size = titleSize}), graphics2d.measureText(nil, self.caption, {size = captionSize})) + 120 * scale
    local height = 200 * scale
    graphics2d.drawShape({centerX - width / 2, top - height / 2 + 10 * scale, width, height}, {radius = 40 * scale, color = '#66000000', softness = 30 * scale})
    graphics2d.drawShape({centerX - width / 2, top - height / 2, width, height}, {radius = 36 * scale, color = '#E62F3352', borderWidth = 5 * scale, borderColor = '#FFFFC93C', layer = 1})
    graphics2d.drawText(self.font, self.title, centerX, top - 22 * scale, {size = titleSize, anchor = {0.5, 0.5}, outlineWidth = 5 * scale, outlineColor = '#FF15172A', layer = 2})
    graphics2d.drawText(nil, self.caption, centerX, top + 56 * scale, {size = captionSize, anchor = {0.5, 0.5}, color = '#FFE8EAF2', layer = 2})
    if self.stage then
        graphics2d.drawShape(area, {color = '#00000000', borderWidth = 4, borderColor = '#FF15172A', layer = 3})
    end
end

return Card
