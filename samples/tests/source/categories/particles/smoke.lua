-- Smoke: slow puffs that grow, spin and fade, prewarmed so the plumes are fully grown on the first frame, and pushed by the wind through gravity.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local Smoke = haylen.class('Smoke', ParticleTest)

Smoke.kinds = {
    steam = {'#00FFFFFF', '#A0E8ECF0', '#00FFFFFF'},
    smoke = {'#00202020', '#C0303034', '#00101010'},
    toxic = {'#0040FF40', '#A060D040', '#00204010'},
}

function Smoke:init(entry)
    Smoke.super.init(self, entry)
    self.wind = 40
    local texture = ParticleTest.texture('smoke')
    self.chimneys = {}
    for index = 1, 3 do
        local emitter = particles2d.newEmitter({texture = texture, frames = ParticleTest.frames(texture), rate = 10, prewarm = 6, lifetime = {4, 6}, speed = {50, 80}, spread = 0.3, gravity = {self.wind, -8}, damping = 0.2, startSize = {50, 70}, endSize = {220, 300}, spin = {-0.4, 0.4}, colors = Smoke.kinds.smoke, shape = 'circle', shapeSize = {16, 0}, maxParticles = 128, layer = 1, seed = index})
        emitter.position = {-620 + (index - 1) * 560, 180}
        self.chimneys[index] = emitter
    end
end

function Smoke:enter()
    self:frame{
        hint = 'Change the wind and the kind of smoke. The plumes were already tall on the first frame thanks to prewarm.',
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
    self:status(string.format('Puffs %d   Wind %.0f', count, self.wind))
end

function Smoke:draw(area)
    ParticleTest.backdrop({0.35, 0.45, 0.6}, {0.75, 0.7, 0.6})
    graphics2d.drawRect({-1600, 300, 3200, 700}, '#FF3A4A3A')
    for _, emitter in ipairs(self.chimneys) do
        graphics2d.drawRect({emitter.x - 90, emitter.y + 60, 180, 240}, '#FF6A4A3A')
        graphics2d.drawRect({emitter.x - 30, emitter.y, 60, 80}, '#FF4A3A30')
        emitter:draw()
    end
end

return Smoke
