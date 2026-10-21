-- Snow: two emitters, small slow flakes far away and large ones close by, prewarmed so the view starts full, drifting with the wind over snowy mountains and pines.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local Snow = haylen.class('Snow', ParticleTest)

Snow.code = [[
local far = particles2d.newEmitter({texture = flake, rate = 70, prewarm = 12, lifetime = {12, 14}})
far:configure({speed = {40, 70}, direction = 1.5708, startSize = {8, 12}, endSize = {8, 12}})
far:configure({shape = 'rectangle', shapeSize = {width, 10}, colors = {'#90D8E8FF'}})
near:configure({startSize = {20, 30}, colors = {'#FFE4F0FF'}, layer = 3})  -- Bigger and closer.
far:configure({gravity = {wind * 0.5, 4}})
near:configure({gravity = {wind, 4}})]]

Snow.pines = {{-980, 200, 0.95}, {-760, 250, 1.2}, {-180, 230, 1}, {420, 190, 0.9}, {660, 260, 1.25}, {1040, 220, 1.05}}

function Snow:init(entry)
    Snow.super.init(self, entry)
    self.mountains = assets.texture('camera/mountains.png', {filter = 'linear', wrap = 'repeat'})
    self.trees = assets.texture('tiled/tiles/props.png', {filter = 'linear'})
    self.wind = 30
    local flake = 'snowflake'
    local area = {shape = 'rectangle', shapeSize = {2600, 10}, direction = 1.5708, spread = 0.4}
    self.layers = {
        {drift = 0.5, emitter = particles2d.newEmitter(ParticleTest.look(flake, {rate = 110, prewarm = 12, lifetime = {12, 14}, speed = {40, 70}, spread = area.spread, direction = area.direction, tangentialAcceleration = {-4, 4}, startSize = {8, 12}, endSize = {8, 12}, spin = {-1, 1}, colors = {'#90D8E8FF'}, shape = area.shape, shapeSize = area.shapeSize, maxParticles = 2048, layer = 1, seed = 6}))},
        {drift = 1, emitter = particles2d.newEmitter(ParticleTest.look(flake, {rate = 40, prewarm = 12, lifetime = {7, 8}, speed = {90, 140}, spread = area.spread, direction = area.direction, tangentialAcceleration = {-8, 8}, startSize = {20, 30}, endSize = {20, 30}, spin = {-2, 2}, colors = {'#FFE4F0FF'}, shape = area.shape, shapeSize = area.shapeSize, maxParticles = 512, layer = 5, seed = 7}))},
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

function Snow:enter()
    self:frame{
        hint = 'Blow the snow with the wind. The far layer is smaller, dimmer and slower, which reads as depth.',
        code = Snow.code,
        controls = {ui.formField{label = 'Wind', ui.slider{id = 'wind', min = -150, max = 150, value = self.wind, showValue = true, decimals = 0, onChange = function(event)
            self.wind = event.value
            self:blow()
        end}}},
        focus = 'wind',
    }
end

-- Spreads the flakes over the width of the canvas, which changes with the play area, and the drift of the wind.
function Snow:fit()
    local area = self.camera:visibleBounds()
    if area.width == self.width then
        return
    end
    self.width = area.width
    for _, layer in ipairs(self.layers) do
        layer.emitter:configure({shapeSize = {area.width + 600, 10}})
        layer.emitter.position = {area.x + area.width / 2, area.y - 40}
    end
end

function Snow:update(dt)
    Snow.super.update(self, dt)
    if not self.stage then
        return
    end
    self:fit()
    local count = 0
    for _, layer in ipairs(self.layers) do
        layer.emitter:update(dt)
        count = count + layer.emitter.count
    end
    self:report('%d flakes drift in a wind of %.0f.', count, self.wind)
end

-- Returns the outline of a smooth snowy slope across the canvas, a wave of gentle hills whose top runs around `y`.
function Snow.slope(area, y, height, phase, layer, color)
    local points = {}
    local steps = 48
    for index = 0, steps do
        local x = area.x + area.width * index / steps
        points[#points + 1] = {x, y - math.sin(x * 0.0024 + phase) * height - math.sin(x * 0.0061 + phase * 2) * height * 0.35}
    end
    points[#points + 1] = {area:right(), area:bottom()}
    points[#points + 1] = {area.x, area:bottom()}
    graphics2d.drawPolygon(points, color, {layer = layer})
end

function Snow:draw(area)
    ParticleTest.backdrop({0.04, 0.06, 0.15}, {0.24, 0.3, 0.46})
    local canvas = graphics2d.canvasBounds()
    local height = 460
    local width = self.mountains.width * height / self.mountains.height
    local first = math.floor(canvas.x / width) * width
    local copies = math.ceil((canvas:right() - first) / width)
    graphics2d.draw(self.mountains, first, 300 - height, {source = {0, 0, self.mountains.width * copies, self.mountains.height}, width = width * copies, height = height, pivotX = 0, pivotY = 0, color = '#FF8C98C8', layer = 0})
    Snow.slope(canvas, 260, 50, 0.4, 1, '#FFB8C8E4')
    for _, pine in ipairs(Snow.pines) do
        graphics2d.draw(self.trees, pine[1], pine[2] + 30, {source = {134, 2, 128, 256}, width = 128 * pine[3], height = 256 * pine[3], pivotY = 1, color = '#FFA8B8D8', layer = 2})
    end
    Snow.slope(canvas, 380, 40, 1.7, 4, '#FFE8F0FA')
    for _, layer in ipairs(self.layers) do
        layer.emitter:draw()
    end
end

return Snow
