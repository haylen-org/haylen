-- Fire: three emitters make one fire, additive flames that shrink as they rise, embers that fly higher and smoke above them.
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local Fire = haylen.class('Fire', ParticleTest)

Fire.rates = {smoke = 14, flames = 90, embers = 18}

function Fire:init(entry)
    Fire.super.init(self, entry)
    self.strength = 1
    local smoke = ParticleTest.texture('smoke')
    self.emitters = {
        smoke = particles2d.newEmitter({texture = smoke, frames = ParticleTest.frames(smoke), rate = 14, lifetime = {1.6, 2.4}, speed = {60, 110}, spread = 0.5, gravity = {0, -30}, startSize = {40, 60}, endSize = {140, 180}, spin = {-0.6, 0.6}, colors = {'#00303030', '#60404040', '#00202020'}, shape = 'circle', shapeSize = {20, 0}, layer = 1, seed = 1}),
        flames = particles2d.newEmitter({texture = ParticleTest.texture('soft'), rate = 90, lifetime = {0.5, 0.9}, speed = {90, 170}, spread = 0.35, gravity = {0, -120}, startSize = {100, 140}, endSize = {14, 24}, colors = {'#FFFFF0B0', '#FFFFA030', '#C0FF4010', '#00801000'}, shape = 'circle', shapeSize = {24, 0}, blend = 'additive', layer = 2, seed = 2}),
        embers = particles2d.newEmitter({texture = ParticleTest.texture('spark'), rate = 18, lifetime = {1, 2}, speed = {120, 260}, spread = 0.9, gravity = {0, 60}, damping = 0.4, tangentialAcceleration = {-40, 40}, startSize = {6, 10}, endSize = 2, colors = {'#FFFFE080', '#FFFF6020', '#00FF2000'}, shape = 'circle', shapeSize = {20, 0}, blend = 'additive', layer = 3, seed = 3}),
    }
end

function Fire:enter()
    self:frame{
        hint = 'The fire follows the cursor: move it with the mouse, a finger, the arrows, WASD or a stick. The flames lean as it moves because particles stay in world space.',
        cursor = true,
        controls = {ui.formField{label = 'Strength', ui.slider{id = 'strength', min = 0.2, max = 3, value = self.strength, showValue = true, onChange = function(event)
            self.strength = event.value
            for name, emitter in pairs(self.emitters) do
                emitter:configure({rate = Fire.rates[name] * event.value})
            end
        end}}},
    }
end

function Fire:update(dt)
    Fire.super.update(self, dt)
    local count = 0
    for _, emitter in pairs(self.emitters) do
        emitter.x, emitter.y = self.cursorX, self.cursorY
        emitter:update(dt)
        count = count + emitter.count
    end
    self:status(string.format('Particles %d   Strength %.2f', count, self.strength))
end

function Fire:draw(area)
    ParticleTest.backdrop({0.03, 0.03, 0.08}, {0.12, 0.06, 0.05})
    for _, emitter in pairs(self.emitters) do
        emitter:draw()
    end
end

return Fire
