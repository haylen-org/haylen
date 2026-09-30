-- Dead zone and drag margins: the view stays put while the player moves inside a box around its center, a fixed dead zone in world units or drag margins in fractions of the view.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Player = require('player')
local World = require('world')
local sample = require('sample')

local Follow = haylen.class('Follow', sample.Test)

Follow.hints = 'Walk with WASD, the left stick or by holding the mouse button or a finger. The box drawn by "camera:drawDebug" is where the player moves without moving the view.'

function Follow:init(entry)
    Follow.super.init(self, entry)
    self.world = World()
    self.player = Player(0, 0, '#FFFFD040', 'move')
    self.camera = graphics2d.newCamera()
    self.mode = 'deadZone'
    self:apply()
end

function Follow:apply()
    local camera = self.camera
    camera.deadZone = self.mode == 'deadZone' and {420, 260} or {0, 0}
    camera.dragHorizontal = self.mode == 'drag'
    camera.dragVertical = self.mode == 'drag'
    camera.dragMargins = {0.5, 0.35, 0.5, 0.35}
    camera:align()
end

function Follow:controls()
    return {ui.formField{label = 'Follow with', ui.radioGroup{selected = self.mode, items = {{id = 'none', text = 'Nothing, locked on'}, {id = 'deadZone', text = 'Dead zone of 420 by 260'}, {id = 'drag', text = 'Drag margins of half the view'}}, onChange = function(event)
        self.mode = event.value
        self:apply()
    end}}}
end

function Follow:update(dt)
    self.player:update(dt, self.camera)
    self.camera:follow(self.player.x, self.player.y, dt)
    self.camera:update(dt)
    self:setStatus(string.format('player %.0f, %.0f, camera %.0f, %.0f', self.player.x, self.player.y, self.camera.x, self.camera.y))
end

function Follow:render()
    graphics2d.beginWorld(self.camera)
    self.world:draw()
    self.player:draw()
    self.camera:drawDebug({layer = 10})
end

return Follow
