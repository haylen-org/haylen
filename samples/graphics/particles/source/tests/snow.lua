-- Snow: two emitters, small slow flakes far away and large ones close by, prewarmed so the view starts full, drifting with the wind.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local art = require('art')
local sample = require('sample')

local Snow = haylen.class('Snow', sample.Test)

Snow.hints = 'Blow the snow with the wind. The far layer is smaller, dimmer and slower, which reads as depth.'

function Snow:init(entry)
    Snow.super.init(self, entry)
    self.camera = graphics2d.newCamera()
    self.wind = 30
    local flake = art.texture('snowflake')
    local area = {shape = 'rectangle', shapeSize = {1300, 10}, direction = 1.5708, spread = 0.4}
    self.layers = {
        {drift = 0.5, emitter = particles2d.newEmitter({texture = flake, rate = 70, prewarm = 12, lifetime = {12, 14}, speed = {40, 70}, spread = area.spread, direction = area.direction, tangentialAcceleration = {-4, 4}, startSize = {8, 12}, endSize = {8, 12}, spin = {-1, 1}, colors = {'#90D8E8FF'}, shape = area.shape, shapeSize = area.shapeSize, maxParticles = 1024, layer = 1, seed = 6})},
        {drift = 1, emitter = particles2d.newEmitter({texture = flake, rate = 25, prewarm = 12, lifetime = {7, 8}, speed = {90, 140}, spread = area.spread, direction = area.direction, tangentialAcceleration = {-8, 8}, startSize = {20, 30}, endSize = {20, 30}, spin = {-2, 2}, colors = {'#F0FFFFFF'}, shape = area.shape, shapeSize = area.shapeSize, maxParticles = 512, layer = 3, seed = 7})},
    }
    for _, layer in ipairs(self.layers) do
        layer.emitter.position = {0, -620}
    end
    self:blow()
end

function Snow:blow()
    for _, layer in ipairs(self.layers) do
        layer.emitter:configure({gravity = {self.wind * layer.drift, 4}})
    end
end

function Snow:controls()
    return {ui.formField{label = 'Wind', ui.slider{min = -150, max = 150, value = self.wind, showValue = true, decimals = 0, onChange = function(event)
        self.wind = event.value
        self:blow()
    end}}}
end

function Snow:update(dt)
    Snow.super.update(self, dt)
    local count = 0
    for _, layer in ipairs(self.layers) do
        layer.emitter:update(dt)
        count = count + layer.emitter.count
    end
    self:setStatus(string.format('%d flakes, wind %.0f', count, self.wind))
end

function Snow:render()
    graphics2d.beginWorld(self.camera)
    art.backdrop({0.05, 0.08, 0.16}, {0.2, 0.26, 0.38})
    graphics2d.drawPolygon({{-1600, 380}, {-700, 200}, {100, 360}, {900, 180}, {1600, 340}, {1600, 900}, {-1600, 900}}, '#FFD8E4F0', {layer = 2})
    for _, layer in ipairs(self.layers) do
        layer.emitter:draw()
    end
end

return Snow
