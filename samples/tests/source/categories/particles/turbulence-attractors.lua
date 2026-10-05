-- Turbulence and attractors: `turbulence` pushes particles through moving noise so smoke curls, and an attractor pulls the particles inside its radius toward a point, harder the closer they get, and removes them at its kill radius, like coins that fly to a counter. The attractor lives in world space and `emitter:setAttractor` moves it to the cursor every frame.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local TurbulenceAttractors = haylen.class('TurbulenceAttractors', ParticleTest)

TurbulenceAttractors.killRadius = 40
TurbulenceAttractors.chest = {460, 360, 140, 90}
TurbulenceAttractors.chimney = {-560, 300, 120, 260}
TurbulenceAttractors.order = {layer = 1}
TurbulenceAttractors.ring = {layer = 4}

function TurbulenceAttractors:init(entry)
    TurbulenceAttractors.super.init(self, entry)
    self.strength = 260
    self.frequency = 0.004
    self.pull = 2800
    self.smoke = particles2d.newEmitter({texture = ParticleTest.library('smoke_sheet'), frameGrid = {columns = 4, rows = 1}, frameMode = 'random', rate = 14, prewarm = 4, lifetime = {4, 5}, speed = {90, 120}, spread = 0.2, turbulence = self:turbulence(), startSize = {50, 70}, endSize = {200, 260}, rotation = {0, m.tau}, spin = {-0.4, 0.4}, colors = {'#00D0D0D8', '#A0B0B0B8', '#00909098'}, maxParticles = 128, layer = 2, seed = 221})
    self.smoke.x, self.smoke.y = -500, 300
    self.coins = particles2d.newEmitter({texture = ParticleTest.library('coin_sheet'), frameGrid = {columns = 4, rows = 1}, frameMode = 'loopRandomStart', frameRate = 12, rate = 5, lifetime = 4, speed = {450, 750}, spread = 1.4, damping = 1.2, startSize = 44, endSize = 44, attractors = self:attractors(), maxParticles = 256, layer = 3, seed = 222})
    self.coins.x, self.coins.y = 530, 360
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
    self:report('Smoke %d with turbulence %.0f and frequency %.3f   Coins %d   Attractor at %.0f, %.0f with strength %.0f', self.smoke.count, self.strength, self.frequency, self.coins.count, self.cursorX, self.cursorY, self.pull)
end

function TurbulenceAttractors:draw(area)
    ParticleTest.backdrop({0.05, 0.06, 0.12}, {0.16, 0.14, 0.18})
    graphics2d.drawRect(TurbulenceAttractors.chimney, '#FF4A3A34', TurbulenceAttractors.order)
    graphics2d.drawRect(TurbulenceAttractors.chest, '#FF8A5A2A', TurbulenceAttractors.order)
    graphics2d.drawRing(self.cursorX, self.cursorY, TurbulenceAttractors.killRadius, 4, '#C0FFD860', TurbulenceAttractors.ring)
    ParticleTest.drawAll(self.emitters)
    ParticleTest.label('Smoke with "turbulence"', -500, 380)
    ParticleTest.label('Coins pulled to the cursor', 530, 460)
end

return TurbulenceAttractors
