-- Fire: three emitters make the flame of a torch, painted flame frames that lick upward and shrink, embers that fly higher and soft smoke above them, each with a switch, and the torch follows the cursor.
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local Fire = haylen.class('Fire', ParticleTest)

Fire.code = [[
local flames = particles2d.newEmitter({texture = atlas.texture, frames = flame, rate = 44})
flames:configure({lifetime = {0.55, 0.85}, speed = {160, 240}, frameMode = 'loopRandomStart'})
flames:configure({startSize = {150, 190}, endSize = {30, 44}, sizeCurve = 'quadIn'})
flames:configure({colors = {'#00FFE7A8', '#C0FFC24A', '#A0FF7A1F', '#00801010'}, blend = 'additive'})
flames.x, flames.y = x, y  -- Live flames stay in the world, so they lean.
flames:configure({rate = 44 * strength})]]

Fire.rates = {smoke = 14, flames = 44, embers = 14}
Fire.parts = {{name = 'flames', text = 'Flames'}, {name = 'embers', text = 'Embers'}, {name = 'smoke', text = 'Smoke'}}
Fire.torchScale = 0.9

function Fire:init(entry)
    Fire.super.init(self, entry)
    self.strength = 1
    self.emitters = {
        smoke = particles2d.newEmitter(ParticleTest.look('smoke', {frameMode = 'random', rate = 14, lifetime = {2, 3}, speed = {70, 120}, spread = 0.35, gravity = {0, -30}, damping = 0.3, startSize = {80, 110}, endSize = {260, 340}, rotation = {0, 6.28}, spin = {-0.4, 0.4}, colors = {'#00585860', '#80686870', '#40585860', '#00484850'}, colorTimes = {0, 0.2, 0.6, 1}, shape = 'circle', shapeSize = {16, 0}, layer = 1, seed = 1})),
        flames = particles2d.newEmitter(ParticleTest.look('flame', {frameMode = 'loopRandomStart', frameRate = 14, rate = 44, lifetime = {0.55, 0.85}, speed = {160, 240}, spread = 0.2, startSize = {150, 190}, endSize = {30, 44}, sizeCurve = 'quadIn', colors = {'#00FFE7A8', '#C0FFC24A', '#A0FF7A1F', '#50D8381F', '#00801010'}, colorTimes = {0, 0.12, 0.45, 0.8, 1}, shape = 'ellipse', shapeSize = {34, 10}, blend = 'additive', layer = 3, seed = 2})),
        embers = particles2d.newEmitter(ParticleTest.look('ember', {rate = 14, lifetime = {1, 2}, speed = {160, 300}, spread = 0.7, gravity = {0, 40}, damping = 0.5, tangentialAcceleration = {-60, 60}, startSize = {10, 16}, endSize = 4, colors = {'#FFFFE080', '#FFFF7A20', '#00FF2000'}, shape = 'circle', shapeSize = {20, 0}, blend = 'additive', layer = 4, seed = 3})),
    }
end

function Fire:enter()
    self:frame{
        hint = 'The torch follows the cursor: move it with the mouse, a finger, the arrows, WASD or a stick. Its flames lean as it moves, because live particles stay in the world.',
        code = Fire.code,
        cursor = true,
        controls = {
            ui.formField{label = 'Strength', ui.slider{id = 'strength', min = 0.2, max = 3, value = self.strength, showValue = true, onChange = function(event)
                self.strength = event.value
                for name, emitter in pairs(self.emitters) do
                    emitter:configure({rate = Fire.rates[name] * event.value})
                end
            end}},
            ui.formField{label = 'Emitters of the torch', ui.column{gap = 8, children = self:emitterToggles()}},
        },
    }
end

-- Every emitter of the torch has a switch that stops or starts its spawning, so each part of the fire shows on its own.
function Fire:emitterToggles()
    local toggles = {}
    for index, part in ipairs(Fire.parts) do
        toggles[index] = ui.toggle{id = part.name, text = part.text, checked = true, onChange = function(event)
            self.emitters[part.name].emitting = event.checked
        end}
    end
    return toggles
end

function Fire:update(dt)
    Fire.super.update(self, dt)
    local count = 0
    for _, emitter in pairs(self.emitters) do
        emitter.x, emitter.y = self.cursorX, self.cursorY - 16
        emitter:update(dt)
        count = count + emitter.count
    end
    self:report('The torch burns with %d particles at %.0f percent of its strength.', count, self.strength * 100)
end

function Fire:draw(area)
    ParticleTest.backdrop({0.05, 0.05, 0.12}, {0.16, 0.08, 0.08})
    ParticleTest.paveFloor('stone', 360, 160, '#FF6A6070')
    ParticleTest.prop('wall-torch', self.cursorX, self.cursorY - 40, Fire.torchScale, {pivotY = 0, layer = 2})
    for _, emitter in pairs(self.emitters) do
        emitter:draw()
    end
end

return Fire
