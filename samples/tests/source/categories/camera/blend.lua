-- Blending cameras: `graphics2d.blendCameras` returns a view between two cameras, so a tween of the amount makes a smooth cut from the walker to a landmark and back.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Test = require('harness.test')
local Walker = require('categories.camera.walker')
local World = require('categories.camera.world')
local actions = require('categories.camera.actions')

local Blend = haylen.class('Blend', Test)

function Blend:init(entry)
    Blend.super.init(self, entry)
    self.world = World()
    self.walker = Walker(-600, 300, '#FFFFD040', 'move')
    self.camera = graphics2d.newCamera()
    self.camera.positionSmoothing = true
    self.landmark = graphics2d.newCamera()
    self.landmark.position = {600, -300}
    self.landmark.zoom = {0.55, 0.55}
    self.landmark.rotation = 0.25
    self.cut = {amount = 0}
end

function Blend:enter()
    self:loadActions(actions)
    self:frame{
        hint = 'Cut to the pond and back. The walker keeps walking with WASD, the arrows or the left stick while the view blends.',
        play = true,
        controls = {
            ui.button{id = 'pond', text = 'Cut to the pond', onClick = function() self:blendTo(1) end},
            ui.button{id = 'walker', text = 'Back to the walker', onClick = function() self:blendTo(0) end},
        },
    }
end

function Blend:blendTo(amount)
    tween.to(self.cut, 1.4, {amount = amount}, {ease = 'cubicInOut', owner = self, overwrite = true})
end

function Blend:update(dt)
    Blend.super.update(self, dt)
    if not self.stage then
        return
    end
    self.landmark.viewport = self.stage
    self.walker:update(dt, self.blended or self.camera)
    self.camera:follow(self.walker.x, self.walker.y, dt)
    self.camera:update(dt)
    self.blended = graphics2d.blendCameras(self.camera, self.landmark, self.cut.amount)
    self:status(string.format('Amount %.2f   Zoom %.2f   Rotation %.2f', self.cut.amount, self.blended.zoom.x, self.blended.rotation))
end

function Blend:render()
    if not self.blended then
        return
    end
    graphics2d.beginWorld(self.blended)
    graphics2d.drawRect(graphics2d.canvasBounds(), Test.stageColor, {layer = -1000})
    self.world:draw()
    self.walker:draw()
end

return Blend
