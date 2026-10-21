-- Bursts and prewarm: bursts fire at their times in the emission cycle, a cycle without `loop` stops the emitter, and prewarm simulates seconds on the first update so an effect starts grown. The test replays the one-shot cycle and restarts the plumes on its own, so the comparisons keep repeating.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local Bursts = haylen.class('Bursts', ParticleTest)

Bursts.code = [[
local bursts = {{time = 0, count = 40}, {time = 0.5, count = 12}, {time = 0.8, count = 12}, {time = 1.2, count = 60}}
local pops = particles2d.newEmitter({texture = dot, rate = 0, bursts = bursts, duration = 2, loop = true})
local plume = particles2d.newEmitter({texture = smoke, rate = 10, lifetime = {3, 4}, prewarm = 4})
local once = particles2d.newEmitter({texture = dot, rate = 80, duration = 1.2})  -- Then stops.
once:restart()  -- Plays the cycle again.]]

Bursts.replayEvery = 3
Bursts.restartEvery = 6
Bursts.vents = {-150, 150}

function Bursts:init(entry)
    Bursts.super.init(self, entry)
    local soft, smoke = 'soft-dot', 'smoke'
    local plume = ParticleTest.look(smoke, {rate = 10, lifetime = {3, 4}, speed = {60, 90}, spread = 0.3, gravity = {10, -10}, startSize = {40, 50}, endSize = {140, 180}, spin = {-0.5, 0.5}, colors = {'#00C0C8D0', '#A0C0C8D0', '#00C0C8D0'}, layer = 2})
    self.scheduled = particles2d.newEmitter(ParticleTest.look(soft, {rate = 0, bursts = {{time = 0, count = 40}, {time = 0.5, count = 12}, {time = 0.8, count = 12}, {time = 1.2, count = 60}}, duration = 2, loop = true, lifetime = {0.6, 1}, speed = {150, 300}, spread = 6.2832, damping = 2, startSize = {20, 28}, endSize = 2, colors = {'#FFFFE070', '#00FF6020'}, blend = 'additive', layer = 2, seed = 40}))
    self.scheduled.position = {-640, -40}
    plume.prewarm, plume.seed = 4, 41
    self.warm = particles2d.newEmitter(plume)
    self.warm.position = {Bursts.vents[1], 260}
    plume.prewarm, plume.seed = 0, 42
    self.cold = particles2d.newEmitter(plume)
    self.cold.position = {Bursts.vents[2], 260}
    self.once = particles2d.newEmitter(ParticleTest.look(soft, {rate = 80, duration = 1.2, lifetime = {0.8, 1.2}, speed = {80, 160}, spread = 0.8, gravity = {0, 200}, startSize = 20, endSize = 4, colors = {'#FF80FFA0', '#0040C060'}, blend = 'additive', layer = 2, seed = 43}))
    self.once.position = {640, 0}
    self.emitters = {self.scheduled, self.warm, self.cold, self.once}
    self.sinceReplay, self.sinceRestart = 0, 0
end

function Bursts:enter()
    self:frame{
        hint = 'The one shot plays again every 3 seconds and the plumes restart every 6, or restart them yourself, to compare a prewarmed plume with a cold one.',
        code = Bursts.code,
        controls = {
            ui.button{id = 'plumes', text = 'Restart the plumes', onClick = function()
                self:restartPlumes()
            end},
            ui.button{id = 'once', text = 'Replay the one shot', onClick = function()
                self:replay()
            end},
        },
        focus = 'plumes',
    }
end

function Bursts:restartPlumes()
    self.warm:restart()
    self.cold:restart()
    self.sinceRestart = 0
end

function Bursts:replay()
    self.once:restart()
    self.sinceReplay = 0
end

function Bursts:update(dt)
    Bursts.super.update(self, dt)
    self.sinceReplay = self.sinceReplay + dt
    self.sinceRestart = self.sinceRestart + dt
    if self.sinceReplay > Bursts.replayEvery then
        self:replay()
    end
    if self.sinceRestart > Bursts.restartEvery then
        self:restartPlumes()
    end
    for _, emitter in ipairs(self.emitters) do
        emitter:update(dt)
    end
    local once = self.once.emitting and 'emits' or self.once.alive and 'fades out' or 'waits'
    self:report('The burst cycle is at %.1f seconds, the plumes restarted %.1f seconds ago, and the one shot %s.', self.scheduled.cycleTime, self.sinceRestart, once)
end

-- Draws the iron vent a plume rises from.
function Bursts.drawVent(x, y)
    graphics2d.drawShape({x - 34, y - 10, 68, 90}, {radius = 8, color = '#FF5A6080', borderWidth = 5, borderColor = '#FF1E1B33', layer = 3})
    graphics2d.drawShape({x - 44, y - 22, 88, 26}, {radius = 8, color = '#FF7A82A4', borderWidth = 5, borderColor = '#FF1E1B33', layer = 3})
end

function Bursts:draw(area)
    ParticleTest.backdrop({0.05, 0.06, 0.1}, {0.1, 0.12, 0.2})
    for _, x in ipairs(Bursts.vents) do
        Bursts.drawVent(x, 270)
    end
    for _, emitter in ipairs(self.emitters) do
        emitter:draw()
    end
    ParticleTest.label('Bursts', -640, 380, 'At 0, 0.5, 0.8 and 1.2 seconds')
    ParticleTest.label('Prewarmed', -150, 380, 'prewarm = 4')
    ParticleTest.label('Cold', 150, 380, 'No prewarm')
    ParticleTest.label('One shot', 640, 380, 'duration = 1.2')
end

return Bursts
