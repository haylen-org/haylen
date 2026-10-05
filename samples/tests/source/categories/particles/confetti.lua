-- Confetti: every color is its own emitter because colors change over the lifetime, not per particle, and the frames of a flipping strip repeat so each piece tumbles while it falls.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')

local ParticleTest = require('categories.particles.particle-test')

local Confetti = haylen.class('Confetti', ParticleTest)

Confetti.colors = {'#FFFF4060', '#FFFFD030', '#FF40D080', '#FF40A0FF', '#FFC060FF'}

function Confetti:init(entry)
    Confetti.super.init(self, entry)
    self.texture = ParticleTest.texture('confetti')
    self.strip = {}
    for _ = 1, 8 do
        table.move(ParticleTest.frames(self.texture), 1, 4, #self.strip + 1, self.strip)
    end
    self.bursts = {}
    self.random = m.random(21)
    self.clock = 3
end

function Confetti:enter()
    self:frame{hint = 'A click, a tap, E, Enter or the south button fires both cannons. They also fire every three seconds.', cursor = true}
end

-- Fires one cannon: a burst of every color from a cone that aims up and inward.
function Confetti:fire(x, y, direction)
    for index, color in ipairs(Confetti.colors) do
        local emitter = particles2d.newEmitter({texture = self.texture, frames = self.strip, rate = 0, bursts = {{time = 0, count = 28}}, duration = 0.1, lifetime = {3, 4.5}, speed = {700, 1200}, direction = direction, spread = 0.5, gravity = {0, 520}, damping = 1.6, startSize = {18, 26}, endSize = {18, 26}, spin = {-6, 6}, colors = {color, color, m.color(color):withAlpha(0)}, shape = 'cone', shapeSize = {20, 0}, maxParticles = 64, layer = 2, seed = self.random:integer(1, 100000) + index})
        emitter.position = {x, y}
        self.bursts[#self.bursts + 1] = emitter
    end
end

function Confetti:update(dt)
    Confetti.super.update(self, dt)
    self.clock = self.clock + dt
    if self:pressed() or self.clock > 3 then
        self:fire(-860, 480, -1.1)
        self:fire(860, 480, -2.04)
        self.clock = 0
    end

    local alive, count = {}, 0
    for _, emitter in ipairs(self.bursts) do
        emitter:update(dt)
        count = count + emitter.count
        if emitter.alive then
            alive[#alive + 1] = emitter
        end
    end
    self.bursts = alive
    self:status(string.format('Pieces of confetti %d   Emitters %d', count, #self.bursts))
end

function Confetti:draw(area)
    ParticleTest.backdrop({0.1, 0.08, 0.2}, {0.25, 0.12, 0.3})
    for _, x in ipairs({-860, 860}) do
        graphics2d.drawCircle(x, 480, 60, '#FF505068', {layer = 3})
    end
    for _, emitter in ipairs(self.bursts) do
        emitter:draw()
    end
end

return Confetti
