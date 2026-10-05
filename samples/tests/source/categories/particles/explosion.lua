-- Explosion: one-shot emitters with a burst at time 0 and no rate, created per blast and dropped once they are no longer alive.
local haylen = require('haylen')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')

local ParticleTest = require('categories.particles.particle-test')

local Explosion = haylen.class('Explosion', ParticleTest)

function Explosion:init(entry)
    Explosion.super.init(self, entry)
    self.random = m.random(5)
    self.soft, self.spark, self.ring = ParticleTest.texture('soft'), ParticleTest.texture('spark'), ParticleTest.texture('ring')
    self.smoke = ParticleTest.texture('smoke')
    self.blasts = {}
    self.quiet = 0
end

function Explosion:enter()
    self:frame{hint = 'A click, a tap, E, Enter or the south button makes a blast at the cursor. Without input a blast goes off every two seconds.', cursor = true}
end

-- Returns an emitter that bursts once at a point and stops.
function Explosion.once(options, x, y, seed)
    options.rate, options.duration, options.seed = 0, 0.1, seed
    local emitter = particles2d.newEmitter(options)
    emitter.position = {x, y}
    return emitter
end

-- Adds the parts of one blast: a flash, a shock ring, fireballs, debris and smoke.
function Explosion:blast(x, y)
    local seed = self.random:integer(1, 100000)
    local once = Explosion.once
    self.blasts[#self.blasts + 1] = {
        once({texture = self.soft, bursts = {{time = 0, count = 1}}, lifetime = 0.25, speed = 0, startSize = 420, endSize = 600, colors = {'#FFFFFFF0', '#00FFC060'}, blend = 'additive', layer = 5}, x, y, seed),
        once({texture = self.ring, bursts = {{time = 0, count = 1}}, lifetime = 0.5, speed = 0, startSize = 60, endSize = 700, colors = {'#C0FFE0A0', '#00FF8040'}, blend = 'additive', layer = 4}, x, y, seed),
        once({texture = self.soft, bursts = {{time = 0, count = 36}}, lifetime = {0.4, 0.9}, speed = {150, 420}, spread = m.tau, damping = 3, startSize = {80, 130}, endSize = {10, 30}, colors = {'#FFFFF0B0', '#FFFF9020', '#A0C02000', '#00200000'}, blend = 'additive', layer = 3}, x, y, seed),
        once({texture = self.spark, bursts = {{time = 0, count = 60}}, lifetime = {0.8, 1.6}, speed = {300, 800}, spread = m.tau, gravity = {0, 700}, damping = 0.8, startSize = {6, 12}, endSize = 2, colors = {'#FFFFF0A0', '#FFFF6010', '#00801000'}, blend = 'additive', layer = 6}, x, y, seed),
        once({texture = self.smoke, frames = ParticleTest.frames(self.smoke), bursts = {{time = 0.05, count = 18}}, lifetime = {1.5, 2.5}, speed = {40, 140}, spread = m.tau, damping = 1.5, gravity = {0, -40}, startSize = {80, 120}, endSize = {200, 280}, spin = {-1, 1}, colors = {'#00303030', '#B0303030', '#00202020'}, layer = 2}, x, y, seed),
    }
end

function Explosion:update(dt)
    Explosion.super.update(self, dt)
    self.quiet = self.quiet + dt
    if self:pressed() then
        self:blast(self.cursorX, self.cursorY)
        self.quiet = 0
    elseif self.quiet > 2 then
        self:blast(self.random:range(-600, 400), self.random:range(-200, 250))
        self.quiet = 0
    end

    local count, alive = 0, {}
    for _, blast in ipairs(self.blasts) do
        local live = false
        for _, emitter in ipairs(blast) do
            emitter:update(dt)
            count = count + emitter.count
            live = live or emitter.alive
        end
        if live then
            alive[#alive + 1] = blast
        end
    end
    self.blasts = alive
    self:status(string.format('Blasts alive %d   Particles %d', #self.blasts, count))
end

function Explosion:draw(area)
    ParticleTest.backdrop({0.05, 0.05, 0.1}, {0.18, 0.12, 0.1})
    for _, blast in ipairs(self.blasts) do
        for _, emitter in ipairs(blast) do
            emitter:draw()
        end
    end
end

return Explosion
