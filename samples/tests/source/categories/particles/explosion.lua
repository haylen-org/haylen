-- Explosion: one-shot emitters with a burst at time 0 and no rate, created per blast and dropped once they are no longer alive.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')

local ParticleTest = require('categories.particles.particle-test')

local Explosion = haylen.class('Explosion', ParticleTest)

Explosion.floor = 380
Explosion.interval = 1.2

Explosion.code = [[
local once = {rate = 0, duration = 0.1}  -- A burst at time 0 and no rate.
local flash = particles2d.newEmitter({texture = dot, bursts = {{time = 0, count = 1}}, startSize = 420})
local debris = particles2d.newEmitter({texture = rocks, bursts = {{time = 0, count = 24}}, gravity = {0, 1400}})
debris:configure({collision = {type = 'floor', y = floor - y, bounce = 0.35}})  -- Rocks rest on their edge.
flash.position, debris.position = {x, y}, {x, y}
if not debris.alive then table.remove(blasts, index) end  -- Drops it once its last particle is gone.]]

function Explosion:init(entry)
    Explosion.super.init(self, entry)
    self.random = m.random(5)
    self.soft, self.spark, self.ring = 'soft-dot', 'spark-streak', 'ring-thin'
    self.smoke = 'smoke'
    self.blasts = {}
    self.scorches = {}
    self.quiet = 0
end

function Explosion:enter()
    self:frame{hint = 'A click, a tap, E, Enter or the south button makes a blast at the cursor. Without input a blast goes off near the middle every 1.2 seconds.', code = Explosion.code, cursor = true}
end

-- Returns an emitter that bursts once at a point and stops.
function Explosion.once(options, x, y, seed)
    options.rate, options.duration, options.seed = 0, 0.1, seed
    local emitter = particles2d.newEmitter(options)
    emitter.position = {x, y}
    return emitter
end

-- Adds the parts of one blast: a flash, a shock ring, fireballs, sparks, rocks that bounce on the floor, smoke and a scorch mark where it touches the floor.
function Explosion:blast(x, y)
    local seed = self.random:integer(1, 100000)
    local once = Explosion.once
    local floor = Explosion.floor - y
    self.blasts[#self.blasts + 1] = {
        once(ParticleTest.look(self.soft, {bursts = {{time = 0, count = 1}}, lifetime = 0.25, speed = 0, startSize = 420, endSize = 600, colors = {'#FFFFFFF0', '#00FFC060'}, blend = 'additive', layer = 5}), x, y, seed),
        once(ParticleTest.look(self.ring, {bursts = {{time = 0, count = 1}}, lifetime = 0.5, speed = 0, startSize = 60, endSize = 700, colors = {'#C0FFE0A0', '#00FF8040'}, blend = 'additive', layer = 4}), x, y, seed),
        once(ParticleTest.look('fireball', {bursts = {{time = 0, count = 14}}, frameMode = 'random', lifetime = {0.5, 0.9}, speed = {120, 360}, spread = m.tau, damping = 3, startSize = {110, 160}, endSize = {40, 70}, spin = {-2, 2}, colors = {'#FFFFF0B0', '#E0FFA030', '#90E04010', '#00401000'}, blend = 'additive', layer = 3}), x, y, seed),
        once(ParticleTest.look(self.spark, {bursts = {{time = 0, count = 50}}, lifetime = {0.5, 1.1}, speed = {500, 1100}, spread = m.tau, gravity = {0, 900}, damping = 1, startSize = {14, 22}, endSize = 4, alignToVelocity = true, stretch = 0.03, colors = {'#FFFFF0A0', '#FFFF9030', '#00A02000'}, collision = {type = 'floor', y = floor, bounce = 0.3, sizeScale = 0}, blend = 'additive', layer = 6}), x, y, seed),
        once(ParticleTest.look('debris', {bursts = {{time = 0, count = 22}}, frameMode = 'random', lifetime = {1.6, 2.4}, speed = {300, 800}, direction = -1.5708, spread = 1.4, gravity = {0, 1600}, startSize = {18, 34}, endSize = {18, 34}, spin = {-8, 8}, colors = {'#FFFFFFFF', '#FFFFFFFF', '#00FFFFFF'}, colorTimes = {0, 0.85, 1}, collision = {type = 'floor', y = floor, bounce = 0.35, friction = 0.4}, layer = 4}), x, y, seed),
        once(ParticleTest.look(self.smoke, {bursts = {{time = 0.05, count = 18}}, lifetime = {1.6, 2.6}, speed = {60, 180}, spread = m.tau, damping = 1.5, gravity = {0, -50}, startSize = {120, 170}, endSize = {260, 340}, spin = {-1, 1}, colors = {'#00585058', '#C0484048', '#00303030'}, layer = 2}), x, y, seed),
    }
    if floor < 260 then
        self.scorches[#self.scorches + 1] = {x = x, age = 0}
    end
end

function Explosion:update(dt)
    Explosion.super.update(self, dt)
    self.quiet = self.quiet + dt
    if self:pressed() then
        self:blast(self.cursorX, self.cursorY)
        self.quiet = 0
    elseif self.quiet > Explosion.interval then
        self:blast(self.random:range(-420, 420), self.random:range(-120, 220))
        self.quiet = 0
    end
    for index = #self.scorches, 1, -1 do
        local scorch = self.scorches[index]
        scorch.age = scorch.age + dt
        if scorch.age > 6 then
            table.remove(self.scorches, index)
        end
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
    self:report('%d blasts still burn with %d particles.', #self.blasts, count)
end

function Explosion:draw(area)
    ParticleTest.backdrop({0.07, 0.07, 0.14}, {0.22, 0.14, 0.12})
    ParticleTest.paveFloor('dirt', Explosion.floor, 150, '#FFB0A090')
    for _, scorch in ipairs(self.scorches) do
        local fade = 1 - scorch.age / 6
        graphics2d.draw(graphics2d.lightTexture(), scorch.x, Explosion.floor + 20, {width = 360, height = 70, color = {0.08, 0.05, 0.04, 0.8 * fade}, layer = -3})
    end
    for _, blast in ipairs(self.blasts) do
        for _, emitter in ipairs(blast) do
            emitter:draw()
        end
    end
end

return Explosion
