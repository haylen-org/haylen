-- Rain: a rectangle emitter above the view drops fast streaks, and a thin rectangle on the ground throws up the splashes.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local art = require('art')
local sample = require('sample')

local Rain = haylen.class('Rain', sample.Test)

Rain.hints = 'Change how hard it rains. The streaks start above the view and fall behind the ground, and the splashes come from a line along it.'

Rain.ground = 330

function Rain:init(entry)
    Rain.super.init(self, entry)
    self.camera = graphics2d.newCamera()
    self.intensity = 1
    self.drops = particles2d.newEmitter({texture = art.texture('raindrop'), rate = 500, lifetime = 0.95, speed = {1150, 1350}, direction = 1.62, spread = 0.02, startSize = {36, 52}, endSize = {36, 52}, colors = {'#90B0C8FF', '#90B0C8FF'}, shape = 'rectangle', shapeSize = {1100, 10}, maxParticles = 2048, layer = 1, seed = 4})
    self.drops.position = {-40, -820}
    self.splashes = particles2d.newEmitter({texture = art.texture('spark'), rate = 260, lifetime = {0.25, 0.45}, speed = {80, 200}, spread = 1.4, gravity = {0, 900}, startSize = {4, 7}, endSize = 2, colors = {'#C0D0E8FF', '#00D0E8FF'}, shape = 'rectangle', shapeSize = {1000, 4}, maxParticles = 1024, layer = 3, seed = 5})
    self.splashes.position = {0, Rain.ground}
end

function Rain:controls()
    return {ui.formField{label = 'Intensity', ui.slider{min = 0, max = 3, value = self.intensity, showValue = true, onChange = function(event)
        self.intensity = event.value
        self.drops:configure({rate = 500 * event.value})
        self.splashes:configure({rate = 260 * event.value})
    end}}}
end

function Rain:update(dt)
    self.drops:update(dt)
    self.splashes:update(dt)
    self:setStatus(string.format('%d drops, %d splashes, intensity %.2f', self.drops.count, self.splashes.count, self.intensity))
end

function Rain:render()
    graphics2d.beginWorld(self.camera)
    art.backdrop({0.12, 0.14, 0.2}, {0.25, 0.28, 0.34})
    graphics2d.drawRect({-1600, Rain.ground, 3200, 800}, '#FF1E2630', {layer = 2})
    self.drops:draw()
    self.splashes:draw()
end

return Rain
