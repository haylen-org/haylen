-- Blending cameras: `graphics2d.blendCameras` returns a view between two cameras, so a tween of the amount makes a smooth cut from the player to a landmark and back.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Player = require('player')
local World = require('world')
local sample = require('sample')

local Blend = haylen.class('Blend', sample.Test)

Blend.hints = 'Cut to the pond and back. The player keeps walking with WASD or the left stick while the view blends.'

function Blend:init(entry)
    Blend.super.init(self, entry)
    self.world = World()
    self.player = Player(-600, 300, '#FFFFD040', 'move')
    self.follow = graphics2d.newCamera()
    self.follow.positionSmoothing = true
    self.landmark = graphics2d.newCamera()
    self.landmark.position = {600, -300}
    self.landmark.zoom = {0.55, 0.55}
    self.landmark.rotation = 0.25
    self.cut = {amount = 0}
    self.view = self.follow
end

function Blend:blendTo(amount)
    tween.to(self.cut, 1.4, {amount = amount}, {ease = 'cubicInOut', owner = self, overwrite = true})
end

function Blend:controls()
    return {
        ui.button{text = 'Cut to the pond', onClick = function() self:blendTo(1) end},
        ui.button{text = 'Back to the player', onClick = function() self:blendTo(0) end},
    }
end

function Blend:update(dt)
    self.player:update(dt, self.view)
    self.follow:follow(self.player.x, self.player.y, dt)
    self.follow:update(dt)
    self.view = graphics2d.blendCameras(self.follow, self.landmark, self.cut.amount)
    self:setStatus(string.format('amount %.2f, zoom %.2f, rotation %.2f', self.cut.amount, self.view.zoom.x, self.view.rotation))
end

function Blend:render()
    graphics2d.beginWorld(self.view)
    self.world:draw()
    self.player:draw()
end

return Blend
