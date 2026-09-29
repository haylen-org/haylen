-- Touch: every finger on the screen, up to ten, with its id, phase and time down, a line from where it landed and a trail that fades after it lifts.
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local sample = require('sample')

local Touch = haylen.class('Touch', sample.Test)

local kColors = {'#FF8FB0FF', '#FFF2B23A', '#FF6FDCA0', '#FFFF8A84', '#FFC9A0FF', '#FF5ED3E8', '#FFFFD166', '#FFB8E986', '#FFFF9ED8', '#FFE8EAF2'}
local kFade = 0.8

function Touch:enter()
    -- Trails are kept by finger id, so a finger that lifted keeps fading while new fingers land.
    self.trails = {}
    self.events = 0
    self.most = 0
    self:frame({
        hint = 'Put up to ten fingers on the screen. The mouse does not make touches, so this test needs a touch screen.',
        controls = {
            ui.button{id = 'clear', text = 'Clear the trails', onClick = function() self.trails = {} end},
            ui.sectionTitle{text = 'Reading touches'},
            ui.label{font = 'monospace', text = 'for _, touch in ipairs(input.touches()) do\n  touch.id, touch.phase\n  touch.x, touch.y\n  touch.startX, touch.startY\n  touch.dx, touch.dy\n  touch.duration\nend'},
        },
    })
end

function Touch:event(event)
    if event.type:sub(1, 5) == 'touch' then
        self.events = self.events + 1
    end
end

function Touch:update(dt)
    Touch.super.update(self, dt)
    if not self.area then
        return
    end
    for _, trail in pairs(self.trails) do
        trail.fade = trail.lifted and trail.fade - dt or trail.fade
    end
    for id, trail in pairs(self.trails) do
        if trail.fade <= 0 then
            self.trails[id] = nil
        end
    end

    local touches = input.touches()
    for _, touch in ipairs(touches) do
        local trail = self.trails[touch.id]
        if not trail or trail.lifted and touch.phase == 'began' then
            trail = {points = collections.newRingBuffer(48), color = kColors[touch.id % #kColors + 1], fade = kFade}
            self.trails[touch.id] = trail
        end
        local x, y = self:toStage(touch.x, touch.y)
        local startX, startY = self:toStage(touch.startX, touch.startY)
        trail.points:push({x, y})
        trail.x, trail.y, trail.startX, trail.startY = x, y, startX, startY
        trail.phase, trail.duration = touch.phase, touch.duration
        trail.lifted = touch.phase == 'ended' or touch.phase == 'cancelled'
    end
    self.most = math.max(self.most, #touches)
    self:status(string.format('fingers %d   most at once %d   touch events %d', #touches, self.most, self.events))
end

function Touch:draw(area)
    local count = 0
    for id, trail in pairs(self.trails) do
        count = count + 1
        local alpha = trail.lifted and math.max(0, trail.fade / kFade) or 1
        local color = trail.color
        local points = trail.points:values()
        for index = 2, #points do
            graphics2d.drawLine(points[index - 1][1], points[index - 1][2], points[index][1], points[index][2], 6 * alpha + 1, color, {layer = 1})
        end
        graphics2d.drawRing(trail.startX, trail.startY, 20, 3, color, {layer = 1})
        graphics2d.drawLine(trail.startX, trail.startY, trail.x, trail.y, 2, '#40FFFFFF', {layer = 1})
        graphics2d.drawCircle(trail.x, trail.y, 56 * (0.4 + 0.6 * alpha), trail.lifted and '#40FFFFFF' or color, {layer = 2})
        sample.caption(string.format('id %d  %s  %.1fs', id, trail.phase, trail.duration), trail.x, trail.y - 70, {anchor = {0.5, 1}, color = sample.ink})
    end
    if count == 0 then
        sample.caption('Touch the stage with up to ten fingers', area.width / 2, area.height / 2, {anchor = {0.5, 0.5}, size = 34})
    end
end

return Touch
