-- Delays and burst cycles: `delay` holds an emitter back before its emission cycle starts, so the parts of an effect start in sequence, and a burst fires `cycles` times `interval` seconds apart, each time with the chance `probability` and a count picked in its range, which makes crackles, sputters and pulses without any Lua per frame.
local haylen = require('haylen')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local Look = require('categories.physics.look')
local ParticleTest = require('categories.particles.particle-test')

local Timing = haylen.class('Timing', ParticleTest)

Timing.code = [[
local flame = particles2d.newEmitter({texture = flameSoft, rate = 70, duration = 1.6})
flame:configure({delay = 0.5})  -- Starts half a second after the smoke.
local crackle = particles2d.newEmitter({texture = star, rate = 0, duration = 1.6, loop = true})
crackle:configure({bursts = {{time = 0, count = {4, 10}, cycles = 12, interval = 0.07}}})
crackle:configure({bursts = {{time = 0, count = 6, cycles = 12, probability = 0.6}}})
flame:restart()  -- Arms the delay again.]]

Timing.launch = {-720, 280}
Timing.floor = 310
Timing.captionY = 360
-- The times of the launch wrap within this width, so they never reach the caption of the crackle.
Timing.captionWidth = 420
Timing.rest = 1.2
-- The crackle sparks from a campfire, the sputter coughs out of a cannon and the pulses spout from a fountain, all standing on the floor.
Timing.campfire = {x = -240, scale = 0.42}
Timing.cannon = {x = 170, scale = 0.45, mouth = {0.82, 0.18}}
Timing.fountain = {x = 720, scale = 0.45}

