-- Many particles: large emitters simulate on worker threads and draw each emitter as one batch, so tens of thousands of particles stay cheap.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local art = require('art')
local sample = require('sample')

local Stress = haylen.class('Stress', sample.Test)

Stress.hints = 'Raise the rate to fill the screen. Four fountains share the particles, up to 25000 each.'

Stress.fountains = 4

function Stress:init(entry)
    Stress.super.init(self, entry)
    self.camera = graphics2d.newCamera()
    self.rate = 8000
    self.frameTime = 1 / 60
    self.emitters = {}
    for index = 1, Stress.fountains do
        local emitter = particles2d.newEmitter({texture = art.texture('spark'), rate = self.rate / Stress.fountains, lifetime = {2.2, 2.8}, speed = {500, 900}, direction = -1.5708, spread = 0.9, gravity = {0, 600}, startSize = {5, 8}, endSize = 2, colors = {m.fromHsv(index / Stress.fountains, 0.6, 1), m.fromHsv(index / Stress.fountains + 0.2, 0.8, 1):withAlpha(0)}, blend = 'additive', maxParticles = 25000, layer = 2, seed = 90 + index})
        emitter.position = {(index - 2.5) * 420, 460}
        self.emitters[index] = emitter
    end
end

function Stress:controls()
    return {ui.formField{label = 'Particles per second', ui.slider{min = 500, max = 40000, step = 500, value = self.rate, showValue = true, decimals = 0, onChange = function(event)
        self.rate = event.value
        for _, emitter in ipairs(self.emitters) do
            emitter:configure({rate = event.value / Stress.fountains})
        end
    end}}}
end

function Stress:update(dt)
    local count = 0
    for _, emitter in ipairs(self.emitters) do
        emitter:update(dt)
        count = count + emitter.count
    end
    self.frameTime = m.lerp(self.frameTime, haylen.unscaledDelta(), 0.05)
    local stats = graphics2d.stats()
    self:setStatus(string.format('%d particles, %d sprites and %d draw calls last frame, %.0f frames per second', count, stats.sprites, stats.drawCalls, 1 / math.max(self.frameTime, 0.0001)))
end

function Stress:render()
    graphics2d.beginWorld(self.camera)
    art.backdrop({0.02, 0.02, 0.05}, {0.05, 0.05, 0.1})
    for _, emitter in ipairs(self.emitters) do
        emitter:draw()
    end
end

return Stress
