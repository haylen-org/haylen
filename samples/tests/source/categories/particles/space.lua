-- Local and world space: in world space live particles stay where they were born when the emitter moves, and with `localSpace` they move with the emitter, like the exhaust of a ship against a shield around it.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local Space = haylen.class('Space', ParticleTest)

Space.code = [[
local options = {texture = dot, rate = 120, speed = {40, 90}, spread = 6.2832}
options.localSpace = false  -- Particles stay where they were born.
local trail = particles2d.newEmitter(options)
options.localSpace = true  -- Particles move with the emitter.
local shield = particles2d.newEmitter(options)
shield.position = {x + math.cos(time) * 220, y + math.sin(time) * 220}]]

function Space:init(entry)
    Space.super.init(self, entry)
    self.speed = 2
    self.time = 0
    local options = ParticleTest.look('soft-dot', {rate = 120, lifetime = {1, 1.4}, speed = {40, 90}, spread = 6.2832, startSize = {26, 32}, endSize = 2, colors = {'#FFFFE080', '#FFFF7040', '#00A02060'}, blend = 'additive', maxParticles = 512, layer = 2})
    options.localSpace, options.seed = false, 50
    self.worldSpace = particles2d.newEmitter(options)
    options.localSpace, options.seed = true, 51
    self.localSpace = particles2d.newEmitter(options)
    self.orbits = {{emitter = self.worldSpace, x = -480, label = 'World space', option = 'localSpace = false'}, {emitter = self.localSpace, x = 480, label = 'Local space', option = 'localSpace = true'}}
end

function Space:enter()
    self:frame{
        hint = 'Both emitters are the same except "localSpace". Change how fast they circle.',
        code = Space.code,
        controls = {ui.formField{label = 'Circling speed', ui.slider{id = 'speed', min = 0, max = 6, value = self.speed, showValue = true, onChange = function(event)
            self.speed = event.value
        end}}},
        focus = 'speed',
    }
end

function Space:update(dt)
    Space.super.update(self, dt)
    self.time = self.time + dt * self.speed
    for _, orbit in ipairs(self.orbits) do
        orbit.emitter.position = {orbit.x + math.cos(self.time) * 220, -40 + math.sin(self.time) * 220}
        orbit.emitter:update(dt)
    end
    self:report('%d particles stay behind in world space, and %d move with their emitter in local space.', self.worldSpace.count, self.localSpace.count)
end

function Space:draw(area)
    ParticleTest.backdrop({0.03, 0.04, 0.09}, {0.07, 0.08, 0.16})
    for _, orbit in ipairs(self.orbits) do
        graphics2d.drawRing(orbit.x, -40, 220, 3, '#50FFFFFF', {layer = 1})
        graphics2d.drawCircle(orbit.emitter.x, orbit.emitter.y, 16, '#FF1E1B33', {layer = 3})
        graphics2d.drawCircle(orbit.emitter.x, orbit.emitter.y, 11, '#FFFFF0C0', {layer = 3})
        orbit.emitter:draw()
        ParticleTest.label(orbit.label, orbit.x, 250, orbit.option)
    end
end

return Space
