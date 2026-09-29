-- Fireworks: rockets are Lua objects with a trail emitter each, and at the top of their flight they burst into a ring of stars and a crackle of sparkles.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')

local art = require('art')
local sample = require('sample')

local Fireworks = haylen.class('Fireworks', sample.Test)

Fireworks.hints = 'Click, tap, E or the X button launches a rocket toward the cursor. Rockets also launch on their own.'

Fireworks.gravity = 620

function Fireworks:init(entry)
    Fireworks.super.init(self, entry)
    self.camera = graphics2d.newCamera()
    self.cursor = sample.Cursor()
    self.random = m.random(33)
    self.soft, self.spark, self.sparkle = art.texture('soft'), art.texture('spark'), art.texture('sparkle')
    self.rockets, self.effects = {}, {}
    self.clock = 0
end

function Fireworks:launch(targetX)
    local x = self.random:range(-500, 500)
    local trail = particles2d.newEmitter({texture = self.spark, rate = 90, lifetime = {0.4, 0.7}, speed = {0, 40}, spread = 6.2832, gravity = {0, 120}, startSize = {8, 12}, endSize = 0, colors = {'#FFFFE0A0', '#FFFF9040', '#00802000'}, blend = 'additive', layer = 2, seed = self.random:integer(1, 100000)})
    local height = self.random:range(900, 1150)
    self.rockets[#self.rockets + 1] = {x = x, y = 540, vx = (targetX - x) * 0.45, vy = -height, trail = trail}
    self.effects[#self.effects + 1] = trail
end

function Fireworks:burst(x, y)
    local hue = self.random:nextFloat()
    local color, tint = m.fromHsv(hue, 0.7, 1), m.fromHsv(hue + 0.1, 0.9, 1)
    local seed = self.random:integer(1, 100000)
    local stars = particles2d.newEmitter({texture = self.soft, rate = 0, bursts = {{time = 0, count = 110}}, duration = 0.1, lifetime = {1.2, 1.8}, speed = {260, 420}, spread = m.tau, gravity = {0, 160}, damping = 1.3, startSize = {18, 26}, endSize = 2, colors = {'#FFFFFFFF', color, tint:withAlpha(0)}, blend = 'additive', maxParticles = 128, layer = 3, seed = seed})
    local crackle = particles2d.newEmitter({texture = self.sparkle, frames = art.frames(self.sparkle), rate = 0, bursts = {{time = 0.5, count = 40}}, duration = 0.6, lifetime = {0.4, 0.8}, speed = {80, 300}, spread = m.tau, gravity = {0, 200}, startSize = {18, 28}, endSize = {18, 28}, colors = {'#FFFFFFFF', tint}, blend = 'additive', layer = 4, seed = seed + 1})
    for _, emitter in ipairs({stars, crackle}) do
        emitter.position = {x, y}
        self.effects[#self.effects + 1] = emitter
    end
end

function Fireworks:update(dt)
    self.cursor:update(dt)
    self.clock = self.clock + dt
    if sample.pressed() then
        self:launch((self.cursor:world(self.camera)))
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
    self:setStatus(string.format('%d rockets, %d emitters, %d particles', #self.rockets, #self.effects, count))
end

function Fireworks:render()
    graphics2d.beginWorld(self.camera)
    art.backdrop({0.01, 0.01, 0.05}, {0.05, 0.05, 0.14})
    graphics2d.drawRect({-1600, 500, 3200, 400}, '#FF06080E', {layer = 5})
    for _, emitter in ipairs(self.effects) do
        emitter:draw()
    end
end

function Fireworks:renderUi()
    graphics2d.beginScreen()
    self.cursor:draw()
end

return Fireworks
