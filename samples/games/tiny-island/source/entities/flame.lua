-- The flame of the campfire: burning logs under a stream of flame particles, and the flickering light they cast. The island behind the menus and the run both draw their fire with it.
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

function flame.new(x, y)
    local self = setmetatable({x = x, y = y, time = 0}, flame)
    self.logs = art.texture('terrain/resources/wood/wood_resource/wood_resource.png')
    self.particles = particles2d.newEmitter(assets.load('effects/flames.particles'))
    self.particles.x = x
    self.particles.y = y - 10
    return self
end

-- Stops sending particles while `burning` is false, so the last ones fade out when the fire dies.
function flame:update(dt, burning)
    self.time = self.time + dt
    self.particles.emitting = burning
    self.particles:update(dt)
end

function flame:draw()
    for index = -1, 1 do
        graphics2d.draw(self.logs, self.x + index * 18, self.y + 10 - math.abs(index) * 4, {pivotX = 0.5, pivotY = 0.5, scaleX = 0.8, scaleY = 0.8, rotation = index * 0.5, layer = config.layer.entities, depth = self.y - 1})
    end
    self.particles:draw()
end

-- Lights the island around the fire out to `radius` in `color`, wavering like the flames.
function flame:light(radius, color)
    local intensity = lighting2d.flicker(self.time, {speed = 7, amount = 0.12})
    graphics2d.drawLight({x = self.x, y = self.y - 20, radius = radius, color = color, intensity = intensity})
end

return flame