function Timing:init(entry)
    Timing.super.init(self, entry)
    self.gap = 0.5
    self.cycles = 12
    self.probability = 0.6
    self.quiet = 0
    local x, y = Timing.launch[1], Timing.launch[2]

    self.smoke = particles2d.newEmitter(ParticleTest.look('smoke', {frameMode = 'random', rate = 30, duration = 2.4, lifetime = {1.6, 2.2}, speed = {80, 200}, direction = -1.5708, spread = 2.6, damping = 1.2, startSize = {50, 70}, endSize = {150, 200}, rotation = {0, m.tau}, spin = {-0.6, 0.6}, colors = {'#00C0C0C8', '#A0A0A0A8', '#00707078'}, maxParticles = 128, layer = 2, seed = 281}))
    self.flame = particles2d.newEmitter(ParticleTest.look('flame-soft', {rate = 70, delay = self.gap, duration = 1.6, lifetime = {0.4, 0.7}, speed = {500, 700}, direction = -1.5708, spread = 0.15, startSize = {60, 80}, endSize = 10, colors = {'#FFFFFFE0', '#FFFFB040', '#00FF4010'}, blend = 'additive', maxParticles = 128, layer = 3, seed = 282}))
    self.sparks = particles2d.newEmitter(ParticleTest.look('spark-streak', {rate = 0, bursts = {{time = 0, count = 40}, {time = 0.3, count = 30}, {time = 0.6, count = 20}}, delay = self.gap * 2, duration = 0.8, lifetime = {0.6, 1.1}, speed = {500, 900}, direction = -1.5708, spread = 1.2, gravity = {0, 1200}, startSize = 14, endSize = 6, alignToVelocity = true, stretch = 0.02, colors = {'#FFFFFFE0', '#FFFFC050', '#00FF6020'}, blend = 'additive', maxParticles = 256, layer = 4, seed = 283}))
    self.parts = {self.smoke, self.flame, self.sparks}
    for _, emitter in ipairs(self.parts) do
        emitter.x, emitter.y = x, y
    end

    self.crackle = particles2d.newEmitter(ParticleTest.look('star-4point', {rate = 0, bursts = self:crackleBursts(), duration = 1, loop = true, lifetime = {0.2, 0.5}, speed = {60, 260}, spread = m.tau, startSize = {14, 28}, endSize = 0, rotation = {0, m.tau}, colors = {'#FFFFFFFF', '#FFFFE080', '#00FFA040'}, blend = 'additive', maxParticles = 256, layer = 3, seed = 284}))
    self.campfire = particles2d.newEmitter(ParticleTest.look('flame', {frameMode = 'loopRandomStart', frameRate = 14, rate = 26, lifetime = {0.5, 0.8}, speed = {90, 150}, direction = -1.5708, spread = 0.2, startSize = {90, 120}, endSize = {20, 30}, sizeCurve = 'quadIn', colors = {'#00FFD890', '#B0FFB040', '#90FF701C', '#00601008'}, colorTimes = {0, 0.15, 0.6, 1}, shape = 'ellipse', shapeSize = {30, 8}, blend = 'additive', layer = 2, seed = 287}))
    self.sputter = particles2d.newEmitter(ParticleTest.look('fireball', {frameMode = 'random', rate = 0, bursts = self:sputterBursts(), duration = 1.4, loop = true, lifetime = {0.3, 0.6}, speed = {300, 520}, direction = -0.85, spread = 0.35, damping = 2, startSize = {30, 46}, endSize = {60, 80}, colors = {'#FFFFF0B0', '#FFFF8020', '#00602000'}, blend = 'additive', maxParticles = 256, layer = 3, seed = 285}))
    self.pulse = particles2d.newEmitter(ParticleTest.look('droplet', {rate = 0, bursts = {{time = 0, count = {18, 26}, cycles = 3, interval = 0.25}}, duration = 1.5, loop = true, lifetime = {0.9, 1.1}, speed = {460, 600}, direction = -1.5708, spread = 0.35, gravity = {0, 1100}, startSize = {18, 26}, endSize = 14, colors = {'#FFA0D8FF', '#0060A0FF'}, maxParticles = 256, layer = 3, seed = 286}))
    local campfire, cannon, fountain = Timing.campfire, Timing.cannon, Timing.fountain
    self.crackle.x, self.crackle.y = campfire.x, Timing.floor - 50
    self.campfire.x, self.campfire.y = campfire.x, Timing.floor - 24
    self.sputter.x = cannon.x + (cannon.mouth[1] - 0.5) * 365 * cannon.scale
    self.sputter.y = Timing.floor + 6 - 390 * cannon.scale * (1 - cannon.mouth[2])
    self.pulse.x, self.pulse.y = fountain.x + (199 - 202) * fountain.scale, Timing.floor + 20 - (318 - 70) * fountain.scale
    self.emitters = {self.smoke, self.flame, self.sparks, self.campfire, self.crackle, self.sputter, self.pulse}
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
    self.launchTimes = string.format('Smoke at 0, flame at %.2f, sparks at %.2f s', self.gap, self.gap * 2)
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
        code = Timing.code,
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
    self:report('The parts start %.2f seconds apart, the crackle fires %d times with a chance of %.2f, and %d particles fly.', self.gap, self.cycles, self.probability, count)
end

function Timing:draw(area)
    ParticleTest.backdrop({0.03, 0.03, 0.08}, {0.1, 0.07, 0.12})
    ParticleTest.paveFloor('stone', Timing.floor, 140, '#FF6A6478')
    -- The launch rises out of an iron tube in the floor.
    Look.block(Timing.launch[1], Timing.floor - 22, 76, 52, 'iron', 5)
    Look.block(Timing.launch[1], Timing.floor - 46, 96, 18, 'steel', 5)
    ParticleTest.prop('campfire', Timing.campfire.x, Timing.floor + 12, Timing.campfire.scale, {pivotY = 1, layer = 1})
    ParticleTest.prop('cannon', Timing.cannon.x, Timing.floor + 6, Timing.cannon.scale, {pivotY = 1, layer = 4})
    ParticleTest.prop('fountain', Timing.fountain.x, Timing.floor + 20, Timing.fountain.scale, {pivotY = 1, layer = 2})
    ParticleTest.drawAll(self.emitters)
    ParticleTest.label('Launch in sequence', Timing.launch[1], Timing.captionY, self.launchTimes, Timing.captionWidth)
    ParticleTest.label('Crackle', self.crackle.x, Timing.captionY, 'Cycles and a probability')
    ParticleTest.label('Sputter', Timing.cannon.x, Timing.captionY, 'Small coughs, then a puff')
    ParticleTest.label('Pulses', Timing.fountain.x, Timing.captionY, 'Three bursts a cycle')
end

return Timing
