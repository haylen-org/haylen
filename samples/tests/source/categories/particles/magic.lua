-- Magic: particles born on a ring swirl around the orb with tangential acceleration and fall into it with negative radial acceleration, stretched along their motion, an attractor at its heart swallows them, and stars twinkle through their frames.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local Magic = haylen.class('Magic', ParticleTest)

Magic.code = [[
local vortex = particles2d.newEmitter({texture = dot, rate = 180, shape = 'ring', shapeSize = {240, 0}})
vortex:configure({tangentialAcceleration = 240, radialAcceleration = -380})  -- Swirls and falls.
vortex:configure({alignToVelocity = true, stretch = 0.05, blend = 'additive'})
vortex:configure({attractors = {{strength = 320, radius = 170, killRadius = 26}}})  -- Swallowed.
vortex.position = {orb.x, orb.y}]]

-- The orb hovers above the cursor, like the light at the tip of a wand.
Magic.hover = 170

function Magic:init(entry)
    Magic.super.init(self, entry)
    self.orb = {x = 0, y = -Magic.hover}
    self.swirl = 240
    self.pull = 380
    self.vortex = particles2d.newEmitter(ParticleTest.look('soft-dot', {rate = 180, lifetime = {1.4, 1.8}, speed = {0, 20}, spread = 6.2832, startSize = {16, 22}, endSize = 6, alignToVelocity = true, stretch = 0.05, colors = {'#00A060FF', '#E0B070FF', '#C060E0FF', '#0040C0FF'}, shape = 'ring', shapeSize = {240, 0}, attractors = {{strength = 320, radius = 170, killRadius = 26}}, blend = 'additive', maxParticles = 512, layer = 2, seed = 13}))
    self.stars = particles2d.newEmitter(ParticleTest.look('sparkle', {rate = 14, lifetime = {0.8, 1.2}, speed = {10, 40}, spread = 6.2832, startSize = {30, 46}, endSize = {30, 46}, colors = {'#FFFFFFFF', '#FFA0E8FF', '#00A0E8FF'}, shape = 'circle', shapeSize = {280, 0}, blend = 'additive', layer = 3, seed = 14}))
    self.core = particles2d.newEmitter(ParticleTest.look('soft-dot', {rate = 40, lifetime = 0.5, speed = 0, startSize = {150, 170}, endSize = {190, 220}, colors = {'#00FFFFFF', '#70C090FF', '#00A060FF'}, blend = 'additive', layer = 1, seed = 15}))
    self.orbLook, self.swirlLook = ParticleTest.look('orb', {}), ParticleTest.look('swirl', {})
    self.pointer.origin = {0.5, 0.6}
    self:shape()
end

function Magic:shape()
    self.vortex:configure({tangentialAcceleration = self.swirl, radialAcceleration = -self.pull})
end

function Magic:enter()
    self:frame{
        hint = 'The orb drifts after the cursor. Turn the swirl and the pull to reshape the vortex.',
        code = Magic.code,
        cursor = true,
        controls = {
            ui.formField{label = 'Swirl, tangential acceleration', ui.slider{id = 'swirl', min = -600, max = 600, value = self.swirl, showValue = true, decimals = 0, onChange = function(event)
                self.swirl = event.value
                self:shape()
            end}},
            ui.formField{label = 'Pull, negative radial acceleration', ui.slider{id = 'pull', min = -300, max = 600, value = self.pull, showValue = true, decimals = 0, onChange = function(event)
                self.pull = event.value
                self:shape()
            end}},
        },
    }
end

function Magic:update(dt)
    Magic.super.update(self, dt)
    local follow = m.dampFactor(3, dt)
    self.orb.x, self.orb.y = m.lerp(self.orb.x, self.cursorX, follow), m.lerp(self.orb.y, self.cursorY - Magic.hover, follow)
    for _, emitter in ipairs({self.vortex, self.stars, self.core}) do
        emitter.position = {self.orb.x, self.orb.y}
        emitter:update(dt)
    end
    self:report('%d particles swirl into the orb with a swirl of %.0f and a pull of %.0f.', self.vortex.count + self.stars.count, self.swirl, self.pull)
end

-- The orb: a glass sphere of light with a swirl that turns inside it.
function Magic:drawOrb()
    local time = haylen.elapsed()
    local orb, swirl = self.orbLook, self.swirlLook
    graphics2d.draw(orb.texture, self.orb.x, self.orb.y, {source = orb.frames[1], width = 96, height = 96, color = '#FFE8F0FF', layer = 4})
    graphics2d.draw(swirl.texture, self.orb.x, self.orb.y, {source = swirl.frames[1], width = 76, height = 76, rotation = time * 3, color = '#C0B080FF', blend = 'additive', layer = 5})
end

function Magic:draw(area)
    ParticleTest.backdrop({0.05, 0.03, 0.12}, {0.12, 0.05, 0.2})
    for _, emitter in ipairs({self.core, self.vortex, self.stars}) do
        emitter:draw()
    end
    self:drawOrb()
end

return Magic
