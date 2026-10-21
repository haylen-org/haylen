-- Turbulence and attractors: `turbulence` pushes particles through moving noise so smoke curls, and an attractor pulls the particles inside its radius toward a point, harder the closer they get, and removes them at its kill radius, like coins that fly to a counter. The attractor lives in world space and `emitter:setAttractor` moves it to the cursor every frame.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')
local Town = require('categories.lighting.town')

local TurbulenceAttractors = haylen.class('TurbulenceAttractors', ParticleTest)

TurbulenceAttractors.code = [[
local smoke = particles2d.newEmitter({texture = puff, rate = 14})
smoke:configure({turbulence = {strength = 260, frequency = 0.004, speed = 0.6}})
local coins = particles2d.newEmitter({texture = coin, rate = 5, damping = 1.2})
coins:configure({attractors = {{x = 0, y = 0, strength = 2800, radius = 2600, killRadius = 40, space = 'world'}}})
coins:setAttractor(1, cursorX, cursorY)  -- Every frame, in the space of the attractor.
coins:burst(24)]]

TurbulenceAttractors.killRadius = 40
TurbulenceAttractors.ground = 400
TurbulenceAttractors.house = {x = -560, scale = 0.62}
-- The chest of the terrain tileset, which the coins jump out of.
TurbulenceAttractors.chest = {x = 530, size = 170, source = {662, 266, 128, 128}}
TurbulenceAttractors.ring = {layer = 4}

function TurbulenceAttractors:init(entry)
    TurbulenceAttractors.super.init(self, entry)
    self.strength = 260
    self.frequency = 0.004
    self.pull = 2800
    self.smoke = particles2d.newEmitter(ParticleTest.look('smoke', {frameMode = 'random', rate = 14, prewarm = 4, lifetime = {4, 5}, speed = {90, 120}, spread = 0.2, turbulence = self:turbulence(), startSize = {50, 70}, endSize = {200, 260}, rotation = {0, m.tau}, spin = {-0.4, 0.4}, colors = {'#00D0D0D8', '#A0B0B0B8', '#00909098'}, maxParticles = 128, layer = 2, seed = 221}))
    self.town = Town()
    local house = TurbulenceAttractors.house
    self.smoke.position = {self.town:chimney('stone-house', house.x, TurbulenceAttractors.ground, house.scale)}
    self.coins = particles2d.newEmitter(ParticleTest.look('coin', {frameMode = 'loopRandomStart', frameRate = 12, rate = 5, lifetime = 4, speed = {450, 750}, spread = 1.4, damping = 1.2, startSize = 44, endSize = 44, attractors = self:attractors(), maxParticles = 256, layer = 3, seed = 222}))
    local chest = TurbulenceAttractors.chest
    self.coins.x, self.coins.y = chest.x, TurbulenceAttractors.ground - chest.size * 0.6
    self.emitters = {self.smoke, self.coins}
end

function TurbulenceAttractors:turbulence()
    return {strength = self.strength, frequency = self.frequency, speed = 0.6}
end

-- Returns the attractor list of the coins at the cursor, which the strength slider changes.
function TurbulenceAttractors:attractors()
    return {{x = self.cursorX, y = self.cursorY, strength = self.pull, radius = 2600, killRadius = TurbulenceAttractors.killRadius, space = 'world'}}
end

function TurbulenceAttractors:enter()
    self:frame{
        hint = 'The coins fly to the cursor and vanish inside its ring: move it with the mouse, a finger, the arrows, WASD or a stick, and a click, a tap, E, Enter or the south button throws a handful more. The smoke curls more as the turbulence grows.',
        code = TurbulenceAttractors.code,
        cursor = true,
        controls = {
            ui.formField{label = 'Turbulence strength', ui.slider{id = 'strength', min = 0, max = 800, value = self.strength, showValue = true, decimals = 0, onChange = function(event)
                self.strength = event.value
                self.smoke:configure({turbulence = self:turbulence()})
            end}},
            ui.formField{label = 'Turbulence frequency', ui.slider{id = 'frequency', min = 0.001, max = 0.02, value = self.frequency, showValue = true, decimals = 3, onChange = function(event)
                self.frequency = event.value
                self.smoke:configure({turbulence = self:turbulence()})
            end}},
            ui.formField{label = 'Attractor strength', ui.slider{id = 'pull', min = 0, max = 6000, value = self.pull, showValue = true, decimals = 0, onChange = function(event)
                self.pull = event.value
                self.coins:configure({attractors = self:attractors()})
            end}},
        },
    }
end

function TurbulenceAttractors:update(dt)
    TurbulenceAttractors.super.update(self, dt)
    self.coins:setAttractor(1, self.cursorX, self.cursorY)
    if self:pressed() then
        self.coins:burst(24)
    end
    ParticleTest.updateAll(self.emitters, dt)
    self:report('%d puffs curl in a turbulence of %.0f at a frequency of %.3f, and %d coins fly to the cursor with a pull of %.0f.', self.smoke.count, self.strength, self.frequency, self.coins.count, self.pull)
end

function TurbulenceAttractors:draw(area)
    local ground, house, chest = TurbulenceAttractors.ground, TurbulenceAttractors.house, TurbulenceAttractors.chest
    ParticleTest.backdrop({0.14, 0.2, 0.36}, {0.5, 0.42, 0.46})
    ParticleTest.paveFloor('grass', ground - 10, 140)
    self.town:house('stone-house', house.x, ground, house.scale, 1, 0)
    graphics2d.draw(assets.texture(ParticleTest.tiles, {filter = 'linear'}), chest.x, ground + 6, {source = chest.source, width = chest.size, height = chest.size, pivotY = 1, layer = 2})
    graphics2d.drawRing(self.cursorX, self.cursorY, TurbulenceAttractors.killRadius, 4, '#C0FFD860', TurbulenceAttractors.ring)
    ParticleTest.drawAll(self.emitters)
    ParticleTest.label('Smoke that curls', house.x, ground + 16, 'turbulence')
    ParticleTest.label('Coins pulled to the cursor', chest.x, ground + 16, 'attractors')
end

return TurbulenceAttractors
