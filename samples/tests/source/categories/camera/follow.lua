-- Dead zone and drag margins: the view stays put while the walker moves inside a box around its center, a fixed dead zone in world units or drag margins in fractions of the view.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local Walker = require('categories.camera.walker')
local World = require('categories.camera.world')
local actions = require('categories.camera.actions')

local Follow = haylen.class('Follow', Test)

Follow.modes = {{id = 'none', text = 'Nothing, locked on'}, {id = 'deadZone', text = 'Dead zone of 420 by 260'}, {id = 'drag', text = 'Drag margins of half the view'}}

function Follow:init(entry)
    Follow.super.init(self, entry)
    self.world = World()
    self.walker = Walker(0, 0, '#FFFFD040', 'move')
    self.camera = graphics2d.newCamera()
    self.mode = 'deadZone'
    self:apply()
end

function Follow:enter()
    self:loadActions(actions)
    self:frame{
        hint = 'Walk with WASD, the arrows, the left stick or by holding the mouse button or a finger. The box of "camera:drawDebug" is where the walker moves without moving the view.',
        play = true,
        controls = {ui.formField{label = 'Follow with', ui.radioGroup{id = 'mode', selected = self.mode, items = Follow.modes, onChange = function(event)
            self.mode = event.value
            self:apply()
        end}}},
    }
end

function Follow:apply()
    local camera = self.camera
    camera.deadZone = self.mode == 'deadZone' and {420, 260} or {0, 0}
    camera.dragHorizontal = self.mode == 'drag'
    camera.dragVertical = self.mode == 'drag'
    camera.dragMargins = {0.5, 0.35, 0.5, 0.35}
    camera:align()
end

function Follow:update(dt)
    Follow.super.update(self, dt)
    self.walker:update(dt, self.camera)
    self.camera:follow(self.walker.x, self.walker.y, dt)
    self.camera:update(dt)
    self:status(string.format('Walker %.0f, %.0f   Camera %.0f, %.0f', self.walker.x, self.walker.y, self.camera.x, self.camera.y))
end

function Follow:draw(area)
    self.world:draw()
    self.walker:draw()
    self.camera:drawDebug({layer = 10})
end

return Follow
