-- Trails: an emitter that moves leaves its particles where they were born, because particles live in world space, so a high rate and no speed draw a trail.
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')

local ParticleTest = require('categories.particles.particle-test')

local Trail = haylen.class('Trail', ParticleTest)

function Trail:init(entry)
    Trail.super.init(self, entry)
    local soft = ParticleTest.texture('soft')
    self.pen = particles2d.newEmitter({texture = soft, rate = 240, lifetime = {0.7, 0.9}, speed = {0, 20}, spread = 6.2832, startSize = {34, 40}, endSize = 0, colors = {'#FFA0F0FF', '#FF6080FF', '#00A040FF'}, blend = 'additive', maxParticles = 512, layer = 2, seed = 10})
    self.comet = particles2d.newEmitter({texture = soft, rate = 200, lifetime = {1, 1.4}, speed = {0, 30}, spread = 6.2832, startSize = {60, 70}, endSize = 4, colors = {'#FFFFF0C0', '#FFFF9030', '#00FF2000'}, blend = 'additive', maxParticles = 512, layer = 1, seed = 11})
    self.dust = particles2d.newEmitter({texture = ParticleTest.texture('spark'), rate = 60, lifetime = {1, 2}, speed = {20, 80}, spread = 6.2832, gravity = {0, 40}, startSize = {4, 7}, endSize = 0, colors = {'#FFFFE0A0', '#00FF8040'}, blend = 'additive', layer = 3, seed = 12})
    self.time = 0
end

function Trail:enter()
    self:frame{hint = 'Draw with the cursor: move it with the mouse, a finger, the arrows, WASD or a stick. The comet circles on its own.', cursor = true}
end

function Trail:update(dt)
    Trail.super.update(self, dt)
    self.time = self.time + dt
    self.pen.position = {self.cursorX, self.cursorY}
    local x, y = math.cos(self.time * 1.3) * 520, math.sin(self.time * 1.3) * 280 + 60
    self.comet.position = {x, y}
    self.dust.position = {x, y}
    for _, emitter in ipairs({self.pen, self.comet, self.dust}) do
        emitter:update(dt)
    end
    self:status(string.format('Particles in the trail of the cursor %d   In the comet %d', self.pen.count, self.comet.count + self.dust.count))
end

function Trail:draw(area)
    ParticleTest.backdrop({0.02, 0.02, 0.06}, {0.06, 0.04, 0.12})
    for _, emitter in ipairs({self.comet, self.pen, self.dust}) do
        emitter:draw()
    end
end

return Trail
