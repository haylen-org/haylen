-- Blending cameras: `graphics2d.blendCameras` returns a view between two cameras, so a tween of the amount makes a smooth cut from the walker to a landmark and back.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local CameraTest = require('categories.camera.camera-test')
local Test = require('harness.test')
local Walker = require('categories.camera.walker')
local World = require('categories.camera.world')
local actions = require('categories.camera.actions')

local Blend = haylen.class('Blend', CameraTest)

Blend.code = [[
local landmark = graphics2d.newCamera()
landmark.position = {820, 380}
landmark.zoom = {1.5, 1.5}
landmark.rotation = 0.15
tween.to(cut, 1.4, {amount = 1}, {ease = 'cubicInOut', owner = self})  -- Cuts from the walker to the shrine.
graphics2d.beginWorld(graphics2d.blendCameras(camera, landmark, cut.amount))]]

-- Until a player presses a button, the view cuts to the shrine and back every few seconds.
Blend.beat = 3.5
Blend.route = {{-400, 230, 1}, {-1300, 200, 1}}

function Blend:init(entry)
    Blend.super.init(self, entry)
    self.world = World()
    self.walker = Walker(-1300, 200, {action = 'move', route = Blend.route})
    self.camera = graphics2d.newCamera()
    self.camera.positionSmoothing = true
    self.camera:snapTo(self.walker.x, self.walker.y)
    self.landmark = graphics2d.newCamera()
    self.landmark.position = {World.shrine.x, World.shrine.y - 90}
    self.landmark.zoom = {1.5, 1.5}
    self.landmark.rotation = 0.15
    self.cut = {amount = 0}
    self.time = 1.5
    self.cycling = true
end

function Blend:enter()
    self:loadActions(actions)
    self:frame{
        hint = 'Cut to the shrine and back. The walker keeps walking with WASD, the arrows or the left stick while the view blends.',
        code = Blend.code,
        play = true,
        controls = {
            ui.button{id = 'shrine', text = 'Cut to the shrine', onClick = function() self:blendTo(1, true) end},
            ui.button{id = 'walker', text = 'Back to the walker', onClick = function() self:blendTo(0, true) end},
        },
    }
end

function Blend:blendTo(amount, chosen)
    if chosen then
        self.cycling = false
    end
    self.target = amount
    tween.to(self.cut, 1.4, {amount = amount}, {ease = 'cubicInOut', owner = self, overwrite = true})
end

function Blend:update(dt)
    Blend.super.update(self, dt)
    if not self.stage then
        return
    end
    if self.cycling then
        self.time = self.time + dt
        if self.time >= Blend.beat then
            self.time = 0
            self:blendTo(self.target == 1 and 0 or 1)
        end
    end
    self.landmark.viewport = self.stage
    self.walker:update(dt, self.world, self.blended or self.camera)
    self.camera:follow(self.walker.x, self.walker.y, dt)
    self.camera:update(dt)
    self.blended = graphics2d.blendCameras(self.camera, self.landmark, self.cut.amount)
    local amount = self.cut.amount
    if amount <= 0.001 then
        self:status('The view follows the walker.')
    elseif amount >= 0.999 then
        self:status('The view rests on the shrine.')
    else
        self:status(string.format('The view is %d percent of the way to the shrine.', math.floor(amount * 100 + 0.5)))
    end
end

function Blend:render()
    if not self.blended then
        return
    end
    graphics2d.beginWorld(self.blended, {sort = 'y', visibilityMask = World.view})
    graphics2d.drawRect(graphics2d.canvasBounds(), Test.stageColor, {layer = -1000})
    self.world:draw()
    self.walker:draw()
end

return Blend
