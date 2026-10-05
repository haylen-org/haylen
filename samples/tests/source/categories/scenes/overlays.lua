-- Transparent overlays: a scene with `transparent = true` lets the scenes below keep rendering, while only the top scene updates and receives input.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local SceneTest = require('categories.scenes.scene-test')

local Overlays = haylen.class('Overlays', SceneTest)

-- An overlay that dims what is below and shows a panel over the play area of the test, cascaded by its depth.
local Overlay = haylen.class('Overlay', scene.Scene)

function Overlay:init(depth, kind, stage)
    self.depth = depth
    self.kind = kind
    self.stage = stage
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
    if self.kind == 'dialog' then
        graphics2d.drawRect(graphics2d.canvasBounds(), '#60000000')
    end
    local offset = (self.depth - 1) * 60
    local panel = {self.stage.x + 60 + offset, self.stage.y + 60 + offset, 700, self.kind == 'hud' and 120 or 300}
    graphics2d.drawRect(panel, self.kind == 'hud' and '#C0203048' or '#F0283048', {layer = 1})
    graphics2d.drawRectOutline(panel, 4, '#FF8FB0FF', {layer = 2})
    local text = string.format('Overlay "%s" at depth %d\nUpdates while on top: %d', self.kind, self.depth, self.updates)
    graphics2d.drawText(nil, text, panel[1] + 30, panel[2] + 24, {size = 30, layer = 3})
end

function Overlays:init(entry)
    Overlays.super.init(self, entry)
    self.updates = 0
    self.balls = {}
    for index = 1, 12 do
        self.balls[index] = {x = index * 140, y = 0.5 + (index % 3) * 0.12, speed = 120 + index * 25}
    end
end

function Overlays:enter()
    self.depth = scene.size()
    self:frame{
        hint = 'Push overlays on the world. The balls move in "update" and stop under an overlay, while the wheel turns in "render" and keeps turning. Escape or the B button pops the top overlay.',
        controls = {
            ui.button{id = 'hud', text = 'Push a HUD overlay', align = 'stretch', onClick = function() self:push('hud') end},
            ui.button{id = 'dialog', text = 'Push a dialog overlay', align = 'stretch', onClick = function() self:push('dialog') end},
            ui.button{id = 'pop', text = 'Pop the top overlay', align = 'stretch', onClick = function() self:popTop() end},
        },
        focus = 'hud',
    }
end

function Overlays:push(kind)
    if not scene.transitioning() then
        scene.push(Overlay(scene.size() - self.depth + 1, kind, self.stage))
    end
end

function Overlays:popTop()
    if scene.size() > self.depth and not scene.transitioning() then
        scene.pop()
    end
end

function Overlays:update(dt)
    Overlays.super.update(self, dt)
    self.updates = self.updates + 1
    if self.area then
        for _, ball in ipairs(self.balls) do
            ball.x = (ball.x + ball.speed * dt) % self.area.width
        end
    end
end

-- Rendering goes on under the overlays while updates stop, so the status line changes here, where it shows how long the world has been standing still.
function Overlays:renderUi()
    local text = string.format('Overlays %d   The world updated %d times', scene.size() - self.depth, self.updates)
    if text ~= self.shown then
        self.shown = text
        self:set('status', {text = text})
    end
end

function Overlays:draw(area)
    graphics2d.drawRect(area, '#FF223A2A')
    for _, ball in ipairs(self.balls) do
        graphics2d.drawCircle(ball.x, area.height * ball.y, 34, '#FFFFD040')
    end
    local angle = haylen.elapsed()
    local x, y = area.width - 200, area.height - 180
    for spoke = 0, 5 do
        local turn = angle + spoke * math.pi / 3
        graphics2d.drawLine(x, y, x + math.cos(turn) * 140, y + math.sin(turn) * 140, 12, '#FFE0E0E0')
    end
    graphics2d.drawRing(x, y, 140, 12, '#FFE0E0E0')
end

return Overlays
