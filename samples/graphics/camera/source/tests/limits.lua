-- Limits: the view stays inside a world rectangle while following. With limit smoothing it eases into the edge instead of stopping at it, and limits smaller than the view center it.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Player = require('player')
local World = require('world')
local sample = require('sample')

local Limits = haylen.class('Limits', sample.Test)

Limits.hints = 'Walk to an edge of the meadow with WASD, the left stick or by holding the mouse button or a finger. The dark frame is the limit rectangle.'

Limits.sizes = {wide = {-1800, -1000, 3600, 2000}, narrow = {-500, -400, 1000, 800}}

function Limits:init(entry)
    Limits.super.init(self, entry)
    self.world = World()
    self.player = Player(0, 0, '#FFFFD040', 'move')
    self.camera = graphics2d.newCamera()
    self.camera.positionSmoothing = true
    self.camera.limitSmoothing = true
    self.size = 'wide'
    self.camera.limits = Limits.sizes.wide
end

function Limits:controls()
    return {
        ui.toggle{text = 'Limit smoothing', checked = true, onChange = function(event)
            self.camera.limitSmoothing = event.checked
        end},
        ui.formField{label = 'Limits', ui.radioGroup{selected = 'wide', items = {{id = 'wide', text = 'Wider than the view'}, {id = 'narrow', text = 'Narrower, so the view centers'}}, onChange = function(event)
            self.size = event.value
            self.camera.limits = Limits.sizes[event.value]
        end}},
    }
end

function Limits:update(dt)
    self.player:update(dt, self.camera)
    self.camera:follow(self.player.x, self.player.y, dt)
    self.camera:update(dt)
    local view = self.camera:visibleBounds()
    self:setStatus(string.format('view from %.0f, %.0f to %.0f, %.0f', view.x, view.y, view:right(), view:bottom()))
end

function Limits:render()
    graphics2d.beginWorld(self.camera)
    self.world:draw()
    self.player:draw()
    graphics2d.drawRectOutline(Limits.sizes[self.size], 12, '#C0102010', {layer = 9})
    self.camera:drawDebug({layer = 10})
end

return Limits
