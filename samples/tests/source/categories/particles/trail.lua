-- Trails: an emitter that moves leaves its particles where they were born, because particles live in world space, so a high rate and no speed draw a trail.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')

local ParticleTest = require('categories.particles.particle-test')

local Trail = haylen.class('Trail', ParticleTest)

Trail.code = [[
local pen = particles2d.newEmitter({texture = dot, rate = 240, lifetime = {0.7, 0.9}})
pen:configure({speed = {0, 20}, spread = 6.2832, startSize = {18, 24}, endSize = 0})
pen:configure({blend = 'additive', maxParticles = 512})
pen.position = {cursorX, cursorY}  -- Particles stay where they were born.
pen:update(dt)  -- So a moving emitter draws a trail.]]

Trail.starColor = {0.85, 0.88, 1, 1}
Trail.starOrder = {layer = -9}

function Trail:init(entry)
    Trail.super.init(self, entry)
    local soft = 'soft-dot'
    self.pen = particles2d.newEmitter(ParticleTest.look(soft, {rate = 240, lifetime = {0.7, 0.9}, speed = {0, 20}, spread = 6.2832, startSize = {18, 24}, endSize = 0, colors = {'#C0B0F4FF', '#A06080FF', '#00A040FF'}, blend = 'additive', maxParticles = 512, layer = 2, seed = 10}))
    self.comet = particles2d.newEmitter(ParticleTest.look(soft, {rate = 120, lifetime = {0.9, 1.2}, speed = {0, 30}, spread = 6.2832, startSize = {34, 44}, endSize = 4, colors = {'#90FFF0C0', '#60FF9030', '#00FF2000'}, blend = 'additive', maxParticles = 512, layer = 1, seed = 11}))
    self.tail = particles2d.newTrail({lifetime = 0.6, minDistance = 6, maxPoints = 64, widthStart = 46, widthEnd = 0, colors = {'#E0FFF4D0', '#A0FFA040', '#00FF4000'}, texture = ParticleTest.library('glow-line'), blend = 'additive', layer = 1})
    self.head = particles2d.newEmitter(ParticleTest.look(soft, {rate = 30, lifetime = 0.2, speed = 0, startSize = {70, 80}, endSize = 60, colors = {'#C0FFF8E0', '#00FFC060'}, blend = 'additive', layer = 3, seed = 16}))
    self.dust = particles2d.newEmitter(ParticleTest.look('hard-dot', {rate = 60, lifetime = {1, 2}, speed = {20, 80}, spread = 6.2832, gravity = {0, 40}, startSize = {4, 7}, endSize = 0, colors = {'#FFFFE0A0', '#00FF8040'}, blend = 'additive', layer = 3, seed = 12}))
    self.time = 0
    self.idle, self.orbit = 0, 0
    -- A night sky of stars that twinkle at their own pace, spread once over the world.
    local random = m.random(17)
    self.stars = {}
    for index = 1, 160 do
        self.stars[index] = {x = random:range(-1400, 1400), y = random:range(-560, 560), size = random:range(1.5, 4), phase = random:range(0, m.tau)}
    end
end

function Trail:enter()
    self:frame{hint = 'Draw with the cursor: move it with the mouse, a finger, the arrows, WASD or a stick. While the cursor rests, the pen circles around it. The comet circles on its own.', code = Trail.code, cursor = true}
end

function Trail:update(dt)
    Trail.super.update(self, dt)
    self.time = self.time + dt
    -- The pen sits on the cursor while it moves, and circles around it once the cursor rests, so the trail always shows.
    local moved = self.cursorX ~= self.lastX or self.cursorY ~= self.lastY
    self.lastX, self.lastY = self.cursorX, self.cursorY
    self.idle = moved and 0 or self.idle + dt
    self.orbit = m.lerp(self.orbit, self.idle > 0.6 and 110 or 0, math.min(1, dt * 4))
    self.pen.position = {self.cursorX + math.cos(self.time * 4) * self.orbit, self.cursorY + math.sin(self.time * 4) * self.orbit}
    local x, y = math.cos(self.time * 1.3) * 520, math.sin(self.time * 1.3) * 280 + 60
    for _, emitter in ipairs({self.comet, self.dust, self.tail, self.head}) do
        emitter.position = {x, y}
    end
    for _, emitter in ipairs({self.pen, self.comet, self.dust, self.tail, self.head}) do
        emitter:update(dt)
    end
    self:report('The cursor trails %d particles, and the comet %d with a ribbon of %d points.', self.pen.count, self.comet.count + self.dust.count, self.tail.count)
end

function Trail:draw(area)
    ParticleTest.backdrop({0.02, 0.02, 0.06}, {0.08, 0.05, 0.15})
    local time, color = haylen.elapsed(), Trail.starColor
    for _, star in ipairs(self.stars) do
        color[4] = 0.45 + 0.35 * math.sin(time * 1.7 + star.phase)
        graphics2d.drawCircle(star.x, star.y, star.size, color, Trail.starOrder)
    end
    for _, emitter in ipairs({self.tail, self.comet, self.pen, self.dust, self.head}) do
        emitter:draw()
    end
end

return Trail
