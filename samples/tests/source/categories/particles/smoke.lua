-- Smoke: slow puffs that grow, spin and fade from the chimneys of a village, prewarmed so the plumes are fully grown on the first frame, and pushed by the wind through gravity.
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')
local Town = require('categories.lighting.town')

local Smoke = haylen.class('Smoke', ParticleTest)

Smoke.code = [[
local chimney = particles2d.newEmitter({texture = puff, rate = 10, prewarm = 6, lifetime = {4, 6}})
chimney:configure({speed = {50, 80}, gravity = {40, -8}, damping = 0.2, spin = {-0.4, 0.4}})
chimney:configure({startSize = {50, 70}, endSize = {220, 300}})
chimney:configure({colors = {'#00202020', '#C0303034', '#00101010'}})
chimney:configure({gravity = {wind, -8}})  -- The wind pushes the plume.]]

Smoke.kinds = {
    steam = {'#00FFFFFF', '#A0E8ECF0', '#00FFFFFF'},
    smoke = {'#00202020', '#C0303034', '#00101010'},
    toxic = {'#0040FF40', '#A060D040', '#00204010'},
}

Smoke.houses = {{name = 'cottage', x = -640}, {name = 'stone-house', x = 0}, {name = 'cottage', x = 640}}
Smoke.scale = 0.75
Smoke.ground = 400

function Smoke:init(entry)
    Smoke.super.init(self, entry)
    self.wind = 40
    local texture = 'smoke'
    self.town = Town()
    self.chimneys = {}
    for index, house in ipairs(Smoke.houses) do
        local emitter = particles2d.newEmitter(ParticleTest.look(texture, {rate = 10, prewarm = 6, lifetime = {4, 6}, speed = {50, 80}, spread = 0.3, gravity = {self.wind, -8}, damping = 0.2, startSize = {50, 70}, endSize = {220, 300}, spin = {-0.4, 0.4}, colors = Smoke.kinds.smoke, shape = 'circle', shapeSize = {16, 0}, maxParticles = 128, layer = 3, seed = index}))
        emitter.position = {self.town:chimney(house.name, house.x, Smoke.ground, Smoke.scale)}
        self.chimneys[index] = emitter
    end
end

function Smoke:enter()
    self:frame{
        hint = 'Change the wind and the kind of smoke. The plumes were already tall on the first frame thanks to prewarm.',
        code = Smoke.code,
        controls = {
            ui.formField{label = 'Wind', ui.slider{id = 'wind', min = -120, max = 120, value = self.wind, showValue = true, decimals = 0, onChange = function(event)
                self.wind = event.value
                for _, emitter in ipairs(self.chimneys) do
                    emitter:configure({gravity = {self.wind, -8}})
                end
            end}},
            ui.formField{label = 'Kind', ui.radioGroup{id = 'kind', horizontal = true, selected = 'smoke', items = {{id = 'steam', text = 'Steam'}, {id = 'smoke', text = 'Smoke'}, {id = 'toxic', text = 'Toxic'}}, onChange = function(event)
                for _, emitter in ipairs(self.chimneys) do
                    emitter:configure({colors = Smoke.kinds[event.value]})
                end
            end}},
        },
        focus = 'wind',
    }
end

function Smoke:update(dt)
    Smoke.super.update(self, dt)
    local count = 0
    for _, emitter in ipairs(self.chimneys) do
        emitter:update(dt)
        count = count + emitter.count
    end
    self:report('%d puffs rise from the chimneys, and the wind blows at %.0f.', count, self.wind)
end

function Smoke:draw(area)
    ParticleTest.backdrop({0.36, 0.56, 0.84}, {0.86, 0.84, 0.76})
    ParticleTest.paveFloor('grass', Smoke.ground - 10, 140)
    for index, house in ipairs(Smoke.houses) do
        self.town:house(house.name, house.x, Smoke.ground, Smoke.scale, 1, 0)
        self.chimneys[index]:draw()
    end
end

return Smoke
