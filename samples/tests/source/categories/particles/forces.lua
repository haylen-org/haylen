-- Gravity and accelerations: gravity pulls every drop of a fountain the same way, radial acceleration pushes them away from its spout or pulls them in, tangential acceleration turns them around it and damping slows them.
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local Forces = haylen.class('Forces', ParticleTest)

Forces.code = [[
local fountain = particles2d.newEmitter({texture = drop, rate = 180, speed = {420, 520}})
fountain:configure({direction = -1.5708, spread = 0.35, gravity = {0, 300}})
fountain:configure({radialAcceleration = -200})  -- Pulls particles back to the emitter.
fountain:configure({tangentialAcceleration = 300})  -- Turns them around it.
fountain:configure({damping = 1.5})  -- Slows them down.]]

Forces.ground = 420
Forces.scale = 0.7
-- The brass spout of the fountain that stands on the ground in the middle, from its place in the picture of 404 by 318 pixels.
Forces.spout = {(199 - 202) * Forces.scale, Forces.ground + 20 - (318 - 70) * Forces.scale}

Forces.defaults = {gravityX = 0, gravityY = 300, radial = 0, tangential = 0, damping = 0}
Forces.sliders = {
    {'gravityX', 'Gravity x', -600, 600},
    {'gravityY', 'Gravity y', -600, 600},
    {'radial', 'Radial acceleration', -600, 600},
    {'tangential', 'Tangential acceleration', -600, 600},
    {'damping', 'Damping', 0, 4},
}

function Forces:init(entry)
    Forces.super.init(self, entry)
    self.values = {}
    for key, value in pairs(Forces.defaults) do
        self.values[key] = value
    end
    self.emitter = particles2d.newEmitter(ParticleTest.look('droplet', {rate = 180, lifetime = {2.6, 3.0}, speed = {420, 520}, direction = -1.5708, spread = 0.35, startSize = {18, 24}, endSize = {10, 14}, alignToVelocity = true, rotation = 1.5708, colors = {'#FFD0F0FF', '#E090D0FF', '#0060B0F0'}, colorTimes = {0, 0.7, 1}, shape = 'circle', shapeSize = {6, 0}, maxParticles = 1024, layer = 3, seed = 60}))
    self.emitter.position = {Forces.spout[1], Forces.spout[2]}
    self:apply()
end

function Forces:apply()
    local values = self.values
    self.emitter:configure({gravity = {values.gravityX, values.gravityY}, radialAcceleration = values.radial, tangentialAcceleration = values.tangential, damping = values.damping})
end

function Forces:enter()
    local controls = {}
    for index, spec in ipairs(Forces.sliders) do
        local key = spec[1]
        controls[index] = ui.formField{label = spec[2], ui.slider{id = key, min = spec[3], max = spec[4], value = self.values[key], showValue = true, decimals = key == 'damping' and 2 or 0, onChange = function(event)
            self.values[key] = event.value
            self:apply()
        end}}
    end
    self:frame{hint = 'Move the sliders and watch the stream bend. Negative radial acceleration pulls particles back toward the emitter.', code = Forces.code, controls = controls, focus = 'gravityX'}
end

function Forces:update(dt)
    Forces.super.update(self, dt)
    self.emitter:update(dt)
    local values = self.values
    self:report('%d drops: gravity %.0f, %.0f, radial %.0f, tangential %.0f and damping %.2f.', self.emitter.count, values.gravityX, values.gravityY, values.radial, values.tangential, values.damping)
end

function Forces:draw(area)
    ParticleTest.backdrop({0.36, 0.56, 0.84}, {0.8, 0.86, 0.9})
    ParticleTest.paveFloor('grass', Forces.ground - 6, 140)
    ParticleTest.prop('fountain', 0, Forces.ground + 20, Forces.scale, {pivotY = 1, layer = 2})
    self.emitter:draw()
end

return Forces
