-- Smoothing and look-ahead: position smoothing eases the view toward its goal, and the look-ahead moves the goal ahead of the player by its velocity.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Player = require('player')
local World = require('world')
local sample = require('sample')

local Smoothing = haylen.class('Smoothing', sample.Test)

Smoothing.hints = 'Walk with WASD, the left stick or by holding the mouse button or a finger, then stop suddenly: the view catches up smoothly and leads where you walk.'

function Smoothing:init(entry)
    Smoothing.super.init(self, entry)
    self.world = World()
    self.player = Player(0, 0, '#FFFFD040', 'move')
    self.camera = graphics2d.newCamera()
    self.camera.positionSmoothing = true
    self.camera.positionSmoothingSpeed = 4
    self.camera.lookAheadTime = 0.4
    self.camera.maxLookAhead = 360
end

function Smoothing:controls()
    local camera = self.camera
    return {
        ui.toggle{text = 'Position smoothing', checked = true, onChange = function(event)
            camera.positionSmoothing = event.checked
        end},
        ui.formField{label = 'Smoothing speed', ui.slider{min = 0.5, max = 15, value = camera.positionSmoothingSpeed, showValue = true, onChange = function(event)
            camera.positionSmoothingSpeed = event.value
        end}},
        ui.formField{label = 'Look-ahead time in seconds', ui.slider{min = 0, max = 1, value = camera.lookAheadTime, showValue = true, onChange = function(event)
            camera.lookAheadTime = event.value
        end}},
    }
end

function Smoothing:update(dt)
    self.player:update(dt, self.camera)
    self.camera:follow(self.player.x, self.player.y, dt)
    self.camera:update(dt)
    local camera = self.camera
    self:setStatus(string.format('lead %.0f, %.0f, smoothing %s at %.1f', camera.x - self.player.x, camera.y - self.player.y, tostring(camera.positionSmoothing), camera.positionSmoothingSpeed))
end

function Smoothing:render()
    graphics2d.beginWorld(self.camera)
    self.world:draw()
    self.player:draw()
    self.camera:drawDebug({layer = 10})
end

return Smoothing
