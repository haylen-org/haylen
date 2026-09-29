-- Gravity and accelerations: gravity pulls every particle the same way, radial acceleration pushes them away from the emitter or pulls them in, tangential acceleration turns them around it and damping slows them.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local art = require('art')
local sample = require('sample')

local Forces = haylen.class('Forces', sample.Test)

Forces.hints = 'Move the sliders and watch the stream bend. Negative radial acceleration pulls particles back toward the emitter.'

Forces.defaults = {gravityX = 0, gravityY = 300, radial = 0, tangential = 0, damping = 0}

function Forces:init(entry)
    Forces.super.init(self, entry)
    self.camera = graphics2d.newCamera()
    self.values = {}
    for key, value in pairs(Forces.defaults) do
        self.values[key] = value
    end
    self.emitter = particles2d.newEmitter({texture = art.texture('soft'), rate = 180, lifetime = {2, 2.5}, speed = {220, 280}, direction = -1.5708, spread = 0.5, startSize = 20, endSize = 6, colors = {'#FF60E0FF', '#FFFFE070', '#00FF6040'}, shape = 'circle', shapeSize = {10, 0}, blend = 'additive', maxParticles = 1024, layer = 2, seed = 60})
    self.emitter.position = {-120, 120}
    self:apply()
end

function Forces:apply()
    local values = self.values
    self.emitter:configure({gravity = {values.gravityX, values.gravityY}, radialAcceleration = values.radial, tangentialAcceleration = values.tangential, damping = values.damping})
end

function Forces:controls()
    local function slider(key, label, minimum, maximum)
        return ui.formField{label = label, ui.slider{min = minimum, max = maximum, value = self.values[key], showValue = true, decimals = key == 'damping' and 2 or 0, onChange = function(event)
            self.values[key] = event.value
            self:apply()
        end}}
    end
    return {
        slider('gravityX', 'Gravity x', -600, 600),
        slider('gravityY', 'Gravity y', -600, 600),
        slider('radial', 'Radial acceleration', -600, 600),
        slider('tangential', 'Tangential acceleration', -600, 600),
        slider('damping', 'Damping', 0, 4),
    }
end

function Forces:update(dt)
    Forces.super.update(self, dt)
    self.emitter:update(dt)
    local values = self.values
    self:setStatus(string.format('gravity %.0f, %.0f, radial %.0f, tangential %.0f, damping %.2f, %d particles', values.gravityX, values.gravityY, values.radial, values.tangential, values.damping, self.emitter.count))
end

function Forces:render()
    graphics2d.beginWorld(self.camera)
    art.backdrop({0.04, 0.05, 0.1}, {0.08, 0.1, 0.18})
    graphics2d.drawCircle(self.emitter.x, self.emitter.y, 12, '#FFFFFFFF', {layer = 3})
    self.emitter:draw()
end

return Forces
