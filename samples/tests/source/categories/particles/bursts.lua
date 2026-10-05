-- Bursts and prewarm: bursts fire at their times in the emission cycle, a cycle without `loop` stops the emitter, and prewarm simulates seconds on the first update so an effect starts grown.
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local Bursts = haylen.class('Bursts', ParticleTest)

function Bursts:init(entry)
    Bursts.super.init(self, entry)
    local soft, smoke = ParticleTest.texture('soft'), ParticleTest.texture('smoke')
    local plume = {texture = smoke, frames = ParticleTest.frames(smoke), rate = 10, lifetime = {3, 4}, speed = {60, 90}, spread = 0.3, gravity = {10, -10}, startSize = {40, 50}, endSize = {140, 180}, spin = {-0.5, 0.5}, colors = {'#00C0C8D0', '#A0C0C8D0', '#00C0C8D0'}, layer = 2}
    self.scheduled = particles2d.newEmitter({texture = soft, rate = 0, bursts = {{time = 0, count = 40}, {time = 0.5, count = 12}, {time = 0.8, count = 12}, {time = 1.2, count = 60}}, duration = 2, loop = true, lifetime = {0.6, 1}, speed = {150, 300}, spread = 6.2832, damping = 2, startSize = {20, 28}, endSize = 2, colors = {'#FFFFE070', '#00FF6020'}, blend = 'additive', layer = 2, seed = 40})
    self.scheduled.position = {-640, -40}
    plume.prewarm, plume.seed = 4, 41
    self.warm = particles2d.newEmitter(plume)
    self.warm.position = {-120, 260}
    plume.prewarm, plume.seed = 0, 42
    self.cold = particles2d.newEmitter(plume)
    self.cold.position = {120, 260}
    self.once = particles2d.newEmitter({texture = soft, rate = 80, duration = 1.2, lifetime = {0.8, 1.2}, speed = {80, 160}, spread = 0.8, gravity = {0, 200}, startSize = 20, endSize = 4, colors = {'#FF80FFA0', '#0040C060'}, blend = 'additive', layer = 2, seed = 43})
    self.once.position = {640, 0}
    self.emitters = {self.scheduled, self.warm, self.cold, self.once}
end

function Bursts:enter()
    self:frame{
        hint = 'Restart the emitters to compare a prewarmed plume with a cold one and to replay the one-shot cycle.',
        controls = {
            ui.button{id = 'plumes', text = 'Restart the plumes', onClick = function()
                self.warm:restart()
                self.cold:restart()
            end},
            ui.button{id = 'once', text = 'Replay the one shot', onClick = function()
                self.once:restart()
            end},
        },
        focus = 'plumes',
    }
end

function Bursts:update(dt)
    Bursts.super.update(self, dt)
    for _, emitter in ipairs(self.emitters) do
        emitter:update(dt)
    end
    self:status(string.format('Burst cycle %.2f s   One shot emitting "%s" and alive "%s"', self.scheduled.cycleTime, self.once.emitting, self.once.alive))
end

function Bursts:draw(area)
    ParticleTest.backdrop({0.05, 0.06, 0.1}, {0.1, 0.12, 0.2})
    for _, emitter in ipairs(self.emitters) do
        emitter:draw()
    end
    ParticleTest.label('Bursts at 0, 0.5, 0.8 and 1.2 s', -640, 280)
    ParticleTest.label('Prewarm 4 s', -120, 330)
    ParticleTest.label('No prewarm', 120, 330)
    ParticleTest.label('One shot of 1.2 s', 640, 280)
end

return Bursts
