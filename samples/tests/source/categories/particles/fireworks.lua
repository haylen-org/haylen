-- Fireworks: rockets are Lua objects with a trail emitter each, launched from behind a sleeping village so they burst inside the canvas, and at the top of their flight they burst into stars that draw short ribbons and a crackle of sparkles.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')

local ParticleTest = require('categories.particles.particle-test')
local Town = require('categories.lighting.town')

local Fireworks = haylen.class('Fireworks', ParticleTest)

Fireworks.code = [[
local apex = top + 360  -- High enough for the stars, inside the canvas.
rocket.vy = -math.sqrt(2 * gravity * (ground - apex))
local stars = particles2d.newEmitter({texture = dot, rate = 0, bursts = {{time = 0, count = 90}}, duration = 0.1})
stars:configure({speed = {240, 380}, spread = m.tau, gravity = {0, 160}, damping = 1.3})
stars:configure({trail = {length = 8, lifetime = 0.35, widthStart = 8}})  -- Each star draws a ribbon.
local crackle = particles2d.newEmitter({texture = sparkle, rate = 0, bursts = {{time = 0.5, count = 40}}})]]

Fireworks.gravity = 620
Fireworks.ground = 420
Fireworks.houses = {{name = 'cottage', x = -980}, {name = 'stone-house', x = -560}, {name = 'cottage', x = -120}, {name = 'stone-house', x = 330}, {name = 'cottage', x = 760}, {name = 'stone-house', x = 1160}}

function Fireworks:init(entry)
    Fireworks.super.init(self, entry)
    self.random = m.random(33)
    self.soft, self.spark, self.sparkle = 'soft-dot', 'hard-dot', 'sparkle'
    self.rockets, self.effects = {}, {}
    self.town = Town()
    self.glowLine = ParticleTest.library('glow-line')
    self.clock = 0
end

function Fireworks:enter()
    self:frame{hint = 'A click, a tap, E, Enter or the south button launches a rocket toward the cursor. Rockets also launch on their own.', code = Fireworks.code, cursor = true}
end

-- Launches a rocket toward `targetX` whose flight peaks at a random height that leaves room for its stars inside the canvas.
function Fireworks:launch(targetX)
    local area = self.camera:visibleBounds()
    local x = self.random:range(-500, 500)
    local trail = particles2d.newEmitter(ParticleTest.look(self.spark, {rate = 90, lifetime = {0.4, 0.7}, speed = {0, 40}, spread = 6.2832, gravity = {0, 120}, startSize = {8, 12}, endSize = 0, colors = {'#FFFFE0A0', '#FFFF9040', '#00802000'}, blend = 'additive', layer = 2, seed = self.random:integer(1, 100000)}))
    local apex = self.random:range(area.y + 360, area.y + area.height * 0.45)
    local rise = math.sqrt(2 * Fireworks.gravity * (Fireworks.ground - apex))
    self.rockets[#self.rockets + 1] = {x = x, y = Fireworks.ground, vx = (targetX - x) * 0.35, vy = -rise, trail = trail}
    self.effects[#self.effects + 1] = trail
end

function Fireworks:burst(x, y)
    local hue = self.random:nextFloat()
    local color, tint = m.fromHsv(hue, 0.7, 1), m.fromHsv(hue + 0.1, 0.9, 1)
    local seed = self.random:integer(1, 100000)
    local stars = particles2d.newEmitter(ParticleTest.look(self.soft, {rate = 0, bursts = {{time = 0, count = 90}}, duration = 0.1, lifetime = {1.2, 1.8}, speed = {240, 380}, spread = m.tau, gravity = {0, 160}, damping = 1.3, startSize = {16, 22}, endSize = 2, colors = {'#FFFFFFFF', color, tint:withAlpha(0)}, trail = {length = 8, lifetime = 0.35, widthStart = 8, widthEnd = 0, colors = {'#C0FFFFFF', '#00FFFFFF'}, texture = self.glowLine}, blend = 'additive', maxParticles = 128, layer = 3, seed = seed}))
    local crackle = particles2d.newEmitter(ParticleTest.look(self.sparkle, {rate = 0, bursts = {{time = 0.5, count = 40}}, duration = 0.6, lifetime = {0.4, 0.8}, speed = {80, 300}, spread = m.tau, gravity = {0, 200}, startSize = {18, 28}, endSize = {18, 28}, colors = {'#FFFFFFFF', tint}, blend = 'additive', layer = 4, seed = seed + 1}))
    for _, emitter in ipairs({stars, crackle}) do
        emitter.position = {x, y}
        self.effects[#self.effects + 1] = emitter
    end
end

function Fireworks:update(dt)
    Fireworks.super.update(self, dt)
    self.clock = self.clock + dt
    if self:pressed() then
        self:launch(self.cursorX)
    elseif self.clock > 0.9 then
        self:launch(self.random:range(-700, 700))
        self.clock = 0
    end

    local flying = {}
    for _, rocket in ipairs(self.rockets) do
        rocket.vy = rocket.vy + Fireworks.gravity * dt
        rocket.x, rocket.y = rocket.x + rocket.vx * dt, rocket.y + rocket.vy * dt
        rocket.trail.position = {rocket.x, rocket.y}
        if rocket.vy >= 0 then
            rocket.trail.emitting = false
            self:burst(rocket.x, rocket.y)
        else
            flying[#flying + 1] = rocket
        end
    end
    self.rockets = flying

    local alive, count = {}, 0
    for _, emitter in ipairs(self.effects) do
        emitter:update(dt)
        count = count + emitter.count
        if emitter.alive then
            alive[#alive + 1] = emitter
        end
    end
    self.effects = alive
    self:report('%d rockets climb, and %d emitters hold %d particles.', #self.rockets, #self.effects, count)
end

function Fireworks:draw(area)
    ParticleTest.backdrop({0.01, 0.01, 0.06}, {0.08, 0.07, 0.18})
    for _, house in ipairs(Fireworks.houses) do
        self.town:house(house.name, house.x, Fireworks.ground + 40, 0.55, 6, 1, '#FF3A4466')
    end
    local area = graphics2d.canvasBounds()
    graphics2d.drawRect({area.x, Fireworks.ground + 36, area.width, area:bottom() - Fireworks.ground}, '#FF0B0D1A', {layer = 8})
    for _, emitter in ipairs(self.effects) do
        emitter:draw()
    end
end

return Fireworks
