-- Local and world space: in world space live particles stay where they were born when the emitter moves, and with `localSpace` they move with the emitter, like the exhaust of a ship against a shield around it.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local art = require('art')
local sample = require('sample')

local Space = haylen.class('Space', sample.Test)

Space.hints = 'Both emitters are the same except "localSpace". Change how fast they circle.'

function Space:init(entry)
    Space.super.init(self, entry)
    self.camera = graphics2d.newCamera()
    self.speed = 2
    self.time = 0
    local options = {texture = art.texture('soft'), rate = 120, lifetime = {1, 1.4}, speed = {40, 90}, spread = 6.2832, startSize = {26, 32}, endSize = 2, colors = {'#FFFFE080', '#FFFF7040', '#00A02060'}, blend = 'additive', maxParticles = 512, layer = 2}
    options.localSpace, options.seed = false, 50
    self.worldSpace = particles2d.newEmitter(options)
    options.localSpace, options.seed = true, 51
    self.localSpace = particles2d.newEmitter(options)
    self.orbits = {{emitter = self.worldSpace, x = -480, label = 'localSpace = false'}, {emitter = self.localSpace, x = 480, label = 'localSpace = true'}}
end

function Space:controls()
    return {ui.formField{label = 'Circling speed', ui.slider{min = 0, max = 6, value = self.speed, showValue = true, onChange = function(event)
        self.speed = event.value
    end}}}
end

function Space:update(dt)
    self.time = self.time + dt * self.speed
    for _, entry in ipairs(self.orbits) do
        entry.emitter.position = {entry.x + math.cos(self.time) * 220, 40 + math.sin(self.time) * 220}
        entry.emitter:update(dt)
    end
    self:setStatus(string.format('%d particles in world space, %d in local space', self.worldSpace.count, self.localSpace.count))
end

function Space:render()
    graphics2d.beginWorld(self.camera)
    art.backdrop({0.03, 0.04, 0.09}, {0.07, 0.08, 0.16})
    for _, entry in ipairs(self.orbits) do
        graphics2d.drawRing(entry.x, 40, 220, 2, '#40FFFFFF', {layer = 1})
        graphics2d.drawCircle(entry.emitter.x, entry.emitter.y, 14, '#FFFFFFFF', {layer = 3})
        entry.emitter:draw()
    end
end

function Space:renderUi()
    graphics2d.beginScreen()
    for _, entry in ipairs(self.orbits) do
        art.label(self.camera, entry.label, entry.x, 300)
    end
end

return Space
