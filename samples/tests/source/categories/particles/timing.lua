-- Delays and burst cycles: `delay` holds an emitter back before its emission cycle starts, so the parts of an effect start in sequence, and a burst fires `cycles` times `interval` seconds apart, each time with the chance `probability` and a count picked in its range, which makes crackles, sputters and pulses without any Lua per frame.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local Timing = haylen.class('Timing', ParticleTest)

Timing.launch = {-600, 360}
Timing.pad = {-720, 360, 240, 30}
Timing.order = {layer = 1}
Timing.rest = 1.2

function Timing:init(entry)
    Timing.super.init(self, entry)
    self.gap = 0.5
    self.cycles = 12
    self.probability = 0.6
    self.quiet = 0
    local x, y = Timing.launch[1], Timing.launch[2]

    self.smoke = particles2d.newEmitter({texture = ParticleTest.library('smoke_puff'), rate = 30, duration = 2.4, lifetime = {1.6, 2.2}, speed = {80, 200}, direction = -1.5708, spread = 2.6, damping = 1.2, startSize = {50, 70}, endSize = {150, 200}, rotation = {0, m.tau}, spin = {-0.6, 0.6}, colors = {'#00C0C0C8', '#A0A0A0A8', '#00707078'}, maxParticles = 128, layer = 2, seed = 281})
    self.flame = particles2d.newEmitter({texture = ParticleTest.library('flame_soft'), rate = 70, delay = self.gap, duration = 1.6, lifetime = {0.4, 0.7}, speed = {500, 700}, direction = -1.5708, spread = 0.15, startSize = {60, 80}, endSize = 10, colors = {'#FFFFFFE0', '#FFFFB040', '#00FF4010'}, blend = 'additive', maxParticles = 128, layer = 3, seed = 282})
    self.sparks = particles2d.newEmitter({texture = ParticleTest.library('spark_streak'), rate = 0, bursts = {{time = 0, count = 40}, {time = 0.3, count = 30}, {time = 0.6, count = 20}}, delay = self.gap * 2, duration = 0.8, lifetime = {0.6, 1.1}, speed = {500, 900}, direction = -1.5708, spread = 1.2, gravity = {0, 1200}, startSize = 14, endSize = 6, alignToVelocity = true, stretch = 0.02, colors = {'#FFFFFFE0', '#FFFFC050', '#00FF6020'}, blend = 'additive', maxParticles = 256, layer = 4, seed = 283})
    self.parts = {self.smoke, self.flame, self.sparks}
    for _, emitter in ipairs(self.parts) do
        emitter.x, emitter.y = x, y
    end

    self.crackle = particles2d.newEmitter({texture = ParticleTest.library('star_4point'), rate = 0, bursts = self:crackleBursts(), duration = 1.6, loop = true, lifetime = {0.2, 0.5}, speed = {60, 260}, spread = m.tau, startSize = {14, 28}, endSize = 0, rotation = {0, m.tau}, colors = {'#FFFFFFFF', '#FFFFE080', '#00FFA040'}, blend = 'additive', maxParticles = 256, layer = 3, seed = 284})
    self.sputter = particles2d.newEmitter({texture = ParticleTest.library('fireball_sheet'), frameGrid = {columns = 4, rows = 1}, frameMode = 'random', rate = 0, bursts = self:sputterBursts(), duration = 1.4, loop = true, lifetime = {0.3, 0.6}, speed = {300, 520}, direction = 0, spread = 0.35, damping = 2, startSize = {30, 46}, endSize = {60, 80}, colors = {'#FFFFF0B0', '#FFFF8020', '#00602000'}, blend = 'additive', maxParticles = 256, layer = 3, seed = 285})
    self.pulse = particles2d.newEmitter({texture = ParticleTest.library('droplet'), rate = 0, bursts = {{time = 0, count = {18, 26}, cycles = 3, interval = 0.25}}, duration = 1.5, loop = true, lifetime = {1, 1.3}, speed = {500, 680}, direction = -1.5708, spread = 0.4, gravity = {0, 1100}, startSize = {14, 20}, endSize = 10, colors = {'#FFA0D8FF', '#0060A0FF'}, maxParticles = 256, layer = 3, seed = 286})
    self.crackle.x, self.crackle.y = 100, -120
    self.sputter.x, self.sputter.y = 380, 60
    self.pulse.x, self.pulse.y = 720, 360
    self.emitters = {self.smoke, self.flame, self.sparks, self.crackle, self.sputter, self.pulse}
    self:describeDelays()
