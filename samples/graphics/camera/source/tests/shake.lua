-- Shake: trauma decays over time and the shake grows with its square, so small hits stay subtle, and camera:shake moves the view only along one direction, like a recoil.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Player = require('player')
local World = require('world')
local sample = require('sample')

local Shake = haylen.class('Shake', sample.Test)

Shake.hints = 'Click, tap, F or the X button adds trauma too. Walk with WASD or the left stick to feel the shake while moving.'

function Shake:init(entry)
    Shake.super.init(self, entry)
    self.world = World()
    self.player = Player(0, 0, '#FFFFD040', 'move')
    self.camera = graphics2d.newCamera()
    self.camera.maxShakeOffset = 40
    self.camera.maxShakeAngle = 0.06
end

function Shake:controls()
    local camera = self.camera
    return {
        ui.row{gap = 8, children = {
            ui.button{text = 'Hit', onClick = function() camera:addTrauma(0.35) end},
            ui.button{text = 'Blast', onClick = function() camera:addTrauma(0.9) end},
            ui.button{text = 'Recoil', onClick = function() camera:shake(0.7, 1, 0) end},
        }},
        ui.formField{label = 'Frequency', ui.slider{min = 2, max = 60, value = camera.shakeFrequency, showValue = true, decimals = 0, onChange = function(event)
            camera.shakeFrequency = event.value
        end}},
        ui.formField{label = 'Largest offset', ui.slider{min = 0, max = 120, value = camera.maxShakeOffset, showValue = true, decimals = 0, onChange = function(event)
            camera.maxShakeOffset = event.value
        end}},
        ui.formField{label = 'Trauma lost per second', ui.slider{min = 0.2, max = 4, value = camera.traumaDecay, showValue = true, onChange = function(event)
            camera.traumaDecay = event.value
        end}},
    }
end

function Shake:update(dt)
    Shake.super.update(self, dt)
    if sample.pressed() then
        self.camera:addTrauma(0.5)
    end
    self.player:update(dt)
    self.camera:follow(self.player.x, self.player.y, dt)
    self.camera:update(dt)
    local offset = self.camera:shakeOffset()
    self:setStatus(string.format('trauma %.2f, offset %.1f, %.1f, angle %.3f', self.camera.trauma, offset.x, offset.y, self.camera:renderRotation()))
end

function Shake:render()
    graphics2d.beginWorld(self.camera)
    self.world:draw()
    self.player:draw()
end

function Shake:renderUi()
    graphics2d.beginScreen()
    local area = graphics2d.canvasBounds()
    local x, y = area.x + 40, area:bottom() - 120
    graphics2d.drawRect({x, y, 400, 24}, '#80000000')
    graphics2d.drawRect({x, y, 400 * self.camera.trauma, 24}, '#FFFF6040', {layer = 1})
end

return Shake
