-- Limits: the view stays inside a world rectangle while following. With limit smoothing it eases into the edge instead of stopping at it, and limits smaller than the view center it.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local Walker = require('categories.camera.walker')
local World = require('categories.camera.world')
local actions = require('categories.camera.actions')

local Limits = haylen.class('Limits', Test)

Limits.sizes = {wide = {-1800, -1000, 3600, 2000}, narrow = {-500, -400, 1000, 800}}

function Limits:init(entry)
    Limits.super.init(self, entry)
    self.world = World()
    self.walker = Walker(0, 0, '#FFFFD040', 'move')
    self.camera = graphics2d.newCamera()
    self.camera.positionSmoothing = true
    self.camera.limitSmoothing = true
    self.size = 'wide'
    self.camera.limits = Limits.sizes.wide
end

function Limits:enter()
    self:loadActions(actions)
    self:frame{
        hint = 'Walk to an edge of the meadow with WASD, the arrows, the left stick or by holding the mouse button or a finger. The dark frame is the limit rectangle.',
        play = true,
        controls = {
            ui.toggle{id = 'smoothing', text = 'Limit smoothing', checked = true, onChange = function(event)
                self.camera.limitSmoothing = event.checked
            end},
            ui.formField{label = 'Limits', ui.radioGroup{id = 'size', selected = 'wide', items = {{id = 'wide', text = 'Wider than the view'}, {id = 'narrow', text = 'Narrower, so the view centers'}}, onChange = function(event)
                self.size = event.value
                self.camera.limits = Limits.sizes[event.value]
            end}},
        },
    }
end

function Limits:update(dt)
    Limits.super.update(self, dt)
    self.walker:update(dt, self.camera)
    self.camera:follow(self.walker.x, self.walker.y, dt)
    self.camera:update(dt)
    local view = self.camera:visibleBounds()
    self:status(string.format('View from %.0f, %.0f to %.0f, %.0f', view.x, view.y, view:right(), view:bottom()))
end

function Limits:draw(area)
    self.world:draw()
    self.walker:draw()
    graphics2d.drawRectOutline(Limits.sizes[self.size], 12, '#C0102010', {layer = 9})
    self.camera:drawDebug({layer = 10})
end

return Limits