end

function Timing:crackleBursts()
    return {{time = 0, count = {4, 10}, cycles = self.cycles, interval = 0.07, probability = self.probability}}
end

-- A sputter coughs a few small puffs at random and then one large one.
function Timing:sputterBursts()
    return {{time = 0, count = {2, 5}, cycles = 6, interval = 0.12, probability = self.probability}, {time = 0.9, count = {12, 18}}}
end

-- Starts the flame and the sparks one and two gaps after the smoke from the next launch on, since a delay is armed when an emitter restarts.
function Timing:space()
    self.flame:configure({delay = self.gap})
    self.sparks:configure({delay = self.gap * 2})
    self:describeDelays()
end

function Timing:describeDelays()
    self.captions = {'Smoke at 0 s', string.format('Flame at %.2f s', self.gap), string.format('Sparks at %.2f s', self.gap * 2)}
end

function Timing:fire()
    self.quiet = 0
    for _, emitter in ipairs(self.emitters) do
        emitter:restart()
    end
end

function Timing:enter()
    self:frame{
        hint = 'A click, a tap, E, Enter, the south button or the button of the panel fires everything again. The launch on the left also fires again a moment after its last part ends, with the delays of the slider.',
        cursor = true,
        controls = {
            ui.button{id = 'fire', text = 'Fire again', onClick = function()
                self:fire()
            end},
            ui.formField{label = 'Delay between the parts of the launch', ui.slider{id = 'gap', min = 0, max = 1.2, value = self.gap, showValue = true, onChange = function(event)
                self.gap = event.value
                self:space()
            end}},
            ui.formField{label = 'Cycles of the crackle', ui.slider{id = 'cycles', min = 1, max = 20, step = 1, value = self.cycles, showValue = true, decimals = 0, onChange = function(event)
                self.cycles = math.floor(event.value)
                self.crackle:configure({bursts = self:crackleBursts()})
            end}},
            ui.formField{label = 'Probability of each crackle and sputter', ui.slider{id = 'probability', min = 0, max = 1, value = self.probability, showValue = true, onChange = function(event)
                self.probability = event.value
                self.crackle:configure({bursts = self:crackleBursts()})
                self.sputter:configure({bursts = self:sputterBursts()})
            end}},
        },
    }
end

function Timing:update(dt)
    Timing.super.update(self, dt)
    if self:pressed() then
        self:fire()
    end
    local count = ParticleTest.updateAll(self.emitters, dt)
    if not (self.smoke.alive or self.flame.alive or self.sparks.alive) then
        self.quiet = self.quiet + dt
        if self.quiet > Timing.rest then
            self:fire()
        end
    end
    self:report('Delays 0, %.2f and %.2f s   Crackle of %d cycles   Probability %.2f   Crackle cycle %.2f s   Particles %d', self.gap, self.gap * 2, self.cycles, self.probability, self.crackle.cycleTime, count)
end

function Timing:draw(area)
    ParticleTest.backdrop({0.03, 0.03, 0.08}, {0.1, 0.07, 0.12})
    graphics2d.drawRect(Timing.pad, '#FF3A3A48', Timing.order)
    ParticleTest.drawAll(self.emitters)
    for index, caption in ipairs(self.captions) do
        ParticleTest.label(caption, Timing.launch[1], 380 + (index - 1) * 40)
    end
    ParticleTest.label('Crackle', self.crackle.x, self.crackle.y + 160)
    ParticleTest.label('Sputter', self.sputter.x + 200, self.sputter.y + 90)
    ParticleTest.label('Pulses', self.pulse.x, self.pulse.y + 20)
end

return Timing
