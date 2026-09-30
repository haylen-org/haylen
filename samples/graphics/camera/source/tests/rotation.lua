-- Rotation: the view turns around its center, rotation smoothing eases the drawn angle the short way around, and `ignoreRotation` keeps the view upright whatever the rotation and the shake.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local Player = require('player')
local World = require('world')
local sample = require('sample')

local Rotation = haylen.class('Rotation', sample.Test)

Rotation.hints = 'Turn with Q and E or the right stick, or a quarter turn at a time with the button. Walk with WASD, the left stick or by holding the mouse button or a finger.'

function Rotation:init(entry)
    Rotation.super.init(self, entry)
    self.world = World()
    self.player = Player(0, 0, '#FFFFD040', 'move')
    self.camera = graphics2d.newCamera()
    self.camera.rotationSmoothing = true
    self.camera.rotationSmoothingSpeed = 4
    self.camera.maxShakeAngle = 0.15
end

function Rotation:controls()
    local camera = self.camera
    return {
        ui.row{gap = 8, children = {
            ui.button{text = 'Quarter turn', onClick = function() camera.rotation = camera.rotation + math.pi / 2 end},
            ui.button{text = 'Shake', onClick = function() camera:addTrauma(0.8) end},
        }},
        ui.toggle{text = 'Rotation smoothing', checked = true, onChange = function(event)
            camera.rotationSmoothing = event.checked
        end},
        ui.toggle{text = 'ignoreRotation', onChange = function(event)
            camera.ignoreRotation = event.checked
        end},
    }
end

function Rotation:update(dt)
    local camera = self.camera
    camera.rotation = camera.rotation + input.value('rotate') * 1.5 * dt
    self.player:update(dt, camera)
    camera:follow(self.player.x, self.player.y, dt)
    camera:update(dt)
    self:setStatus(string.format('rotation %.2f, drawn with %.2f, ignoreRotation %s', camera.rotation, camera:renderRotation(), tostring(camera.ignoreRotation)))
end

function Rotation:render()
    graphics2d.beginWorld(self.camera)
    self.world:draw()
    self.player:draw()
end

return Rotation
