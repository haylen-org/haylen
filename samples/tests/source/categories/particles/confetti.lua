-- Confetti: every color is its own emitter because colors change over the lifetime, not per particle, and the frames of a flipping strip repeat so each piece tumbles while it falls from two party cannons onto the floor, where it rests.
local haylen = require('haylen')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')

local ParticleTest = require('categories.particles.particle-test')

local Confetti = haylen.class('Confetti', ParticleTest)

Confetti.code = [[
for _, color in ipairs({'#FFFF4060', '#FFFFD030', '#FF40D080'}) do  -- One emitter for each color.
    local piece = particles2d.newEmitter({texture = atlas.texture, frames = strip, rate = 0, duration = 0.1})
    piece:configure({bursts = {{time = 0, count = 28}}, speed = {700, 1200}, direction = -0.8})
    piece:configure({colors = {color, color, m.color(color):withAlpha(0)}, spin = {-6, 6}})
    piece:configure({collision = {type = 'floor', y = floor - y, bounce = 0.2, friction = 0.6}})
end]]

Confetti.colors = {'#FFFF4060', '#FFFFD030', '#FF40D080', '#FF40A0FF', '#FFC060FF'}

Confetti.ground = 380
Confetti.cannons = {{x = -760, direction = -0.85}, {x = 760, direction = -2.29, flip = true}}
-- The mouth of the cannon, as a share of its picture, which aims up and to the right.
Confetti.mouth = {0.82, 0.18}
Confetti.scale = 0.6
Confetti.interval = 1.6

function Confetti:init(entry)
    Confetti.super.init(self, entry)
    local confetti = ParticleTest.look('confetti', {})
    self.texture, self.strip = confetti.texture, {}
    for _ = 1, 8 do
        table.move(confetti.frames, 1, #confetti.frames, #self.strip + 1, self.strip)
    end
    self.bursts = {}
    self.random = m.random(21)
    self.clock = Confetti.interval
end

-- Returns where the confetti leaves a cannon that stands on the floor.
function Confetti.muzzle(cannon)
    local width, height = 365 * Confetti.scale, 390 * Confetti.scale
    local offset = (Confetti.mouth[1] - 0.5) * width
    return cannon.x + (cannon.flip and -offset or offset), Confetti.ground - height * (1 - Confetti.mouth[2])
end

function Confetti:enter()
    self:frame{hint = 'A click, a tap, E, Enter or the south button fires both cannons. They also fire on their own every 1.6 seconds.', code = Confetti.code, cursor = true}
end

-- Fires one cannon: a burst of every color from a cone that aims up and inward.
function Confetti:fire(x, y, direction)
    for index, color in ipairs(Confetti.colors) do
        local emitter = particles2d.newEmitter({texture = self.texture, frames = self.strip, rate = 0, bursts = {{time = 0, count = 24}}, duration = 0.1, lifetime = {3, 4.5}, speed = {800, 1300}, direction = direction, spread = 0.35, gravity = {0, 560}, damping = 1.5, startSize = {20, 28}, endSize = {20, 28}, spin = {-6, 6}, colors = {color, color, color, m.color(color):withAlpha(0)}, colorTimes = {0, 0.5, 0.85, 1}, collision = {type = 'floor', y = Confetti.ground - y, bounce = 0.2, friction = 0.6}, shape = 'cone', shapeSize = {20, 0}, maxParticles = 64, layer = 3, seed = self.random:integer(1, 100000) + index})
        emitter.position = {x, y}
        self.bursts[#self.bursts + 1] = emitter
    end
end

function Confetti:update(dt)
    Confetti.super.update(self, dt)
    self.clock = self.clock + dt
    if self:pressed() or self.clock > Confetti.interval then
        for _, cannon in ipairs(Confetti.cannons) do
            local x, y = Confetti.muzzle(cannon)
            self:fire(x, y, cannon.direction)
        end
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
    self:report('%d pieces of confetti fly or rest on the floor, from %d emitters.', count, #self.bursts)
end

function Confetti:draw(area)
    ParticleTest.backdrop({0.14, 0.1, 0.26}, {0.32, 0.16, 0.36})
    ParticleTest.paveFloor('sand', Confetti.ground, 140, '#FFC8B0D0')
    for _, cannon in ipairs(Confetti.cannons) do
        ParticleTest.prop('cannon', cannon.x, Confetti.ground + 6, Confetti.scale, {pivotY = 1, flipHorizontal = cannon.flip, layer = 4})
    end
    for _, emitter in ipairs(self.bursts) do
        emitter:draw()
    end
end

return Confetti
