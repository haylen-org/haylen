-- Magic: particles born on a ring swirl around the orb with tangential acceleration and fall into it with negative radial acceleration, while stars twinkle through their frames.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local art = require('art')
local sample = require('sample')

local Magic = haylen.class('Magic', sample.Test)

Magic.hints = 'The orb drifts after the cursor. Turn the swirl and the pull to reshape the vortex.'

function Magic:init(entry)
    Magic.super.init(self, entry)
    self.camera = graphics2d.newCamera()
    self.cursor = sample.Cursor()
    self.orb = {x = 0, y = 0}
    self.swirl = 260
    self.pull = 180
    local sparkle = art.texture('sparkle')
    self.vortex = particles2d.newEmitter({texture = art.texture('soft'), rate = 160, lifetime = {1.2, 1.6}, speed = {0, 20}, spread = 6.2832, startSize = {22, 30}, endSize = 4, colors = {'#00A060FF', '#FFB070FF', '#FF60E0FF', '#0040C0FF'}, shape = 'ring', shapeSize = {220, 0}, blend = 'additive', maxParticles = 512, layer = 2, seed = 13})
    self.stars = particles2d.newEmitter({texture = sparkle, frames = art.frames(sparkle), rate = 14, lifetime = {0.8, 1.2}, speed = {10, 40}, spread = 6.2832, startSize = {30, 46}, endSize = {30, 46}, colors = {'#FFFFFFFF', '#FFA0E8FF', '#00A0E8FF'}, shape = 'circle', shapeSize = {260, 0}, blend = 'additive', layer = 3, seed = 14})
    self.core = particles2d.newEmitter({texture = art.texture('soft'), rate = 40, lifetime = 0.5, speed = 0, startSize = {120, 150}, endSize = {170, 200}, colors = {'#00FFFFFF', '#80C090FF', '#00A060FF'}, blend = 'additive', layer = 1, seed = 15})
    self:shape()
end

function Magic:shape()
    self.vortex:configure({tangentialAcceleration = self.swirl, radialAcceleration = -self.pull})
end

function Magic:controls()
    return {
        ui.formField{label = 'Swirl, tangential acceleration', ui.slider{min = -600, max = 600, value = self.swirl, showValue = true, decimals = 0, onChange = function(event)
            self.swirl = event.value
            self:shape()
        end}},
        ui.formField{label = 'Pull, negative radial acceleration', ui.slider{min = -300, max = 600, value = self.pull, showValue = true, decimals = 0, onChange = function(event)
            self.pull = event.value
            self:shape()
        end}},
    }
end

function Magic:update(dt)
    self.cursor:update(dt)
    local x, y = self.cursor:world(self.camera)
    local follow = m.damp(3, dt)
    self.orb.x, self.orb.y = m.lerp(self.orb.x, x, follow), m.lerp(self.orb.y, y, follow)
    for _, emitter in ipairs({self.vortex, self.stars, self.core}) do
        emitter.position = {self.orb.x, self.orb.y}
        emitter:update(dt)
    end
    self:setStatus(string.format('tangentialAcceleration %.0f, radialAcceleration %.0f, %d particles', self.swirl, -self.pull, self.vortex.count + self.stars.count))
end

function Magic:render()
    graphics2d.beginWorld(self.camera)
    art.backdrop({0.04, 0.02, 0.1}, {0.1, 0.04, 0.18})
    graphics2d.drawRing(self.orb.x, self.orb.y, 230, 3, '#40B090FF')
    graphics2d.drawCircle(self.orb.x, self.orb.y, 40, '#FFE8F0FF', {layer = 4})
    for _, emitter in ipairs({self.core, self.vortex, self.stars}) do
        emitter:draw()
    end
end

function Magic:renderUi()
    graphics2d.beginScreen()
    self.cursor:draw()
end

return Magic
