-- Rain: two rectangle emitters above the view drop streaks over the whole canvas, far thin ones behind the houses of a village and near bold ones in front, and a line along the wet street throws up splashes and rings.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')
local Town = require('categories.lighting.town')

local Rain = haylen.class('Rain', ParticleTest)

Rain.code = [[
local drops = particles2d.newEmitter({texture = streak, rate = 500, lifetime = 0.95, direction = 1.62})
drops:configure({speed = {1150, 1350}, startSize = {70, 96}, aspect = 2.4})  -- Bolder streaks.
drops:configure({shape = 'rectangle', shapeSize = {width + 400, 10}})  -- The whole canvas and the drift.
drops.position = {-80, top - 60}
drops:configure({rate = 500 * intensity})]]

Rain.ground = 340
Rain.houses = {{name = 'stone-house', x = -1000}, {name = 'cottage', x = -560}, {name = 'stone-house', x = -60}, {name = 'cottage', x = 420}, {name = 'stone-house', x = 900}}
Rain.layers = {
    far = {rate = 320, speed = {800, 950}, size = {34, 46}, width = 1.6, color = '#8090A8C8', layer = 0},
    near = {rate = 260, speed = {1250, 1450}, size = {70, 96}, width = 2.4, color = '#C0C8DCF0', layer = 4},
}

function Rain:init(entry)
    Rain.super.init(self, entry)
    self.town = Town()
    self.intensity = 1
    self.width = 0
    self.drops = {}
    for name, layer in pairs(Rain.layers) do
        self.drops[name] = particles2d.newEmitter(ParticleTest.look('rain-streak', {rate = layer.rate, lifetime = 1.1, speed = layer.speed, direction = 1.62, spread = 0.02, startSize = layer.size, endSize = layer.size, aspect = {layer.width, layer.width}, colors = {layer.color, layer.color}, shape = 'rectangle', shapeSize = {2400, 10}, maxParticles = 2048, layer = layer.layer, seed = layer.layer + 4}))
    end
    self.splashes = particles2d.newEmitter(ParticleTest.look('splash', {rate = 70, lifetime = {0.25, 0.4}, speed = 0, startSize = {20, 34}, endSize = {44, 60}, colors = {'#C0D8E8FF', '#00D8E8FF'}, shape = 'rectangle', shapeSize = {2400, 40}, maxParticles = 512, layer = 5, seed = 5}))
    self.splashes.position = {0, Rain.ground + 40}
    self.ripples = particles2d.newEmitter(ParticleTest.look('ring-thin', {rate = 50, lifetime = {0.5, 0.8}, speed = 0, startSize = 10, endSize = {60, 90}, tilt = 1.25, colors = {'#90C0D8F0', '#00C0D8F0'}, shape = 'rectangle', shapeSize = {2400, 200}, maxParticles = 512, layer = 2, seed = 6}))
    self.ripples.position = {0, Rain.ground + 150}
end

function Rain:enter()
    self:frame{
        hint = 'Change how hard it rains. The streaks start above the canvas, whatever its width, and the splashes and rings come from the street.',
        code = Rain.code,
        controls = {ui.formField{label = 'Intensity', ui.slider{id = 'intensity', min = 0, max = 3, value = self.intensity, showValue = true, onChange = function(event)
            self.intensity = event.value
            for name, emitter in pairs(self.drops) do
                emitter:configure({rate = Rain.layers[name].rate * event.value})
            end
            self.splashes:configure({rate = 70 * event.value})
            self.ripples:configure({rate = 50 * event.value})
        end}}},
        focus = 'intensity',
    }
end

-- Sizes the emitters to the canvas, whose width changes with the play area, with room for the drift of the slanted streaks.
function Rain:fit()
    local area = self.camera:visibleBounds()
    if area.width == self.width then
        return
    end
    self.width = area.width
    for _, emitter in pairs(self.drops) do
        emitter:configure({shapeSize = {area.width + 400, 10}})
        emitter.position = {area.x + area.width / 2 - 80, area.y - 80}
    end
    self.splashes:configure({shapeSize = {area.width, 40}})
    self.ripples:configure({shapeSize = {area.width, 200}})
end

function Rain:update(dt)
    Rain.super.update(self, dt)
    if not self.stage then
        return
    end
    self:fit()
    local count = 0
    for _, emitter in pairs(self.drops) do
        emitter:update(dt)
        count = count + emitter.count
    end
    self.splashes:update(dt)
    self.ripples:update(dt)
    self:report('%d drops fall at %.0f percent, and %d splashes rise from the street.', count, self.intensity * 100, self.splashes.count)
end

function Rain:draw(area)
    ParticleTest.backdrop({0.08, 0.1, 0.17}, {0.2, 0.24, 0.32})
    for _, house in ipairs(Rain.houses) do
        self.town:house(house.name, house.x, Rain.ground + 10, 0.62, 1, 1, '#FF6A7894')
    end
    ParticleTest.paveFloor('stone', Rain.ground, 150, '#FF56606E')
    graphics2d.drawRect({graphics2d.canvasBounds().x, Rain.ground + 4, graphics2d.canvasBounds().width, 900}, '#300A1A30', {layer = -3})
    for _, emitter in pairs(self.drops) do
        emitter:draw()
    end
    self.ripples:draw()
    self.splashes:draw()
end

return Rain
