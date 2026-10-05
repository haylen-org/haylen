-- The fire of the camp: the ring of stones with its logs, the flames, embers and smoke that rise from it, and the flickering light they cast. This is the one place that defines the campfire effect, and the island behind the menus and the run both draw their fire with it.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local lighting2d = require('haylen.lighting2d')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')

local art = require('systems.art')
local config = require('config')

local flame = {}
flame.__index = flame

-- The warm color of the firelight at full strength.
flame.glow = m.color('#FFFFB870')

-- Each layer of the effect rises from its own height above the logs, and draws in front of the stones.
local layers = {
    {effect = 'effects/campfire_smoke.particles', lift = 70},
    {effect = 'effects/campfire_flames.particles', lift = 22},
    {effect = 'effects/campfire_embers.particles', lift = 34},
}

function flame.new(x, y)
    local self = setmetatable({x = x, y = y, time = 0, emitters = {}}, flame)
    self.atlas = art.props()
    self.source = self.atlas:source('campfire')
    for index, spec in ipairs(layers) do
        local emitter = particles2d.newEmitter(assets.load(spec.effect, nil, art.options), {depth = y + index})
        emitter.x = x
        emitter.y = y - spec.lift
        self.emitters[index] = emitter
    end
    return self
end

-- Stops sending particles while `burning` is false, so the last ones fade out when the fire dies.
function flame:update(dt, burning)
    self.time = self.time + dt
    for _, emitter in ipairs(self.emitters) do
        emitter.emitting = burning
        emitter:update(dt)
    end
end

function flame:draw()
    graphics2d.draw(self.atlas.texture, self.x, self.y + 26, {source = self.source, pivotX = 0.5, pivotY = 1, layer = config.layer.entities, depth = self.y})
    for _, emitter in ipairs(self.emitters) do
        emitter:draw()
    end
end

-- Lights the island around the fire out to `radius` in `color`, wavering like the flames.
function flame:light(radius, color)
    local intensity = lighting2d.flicker(self.time, {speed = 7, amount = 0.12})
    graphics2d.drawLight({x = self.x, y = self.y - 24, radius = radius, color = color, intensity = intensity})
end

return flame
