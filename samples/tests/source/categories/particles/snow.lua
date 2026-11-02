-- Snow: two emitters, small slow flakes far away and large ones close by, prewarmed so the view starts full, drifting with the wind over snowy mountains and pines.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')
local Town = require('categories.lighting.town')

local Snow = haylen.class('Snow', ParticleTest)

Snow.code = [[
local far = particles2d.newEmitter({texture = flake, rate = 70, prewarm = 12, lifetime = {12, 14}})
far:configure({speed = {40, 70}, direction = 1.5708, startSize = {8, 12}, endSize = {8, 12}})
far:configure({shape = 'rectangle', shapeSize = {width, 10}, colors = {'#90D8E8FF'}})
near:configure({startSize = {20, 30}, colors = {'#FFE4F0FF'}, layer = 3})  -- Bigger and closer.
far:configure({gravity = {wind * 0.5, 4}})
near:configure({gravity = {wind, 4}})]]

-- The pines stand on the far slope at these places and scales, sunk into it a little.
Snow.pines = {{-980, 0.95}, {-760, 1.2}, {-180, 1}, {420, 0.9}, {660, 1.25}, {1040, 1.05}}
Snow.far = {y = 260, height = 50, phase = 0.4}
Snow.near = {y = 380, height = 40, phase = 1.7}

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

-- Returns the height of a slope at `x`: a wave of gentle hills whose top runs around the `y` of the slope.
function Snow.surface(slope, x)
    return slope.y - math.sin(x * 0.0024 + slope.phase) * slope.height - math.sin(x * 0.0061 + slope.phase * 2) * slope.height * 0.35
end

-- Builds a smooth snowy slope across the canvas as a mesh that runs from the lit color along its crest down into a bluer shade, and a bright rim where the moonlight catches its top.
function Snow.slope(area, slope, lit, shade)
    local vertices, indices, rim = {}, {}, {}
    local steps = 64
    for index = 0, steps do
        local x = area.x + area.width * index / steps
        local y = Snow.surface(slope, x)
        vertices[#vertices + 1] = {x = x, y = y, color = lit}
        vertices[#vertices + 1] = {x = x, y = y + 110, color = shade}
        vertices[#vertices + 1] = {x = x, y = area:bottom(), color = shade}
        rim[#rim + 1] = x
        rim[#rim + 1] = y + 3
        if index > 0 then
            local a, b = (index - 1) * 3, index * 3
            for _, offset in ipairs({0, 1}) do
                indices[#indices + 1] = a + offset + 1
                indices[#indices + 1] = b + offset + 1
                indices[#indices + 1] = b + offset + 2
                indices[#indices + 1] = a + offset + 1
                indices[#indices + 1] = b + offset + 2
                indices[#indices + 1] = a + offset + 2
            end
        end
    end
    return {mesh = graphics2d.newMesh(nil, vertices, indices), rim = rim}
end

-- Builds the slopes again only when the canvas changes, which happens when the play area does.
function Snow:slopes(canvas)
    local key = canvas.x .. ',' .. canvas.width .. ',' .. canvas:bottom()
    if key ~= self.slopesKey then
        self.slopesKey = key
        self.farSlope = Snow.slope(canvas, Snow.far, '#FFC8D6EE', '#FF8E9CC8')
        self.nearSlope = Snow.slope(canvas, Snow.near, '#FFF0F5FC', '#FFB4C2DE')
    end
    return self.farSlope, self.nearSlope
end

function Snow.drawSlope(slope, layer)
    slope.mesh:draw(0, 0, {layer = layer})
    graphics2d.drawStroke(slope.rim, {width = 6, color = '#C0FFFFFF', cap = 'round', join = 'round', layer = layer})
end

function Snow:draw(area)
    ParticleTest.backdrop({0.04, 0.06, 0.15}, {0.24, 0.3, 0.46})
    local canvas = graphics2d.canvasBounds()
    Town.strip(self.mountains, 300, 460, 0, '#FF8C98C8')
    local far, near = self:slopes(canvas)
    Snow.drawSlope(far, 1)
    -- Each pine sinks into the slope, and a drift of snow buries its foot, so it stands in the snow.
    for _, pine in ipairs(Snow.pines) do
        local x, scale = pine[1], pine[2]
        local y = Snow.surface(Snow.far, x) + 30 * scale
        graphics2d.draw(graphics2d.lightTexture(), x + 30 * scale, y - 6 * scale, {width = 180 * scale, height = 40 * scale, color = '#70404C80', layer = 2})
        graphics2d.draw(self.trees, x, y, {source = {134, 2, 128, 256}, width = 128 * scale, height = 256 * scale, pivotY = 1, color = '#FFA8B8D8', layer = 2})
        graphics2d.drawShape({x - 62 * scale, y - 30 * scale, 124 * scale, 46 * scale}, {kind = 'ellipse', color = '#FFC8D6EE', layer = 3})
        graphics2d.drawShape({x - 46 * scale, y - 28 * scale, 70 * scale, 12 * scale}, {kind = 'ellipse', color = '#B0FFFFFF', layer = 3})
    end
    Snow.drawSlope(near, 4)
    for _, layer in ipairs(self.layers) do
        layer.emitter:draw()
    end
end

return Snow
