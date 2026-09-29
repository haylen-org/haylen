-- Confetti: every color is its own emitter because colors change over the lifetime, not per particle, and the frames of a flipping strip repeat so each piece tumbles while it falls.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')

local art = require('art')
local sample = require('sample')

local Confetti = haylen.class('Confetti', sample.Test)

Confetti.hints = 'Click, tap, E or the X button fires both cannons. They also fire every three seconds.'

Confetti.colors = {'#FFFF4060', '#FFFFD030', '#FF40D080', '#FF40A0FF', '#FFC060FF'}

function Confetti:init(entry)
    Confetti.super.init(self, entry)
    self.camera = graphics2d.newCamera()
    self.texture = art.texture('confetti')
    self.frames = {}
    for _ = 1, 8 do
        table.move(art.frames(self.texture), 1, 4, #self.frames + 1, self.frames)
    end
    self.bursts = {}
    self.random = m.random(21)
    self.clock = 3
end

-- Fires one cannon: a burst of every color from a cone that aims up and inward.
function Confetti:fire(x, y, direction)
    for index, color in ipairs(Confetti.colors) do
        local emitter = particles2d.newEmitter({texture = self.texture, frames = self.frames, rate = 0, bursts = {{time = 0, count = 28}}, duration = 0.1, lifetime = {3, 4.5}, speed = {700, 1200}, direction = direction, spread = 0.5, gravity = {0, 520}, damping = 1.6, startSize = {18, 26}, endSize = {18, 26}, spin = {-6, 6}, colors = {color, color, m.color(color):withAlpha(0)}, shape = 'cone', shapeSize = {20, 0}, maxParticles = 64, layer = 2, seed = self.random:integer(1, 100000) + index})
        emitter.position = {x, y}
        self.bursts[#self.bursts + 1] = emitter
    end
end

function Confetti:update(dt)
    self.clock = self.clock + dt
    if sample.pressed() or self.clock > 3 then
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
    self:setStatus(string.format('%d pieces of confetti from %d emitters', count, #self.bursts))
end

function Confetti:render()
    graphics2d.beginWorld(self.camera)
    art.backdrop({0.1, 0.08, 0.2}, {0.25, 0.12, 0.3})
    for _, x in ipairs({-860, 860}) do
        graphics2d.drawCircle(x, 480, 60, '#FF505068', {layer = 3})
    end
    for _, emitter in ipairs(self.bursts) do
        emitter:draw()
    end
end

return Confetti
