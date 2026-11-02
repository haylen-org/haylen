-- Bursts and prewarm: bursts fire at their times in the emission cycle, a cycle without `loop` stops the emitter, and prewarm simulates seconds on the first update so an effect starts grown. The test replays the one-shot cycle and restarts the plumes on its own, so the comparisons keep repeating.
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')
local Town = require('categories.lighting.town')

local Bursts = haylen.class('Bursts', ParticleTest)

Bursts.code = [[
local bursts = {{time = 0, count = 40}, {time = 0.5, count = 12}, {time = 0.8, count = 12}, {time = 1.2, count = 60}}
local pops = particles2d.newEmitter({texture = dot, rate = 0, bursts = bursts, duration = 2, loop = true})
local plume = particles2d.newEmitter({texture = smoke, rate = 8, lifetime = {5, 6}, prewarm = 6})
local once = particles2d.newEmitter({texture = drop, rate = 80, duration = 1.2})  -- Then stops.
once:restart()  -- Plays the cycle again.]]

Bursts.replayEvery = 3
Bursts.restartEvery = 9
Bursts.ground = 330
-- A party cannon pops the bursts, two cottages smoke from their chimneys and a fountain sprays the one shot, each in its column.
Bursts.cannon = {x = -720, scale = 0.5, mouth = {0.82, 0.18}}
Bursts.houses = {-240, 240}
Bursts.houseScale = 0.42
Bursts.fountain = {x = 720, scale = 0.5}

function Bursts:init(entry)
    Bursts.super.init(self, entry)
    self.town = Town()
    local cannon = Bursts.cannon
    local mouthX = cannon.x + (cannon.mouth[1] - 0.5) * 365 * cannon.scale
    local mouthY = Bursts.ground - 390 * cannon.scale * (1 - cannon.mouth[2])
    self.scheduled = particles2d.newEmitter(ParticleTest.look('soft-dot', {rate = 0, bursts = {{time = 0, count = 40}, {time = 0.5, count = 12}, {time = 0.8, count = 12}, {time = 1.2, count = 60}}, duration = 2, loop = true, lifetime = {0.8, 1.2}, speed = {300, 520}, direction = -0.85, spread = 0.7, gravity = {0, 520}, damping = 0.6, startSize = {22, 30}, endSize = 4, colors = {'#FFFFF0A0', '#FFFFB040', '#00FF6020'}, blend = 'additive', layer = 5, seed = 40}))
    self.scheduled.position = {mouthX, mouthY}

    local plume = ParticleTest.look('smoke', {frameMode = 'random', rate = 8, lifetime = {5, 6}, speed = {60, 85}, spread = 0.25, gravity = {12, -8}, startSize = {40, 54}, endSize = {170, 220}, rotation = {0, 6.28}, spin = {-0.4, 0.4}, colors = {'#00B8C0CC', '#A0B8C0CC', '#60B0B8C4', '#00A8B0BC'}, colorTimes = {0, 0.1, 0.6, 1}, layer = 2})
    plume.prewarm, plume.seed = 6, 41
    self.warm = particles2d.newEmitter(plume)
    plume.prewarm, plume.seed = 0, 42
    self.cold = particles2d.newEmitter(plume)
    for index, emitter in ipairs({self.warm, self.cold}) do
        local x, y = self.town:chimney('cottage', Bursts.houses[index], Bursts.ground, Bursts.houseScale)
        emitter.position = {x, y + 6}
    end

    local fountain = Bursts.fountain
    self.once = particles2d.newEmitter(ParticleTest.look('droplet', {rate = 80, duration = 1.2, lifetime = {1.05, 1.25}, speed = {400, 460}, direction = -1.5708, spread = 0.28, gravity = {0, 720}, startSize = {14, 20}, endSize = 10, colors = {'#FFC8F0FF', '#FF8CD0FF', '#004090E0'}, colorTimes = {0, 0.8, 1}, layer = 3, seed = 43}))
    self.once.position = {fountain.x + (199 - 202) * fountain.scale, Bursts.ground + 20 - (318 - 70) * fountain.scale}
    self.emitters = {self.scheduled, self.warm, self.cold, self.once}
    self.sinceReplay, self.sinceRestart = 0, 0
end

function Bursts:enter()
    self:frame{
        hint = 'The cannon pops its bursts at their times, the fountain sprays its one shot again every 3 seconds and the chimneys start over every 9 seconds, or restart them yourself, to compare a prewarmed plume with a cold one.',
        code = Bursts.code,
        controls = {
            ui.button{id = 'plumes', text = 'Restart the plumes', onClick = function()
                self:restartPlumes()
            end},
            ui.button{id = 'once', text = 'Spray the fountain again', onClick = function()
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
    local once = self.once.emitting and 'sprays' or self.once.alive and 'lets its last drops fall' or 'waits'
    self:report('The burst cycle is at %.1f seconds, the chimneys started %.1f seconds ago, and the fountain %s.', self.scheduled.cycleTime, self.sinceRestart, once)
end

function Bursts:draw(area)
    ParticleTest.backdrop({0.05, 0.06, 0.12}, {0.14, 0.15, 0.24})
    ParticleTest.paveFloor('stone', Bursts.ground, 140, '#FF8A8C9C')
    ParticleTest.prop('cannon', Bursts.cannon.x, Bursts.ground + 6, Bursts.cannon.scale, {pivotY = 1, layer = 4})
    for _, x in ipairs(Bursts.houses) do
        self.town:house('cottage', x, Bursts.ground + 4, Bursts.houseScale, 3, 1, '#FFB0B4D0')
    end
    ParticleTest.prop('fountain', Bursts.fountain.x, Bursts.ground + 20, Bursts.fountain.scale, {pivotY = 1, layer = 2})
    for _, emitter in ipairs(self.emitters) do
        emitter:draw()
    end
    local captions = Bursts.ground + 24
    ParticleTest.label('Bursts', Bursts.cannon.x, captions, 'At 0, 0.5, 0.8 and 1.2 seconds')
    ParticleTest.label('Prewarmed', Bursts.houses[1], captions, 'prewarm = 6')
    ParticleTest.label('Cold', Bursts.houses[2], captions, 'No prewarm')
    ParticleTest.label('One shot', Bursts.fountain.x, captions, 'duration = 1.2')
end

return Bursts
