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
local smoke = particles2d.newEmitter({texture = puff, rate = 14, gravity = {20, -70}})  -- Warm smoke rises.
smoke:configure({turbulence = {strength = 180, frequency = 0.003, speed = 0.6}})
local coins = particles2d.newEmitter({texture = coin, rate = 5, damping = 1.2})
coins:configure({attractors = {{x = 0, y = 0, strength = 2800, radius = 2600, killRadius = 40, space = 'world'}}})
coins:setAttractor(1, cursorX, cursorY)  -- Every frame, in the space of the attractor.
coins:burst(24)]]

TurbulenceAttractors.killRadius = 40
TurbulenceAttractors.ground = 400
TurbulenceAttractors.house = {x = -560, scale = 0.62}
-- The chest of the terrain tileset, which the coins jump out of.
TurbulenceAttractors.chest = {x = 530, size = 170, source = {662, 266, 128, 128}}

function TurbulenceAttractors:init(entry)
    TurbulenceAttractors.super.init(self, entry)
    self.strength = 180
    self.frequency = 0.003
    self.caught = 0
    self.pull = 2800
    self.smoke = particles2d.newEmitter(ParticleTest.look('smoke', {frameMode = 'random', rate = 14, prewarm = 4, lifetime = {4, 5}, speed = {90, 120}, spread = 0.2, gravity = {20, -70}, turbulence = self:turbulence(), startSize = {50, 70}, endSize = {200, 260}, rotation = {0, m.tau}, spin = {-0.4, 0.4}, colors = {'#00D0D0D8', '#A0B0B0B8', '#00909098'}, maxParticles = 128, layer = 2, seed = 221}))
    self.town = Town()
    local house = TurbulenceAttractors.house
    self.smoke.position = {self.town:chimney('stone-house', house.x, TurbulenceAttractors.ground, house.scale)}
    self.coins = particles2d.newEmitter(ParticleTest.look('coin', {frameMode = 'loopRandomStart', frameRate = 12, rate = 5, lifetime = 4, speed = {450, 750}, spread = 1.4, damping = 1.2, startSize = 44, endSize = 44, attractors = self:attractors(), maxParticles = 256, layer = 3, seed = 222}))
    local chest = TurbulenceAttractors.chest
    self.coins.x, self.coins.y = chest.x, TurbulenceAttractors.ground - chest.size * 0.6
    self.coinLook = ParticleTest.look('coin', {})
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
        hint = 'The coins fly into the counter at the cursor: move it with the mouse, a finger, the arrows, WASD or a stick, and a click, a tap, E, Enter or the south button throws a handful more. The smoke curls more as the turbulence grows.',
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
    -- The coins that vanish while the attractor pulls are the ones its kill radius took in.
    local flying = self.coins.count
    ParticleTest.updateAll(self.emitters, dt)
    if self.pull > 0 then
        self.caught = self.caught + math.max(0, flying - self.coins.count)
    end
    self:report('%d puffs curl in a turbulence of %.0f, and %d coins fly to the counter, which caught %d with a pull of %.0f.', self.smoke.count, self.strength, self.coins.count, self.caught, self.pull)
end

function TurbulenceAttractors:draw(area)
    local ground, house, chest = TurbulenceAttractors.ground, TurbulenceAttractors.house, TurbulenceAttractors.chest
    ParticleTest.backdrop({0.14, 0.2, 0.36}, {0.5, 0.42, 0.46})
    ParticleTest.paveFloor('grass', ground - 10, 140)
    self.town:house('stone-house', house.x, ground, house.scale, 1, 0)
    graphics2d.draw(assets.texture(ParticleTest.tiles, {filter = 'linear'}), chest.x, ground + 6, {source = chest.source, width = chest.size, height = chest.size, pivotY = 1, layer = 2})
    self:drawCounter()
    ParticleTest.drawAll(self.emitters)
    ParticleTest.label('Smoke that curls', house.x, ground + 16, 'turbulence')
    ParticleTest.label('Coins pulled to the cursor', chest.x, ground + 16, 'attractors')
end

-- Draws the coin counter at the cursor as a chip of the interface, whose coin covers the kill radius of the attractor, with the number of coins it caught.
function TurbulenceAttractors:drawCounter()
    local x, y = self.cursorX, self.cursorY
    local text = tostring(self.caught)
    local width = graphics2d.measureText(nil, text, {size = 36}) + 112
    graphics2d.drawShape({x - 46, y - 34, width, 68}, {radius = 34, color = '#E02F3352', borderWidth = 4, borderColor = '#FF15172A', layer = 4})
    graphics2d.draw(self.coinLook.texture, x, y, {source = self.coinLook.frames[1], width = 56, height = 56, layer = 5})
    graphics2d.drawText(nil, text, x + 42, y, {size = 36, anchor = {0, 0.5}, color = '#FFFFE08A', outlineWidth = 3, layer = 5})
end

return TurbulenceAttractors
