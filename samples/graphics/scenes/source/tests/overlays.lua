-- Transparent overlays: a scene with `transparent = true` lets the scenes below keep rendering, while only the top scene updates and receives input.
local haylen = require('haylen')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local sample = require('sample')

local Overlays = haylen.class('Overlays', sample.Test)

Overlays.hints = 'Push overlays on the world. The balls move in "update" and stop under an overlay, while the wheel turns in "render" and keeps turning. Escape or the B button pops the top overlay.'

-- An overlay that dims what is below and shows a panel, cascaded by its depth.
local Overlay = haylen.class('Overlay', scene.Scene)

function Overlay:init(depth, kind)
    self.depth = depth
    self.kind = kind
    self.transparent = true
    self.updates = 0
end

function Overlay:update(dt)
    self.updates = self.updates + 1
    if input.pressed('back') and not scene.transitioning() then
        scene.pop()
    end
end

function Overlay:renderUi()
    graphics2d.beginScreen()
    local area = graphics2d.canvasBounds()
    local offset = (self.depth - 1) * 60
    if self.kind == 'dialog' then
        graphics2d.drawRect(area, '#60000000')
    end
    local panel = {area.x + 380 + offset, area.y + 360 + offset, 700, self.kind == 'hud' and 120 or 300}
    graphics2d.drawRect(panel, self.kind == 'hud' and '#C0203048' or '#F0283048', {layer = 1})
    graphics2d.drawRectOutline(panel, 4, '#FF8FB0FF', {layer = 2})
    local text = string.format('A "%s" overlay at depth %d\nUpdates while on top: %d', self.kind, self.depth, self.updates)
    graphics2d.drawText(nil, text, panel[1] + 30, panel[2] + 24, {size = 30, layer = 3})
end

function Overlays:init(entry)
    Overlays.super.init(self, entry)
    self.updates = 0
    self.balls = {}
    for index = 1, 12 do
        self.balls[index] = {x = index * 140, y = 600 + (index % 3) * 90, speed = 120 + index * 25}
    end
end

function Overlays:controls()
    return {
        ui.button{text = 'Push a HUD overlay', align = 'stretch', onClick = function() self:push('hud') end},
        ui.button{text = 'Push a dialog overlay', align = 'stretch', onClick = function() self:push('dialog') end},
        ui.button{text = 'Pop the top overlay', align = 'stretch', onClick = function() self:popTop() end},
    }
end

function Overlays:push(kind)
    if not scene.transitioning() then
        scene.push(Overlay(scene.size(), kind))
    end
end

function Overlays:popTop()
    if scene.size() > 1 and not scene.transitioning() then
        scene.pop()
    end
end

function Overlays:update(dt)
    self.updates = self.updates + 1
    for _, ball in ipairs(self.balls) do
        ball.x = (ball.x + ball.speed * dt) % 1920
    end
end

-- Rendering goes on under the overlays, so the status shows here how long the world has been standing still.
function Overlays:renderUi()
    self:setStatus(string.format('%d scenes, the world updated %d times', scene.size(), self.updates))
end

function Overlays:render()
    graphics2d.beginScreen()
    local area = graphics2d.canvasBounds()
    graphics2d.drawRect(area, '#FF223A2A')
    for _, ball in ipairs(self.balls) do
        graphics2d.drawCircle(ball.x, ball.y, 34, '#FFFFD040')
    end
    local angle = haylen.elapsed()
    for spoke = 0, 5 do
        local turn = angle + spoke * math.pi / 3
        graphics2d.drawLine(1600, 820, 1600 + math.cos(turn) * 140, 820 + math.sin(turn) * 140, 12, '#FFE0E0E0')
    end
    graphics2d.drawRing(1600, 820, 140, 12, '#FFE0E0E0')
end

return Overlays
