-- Lights and shading: in a lit canvas an emitter draws its `light` at its position, here wavering with `flicker`, and `particleLights` draws a small light in the color of some of its particles, while `emission` makes particles glow whatever the light and `unshaded` keeps their colors in the dark. An unlit canvas draws the same emitters without any light.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local LightsShading = haylen.class('LightsShading', ParticleTest)

LightsShading.night = {ambientLight = '#FF141828'}
LightsShading.campfire = {-160, 340}
LightsShading.ground = {-1000, 360, 2000, 300}
LightsShading.stones = {{560, 300, 70}, {760, 320, 60}, {660, 250, 55}}
LightsShading.crowns = {{-820, 80, 120}, {-560, 140, 100}, {280, 110, 110}}
LightsShading.trunks = {{-834, 190, 28, 170}, {-574, 230, 28, 130}, {266, 210, 28, 150}}
LightsShading.scenery = {layer = 1}

function LightsShading:init(entry)
    LightsShading.super.init(self, entry)
    self.lit = true
    self.flicker = 0.3
    self.emission = 1
    self.unshaded = true
    local x, y = LightsShading.campfire[1], LightsShading.campfire[2]

    self.flames = particles2d.newEmitter({texture = ParticleTest.library('flame_sheet'), frameGrid = {columns = 4, rows = 1}, frameMode = 'loopRandomStart', frameRate = 10, rate = 40, lifetime = {0.6, 0.9}, speed = {80, 140}, spread = 0.3, gravity = {0, -120}, startSize = {80, 110}, endSize = {30, 50}, colors = {'#FFFFF0C0', '#FFFFA040', '#C0FF5020', '#00801000'}, shape = 'circle', shapeSize = {30, 0}, light = self:fireLight(), emission = self.emission, blend = 'additive', maxParticles = 64, layer = 3, seed = 311})
    self.embers = particles2d.newEmitter({texture = ParticleTest.library('hard_dot'), rate = 16, lifetime = {1.4, 2.2}, speed = {140, 260}, spread = 0.8, gravity = {0, -60}, turbulence = {strength = 140, frequency = 0.01, speed = 1}, startSize = {6, 9}, endSize = 2, colors = {'#FFFFE080', '#FFFF8020', '#00FF3000'}, particleLights = {radius = 50, intensity = 0.8, max = 24}, emission = 1.5, blend = 'additive', maxParticles = 64, layer = 4, seed = 312})
    for _, emitter in ipairs({self.flames, self.embers}) do
        emitter.x, emitter.y = x, y - 20
    end
    self.fireflies = particles2d.newEmitter({texture = ParticleTest.library('soft_dot'), rate = 6, prewarm = 6, lifetime = {5, 7}, speed = {10, 30}, spread = m.tau, turbulence = {strength = 90, frequency = 0.006, speed = 0.8}, startSize = {12, 18}, endSize = {12, 18}, colors = {'#00C0FF60', '#FFC0FF60', '#40C0FF60', '#FFC0FF60', '#00C0FF60'}, colorTimes = {0, 0.2, 0.5, 0.8, 1}, shape = 'rectangle', shapeSize = {300, 160}, particleLights = {radius = 90, intensity = 0.7, max = 32}, emission = 2, blend = 'additive', maxParticles = 64, layer = 4, seed = 313})
    self.fireflies.x, self.fireflies.y = -640, 80
    self.runes = particles2d.newEmitter({texture = ParticleTest.library('rune_sheet'), frameGrid = {columns = 4, rows = 1}, frameMode = 'random', rate = 3, prewarm = 3, lifetime = 3, speed = {30, 50}, spread = 0.3, startSize = 54, endSize = 54, colors = {'#0080C0FF', '#FF80C0FF', '#FF80C0FF', '#0080C0FF'}, colorTimes = {0, 0.2, 0.8, 1}, shape = 'rectangle', shapeSize = {120, 10}, unshaded = self.unshaded, maxParticles = 32, layer = 3, seed = 314})
    self.runes.x, self.runes.y = 660, 220
    self.emitters = {self.flames, self.embers, self.fireflies, self.runes}
end

function LightsShading:fireLight()
    return {radius = 520, color = '#FFFFB060', intensity = 1.4, offset = {0, -40}, flicker = {speed = 7, amount = self.flicker}}
end

function LightsShading:enter()
    self:frame{
        hint = 'The fire lights the clearing and flickers, the embers and the fireflies carry small lights, the flames glow by their emission and the runes over the stones keep their colors because they are unshaded. Switch to the unlit canvas to see the same emitters without light.',
        controls = {
            ui.toggle{id = 'lit', text = 'Lit canvas', checked = self.lit, onChange = function(event)
                self.lit = event.checked
            end},
            ui.formField{label = 'Flicker of the fire light', ui.slider{id = 'flicker', min = 0, max = 1, value = self.flicker, showValue = true, onChange = function(event)
                self.flicker = event.value
                self.flames:configure({light = self:fireLight()})
            end}},
            ui.formField{label = 'Emission of the flames', ui.slider{id = 'emission', min = 0, max = 3, value = self.emission, showValue = true, onChange = function(event)
                self.emission = event.value
                self.flames:configure({emission = event.value})
            end}},
            ui.toggle{id = 'unshaded', text = 'Unshaded runes', checked = self.unshaded, onChange = function(event)
                self.unshaded = event.checked
                self.runes:configure({unshaded = event.checked})
            end},
        },
        focus = 'lit',
    }
end

function LightsShading:update(dt)
    LightsShading.super.update(self, dt)
    local count = ParticleTest.updateAll(self.emitters, dt)
    self:report('Canvas lit "%s"   Flicker %.2f   Flame emission %.2f   Unshaded runes "%s"   Particles %d', self.lit, self.flicker, self.emission, self.unshaded, count)
end

-- Draws the scene in a lit canvas or in an unlit one.
function LightsShading:render()
    if self.area then
        graphics2d.beginWorld(self.camera, self.lit and LightsShading.night or nil)
        self:draw(self.area)
    end
end

function LightsShading:draw(area)
    ParticleTest.backdrop({0.12, 0.16, 0.3}, {0.2, 0.26, 0.34})
    local order = LightsShading.scenery
    graphics2d.drawRect(LightsShading.ground, '#FF3A5A3A', order)
    for index, crown in ipairs(LightsShading.crowns) do
        graphics2d.drawRect(LightsShading.trunks[index], '#FF5A4030', order)
        graphics2d.drawCircle(crown[1], crown[2], crown[3], '#FF2E5A3A', order)
    end
    for _, stone in ipairs(LightsShading.stones) do
        graphics2d.drawCircle(stone[1], stone[2], stone[3], '#FF7A7A88', order)
    end
    ParticleTest.drawAll(self.emitters)
end

return LightsShading
