-- Lights and shading: in a lit canvas an emitter draws its `light` at its position, here wavering with `flicker`, and `particleLights` draws a small light in the color of some of its particles, while `emission` makes particles glow whatever the light and `unshaded` keeps their colors in the dark. An unlit canvas draws the same emitters without any light.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local LightsShading = haylen.class('LightsShading', ParticleTest)

LightsShading.code = [[
local flames = particles2d.newEmitter({texture = flame, rate = 40, emission = 1})  -- Glow.
flames:configure({light = {radius = 620, color = '#FFFFB060', flicker = {speed = 7, amount = 0.3}}})
local embers = particles2d.newEmitter({texture = dot, rate = 16, emission = 1.5})
embers:configure({particleLights = {radius = 50, intensity = 0.8, max = 24}})
runes:configure({unshaded = true})  -- Keeps its colors in the dark.
graphics2d.beginWorld(camera, {ambientLight = '#FF202A40'})  -- Lights need a lit canvas.]]

LightsShading.night = {ambientLight = '#FF202A40'}
LightsShading.campfire = {-120, 330}
LightsShading.floor = 360
-- The trees and pines of the props tileset and the rocks of the terrain tileset that stand around the clearing.
LightsShading.trees = {{-1040, 1.3, 'tree'}, {-820, 1.6, 'pine'}, {-560, 1.25, 'tree'}, {300, 1.45, 'pine'}, {1000, 1.5, 'tree'}}
LightsShading.treeSources = {tree = {2, 2, 128, 256}, pine = {134, 2, 128, 256}}
LightsShading.rocks = {{560, 380, 150}, {760, 390, 120}, {660, 340, 110}}

function LightsShading:init(entry)
    LightsShading.super.init(self, entry)
    self.lit = true
    self.flicker = 0.3
    self.emission = 1
    self.unshaded = true
    local x, y = LightsShading.campfire[1], LightsShading.campfire[2]

    self.flames = particles2d.newEmitter(ParticleTest.look('flame', {frameMode = 'loopRandomStart', frameRate = 12, rate = 40, lifetime = {0.6, 0.9}, speed = {110, 180}, spread = 0.25, startSize = {120, 160}, endSize = {30, 44}, sizeCurve = 'quadIn', colors = {'#00FFE7A8', '#E0FFC24A', '#C0FF7A1F', '#60D8381F', '#00801010'}, colorTimes = {0, 0.12, 0.45, 0.8, 1}, shape = 'ellipse', shapeSize = {40, 10}, light = self:fireLight(), emission = self.emission, maxParticles = 64, layer = 3, seed = 311}))
    self.embers = particles2d.newEmitter(ParticleTest.look('hard-dot', {rate = 16, lifetime = {1.4, 2.2}, speed = {140, 260}, spread = 0.8, gravity = {0, -60}, turbulence = {strength = 140, frequency = 0.01, speed = 1}, startSize = {6, 9}, endSize = 2, colors = {'#FFFFE080', '#FFFF8020', '#00FF3000'}, particleLights = {radius = 50, intensity = 0.8, max = 24}, emission = 1.5, blend = 'additive', maxParticles = 64, layer = 4, seed = 312}))
    for _, emitter in ipairs({self.flames, self.embers}) do
        emitter.x, emitter.y = x, y - 20
    end
    self.fireflies = particles2d.newEmitter(ParticleTest.look('soft-dot', {rate = 6, prewarm = 6, lifetime = {5, 7}, speed = {10, 30}, spread = m.tau, turbulence = {strength = 90, frequency = 0.006, speed = 0.8}, startSize = {12, 18}, endSize = {12, 18}, colors = {'#00C0FF60', '#FFC0FF60', '#40C0FF60', '#FFC0FF60', '#00C0FF60'}, colorTimes = {0, 0.2, 0.5, 0.8, 1}, shape = 'rectangle', shapeSize = {300, 160}, particleLights = {radius = 90, intensity = 0.7, max = 32}, emission = 2, blend = 'additive', maxParticles = 64, layer = 4, seed = 313}))
    self.fireflies.x, self.fireflies.y = -640, 80
    self.runes = particles2d.newEmitter({texture = ParticleTest.library('rune-sheet'), frameGrid = {columns = 4, rows = 1}, frameMode = 'random', rate = 3, prewarm = 3, lifetime = 3, speed = {30, 50}, spread = 0.3, startSize = 54, endSize = 54, colors = {'#0080C0FF', '#FF80C0FF', '#FF80C0FF', '#0080C0FF'}, colorTimes = {0, 0.2, 0.8, 1}, shape = 'rectangle', shapeSize = {120, 10}, unshaded = self.unshaded, maxParticles = 32, layer = 3, seed = 314})
    self.runes.x, self.runes.y = 660, 240
    self.tiles = assets.texture('tiled/tiles/props.png', {filter = 'linear'})
    self.terrain = assets.texture(ParticleTest.tiles, {filter = 'linear'})
    self.emitters = {self.flames, self.embers, self.fireflies, self.runes}
end

function LightsShading:fireLight()
    return {radius = 620, color = '#FFFFB060', intensity = 1.5, offset = {0, -60}, flicker = {speed = 7, amount = self.flicker}}
end

function LightsShading:enter()
    self:frame{
        hint = 'The fire lights the clearing and flickers, the embers and the fireflies carry small lights, the flames glow by their emission and the runes over the stones keep their colors because they are unshaded. Switch to the unlit canvas to see the same emitters without light.',
        code = LightsShading.code,
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
    local canvas = self.lit and 'In the lit canvas the fire lights the clearing' or 'The unlit canvas draws without any light'
    self:report('%s, and %d particles glow.', canvas, count)
end

-- Draws the scene in a lit canvas or in an unlit one.
function LightsShading:render()
    if self.area then
        graphics2d.beginWorld(self.camera, self.lit and LightsShading.night or nil)
        self:draw(self.area)
    end
end

function LightsShading:draw(area)
    ParticleTest.backdrop({0.16, 0.22, 0.4}, {0.3, 0.36, 0.46})
    ParticleTest.paveFloor('grass', LightsShading.floor, 150)
    for _, tree in ipairs(LightsShading.trees) do
        local source = LightsShading.treeSources[tree[3]]
        graphics2d.draw(self.tiles, tree[1], LightsShading.floor + 30, {source = source, width = 128 * tree[2] * 1.4, height = 256 * tree[2] * 1.4, pivotY = 1, layer = 1})
    end
    for _, rock in ipairs(LightsShading.rocks) do
        graphics2d.draw(self.terrain, rock[1], rock[2], {source = {398, 266, 128, 128}, width = rock[3], height = rock[3], pivotY = 0.8, layer = 2})
    end
    ParticleTest.prop('campfire', LightsShading.campfire[1], LightsShading.campfire[2] + 50, 0.5, {pivotY = 1, layer = 2})
    ParticleTest.drawAll(self.emitters)
end

return LightsShading
