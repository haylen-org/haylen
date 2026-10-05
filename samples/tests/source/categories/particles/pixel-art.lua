-- Pixel art effects: `pixelSnap` rounds every drawn position to the grid of the art, `rotationStep` turns particles only by quarter turns and `colorBlend = 'steps'` holds each color of the palette instead of blending into colors the palette lacks. The left half uses them and the right half draws the same effects without them.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local PixelArt = haylen.class('PixelArt', ParticleTest)

PixelArt.pixel = 5
PixelArt.fire = {'#FFFFF8C0', '#FFFFC040', '#FFF06020', '#FF902828', '#00402018'}
PixelArt.smoke = {'#00A8A8B8', '#C0A8A8B8', '#C0707080', '#00484858'}
PixelArt.divider = {-3, -1000, 6, 2000}
PixelArt.order = {layer = 1}
PixelArt.interval = 2.5

-- Returns the effects of the left half with their places, which the right half mirrors.
function PixelArt.effects()
    local pixel = PixelArt.pixel
    return {
        {x = -480, y = -500, options = {texture = ParticleTest.library('pixel_leaf_sheet'), frameGrid = {columns = 4, rows = 1}, frameMode = 'random', rate = 5, prewarm = 4, lifetime = 4, speed = {60, 100}, direction = 1.5708, spread = 0.4, startSize = 8 * pixel, endSize = 8 * pixel, rotation = {0, m.tau}, spin = {-2.5, 2.5}, colors = {'#00FFFFFF', '#FFFFFFFF', '#FFFFFFFF', '#00FFFFFF'}, colorTimes = {0, 0.1, 0.85, 1}, shape = 'rectangle', shapeSize = {400, 0}, layer = 3}},
        {x = -560, y = -80, explosion = true, options = {texture = ParticleTest.library('pixel_explosion_sheet'), frameGrid = {columns = 4, rows = 1}, rate = 0, bursts = {{time = 0, count = {6, 9}}}, duration = 0.1, lifetime = {0.6, 0.9}, speed = {40, 160}, spread = m.tau, startSize = 32 * pixel, endSize = 32 * pixel, rotation = {0, m.tau}, colors = PixelArt.fire, shape = 'circle', shapeSize = {10 * pixel, 0}, layer = 3}},
        {x = -780, y = 330, options = {texture = ParticleTest.library('pixel_coin_sheet'), frameGrid = {columns = 4, rows = 1}, frameMode = 'loop', frameRate = 8, rate = 4, lifetime = 1.6, speed = {420, 520}, spread = 0.5, gravity = {0, 700}, startSize = 8 * pixel, endSize = 8 * pixel, rotation = {-0.4, 0.4}, colors = {'#FFFFFFFF', '#FFFFFFFF', '#00FFFFFF'}, colorTimes = {0, 0.8, 1}, layer = 3}},
        {x = -260, y = 240, options = {texture = ParticleTest.library('pixel_smoke_sheet'), frameGrid = {columns = 4, rows = 1}, rate = 5, lifetime = {1.6, 2.2}, speed = {50, 80}, spread = 0.4, startSize = 8 * pixel, endSize = 16 * pixel, colors = PixelArt.smoke, layer = 2}},
        {x = -260, y = 330, options = {texture = ParticleTest.library('pixel_flame_sheet'), frameGrid = {columns = 4, rows = 1}, frameMode = 'loopRandomStart', frameRate = 8, rate = 18, lifetime = {0.8, 1.1}, speed = {70, 110}, spread = 0.3, startSize = 16 * pixel, endSize = 6 * pixel, colors = PixelArt.fire, shape = 'rectangle', shapeSize = {4 * pixel, 0}, layer = 3}},
    }
end

function PixelArt:init(entry)
    PixelArt.super.init(self, entry)
    self.snap = PixelArt.pixel
    self.quarterTurns = true
    self.steps = true
    self.clock = 0
    self.snapped, self.smooth, self.explosions, self.emitters = {}, {}, {}, {}
    for index, effect in ipairs(PixelArt.effects()) do
        for side, list in ipairs({self.snapped, self.smooth}) do
            effect.options.seed = 330 + index * 2 + side
            local emitter = particles2d.newEmitter(effect.options)
            emitter.x, emitter.y = side == 1 and effect.x or -effect.x, effect.y
            list[index] = emitter
            self.emitters[#self.emitters + 1] = emitter
            if effect.explosion then
                self.explosions[side] = emitter
            end
        end
    end
    self:pixelate()
end

-- Gives the left half the pixel art keys of the controls, while the right half keeps the defaults.
function PixelArt:pixelate()
    local keys = {pixelSnap = self.snap, rotationStep = self.quarterTurns and 1.5708 or 0, colorBlend = self.steps and 'steps' or 'smooth'}
    for _, emitter in ipairs(self.snapped) do
        emitter:configure(keys)
    end
end

function PixelArt:explode()
    self.clock = 0
    for _, emitter in ipairs(self.explosions) do
        emitter:restart()
    end
end

function PixelArt:enter()
    self:frame{
        hint = 'Every effect of the left half snaps to a grid of 5 units, the size of one pixel of its art, turns by quarter turns and holds the colors of its palette. A click, a tap, E, Enter, the south button or the button of the panel sets off both explosions.',
        cursor = true,
        controls = {
            ui.button{id = 'explode', text = 'Set off the explosions', onClick = function()
                self:explode()
            end},
            ui.formField{label = 'Pixel snap of the left half', ui.slider{id = 'snap', min = 0, max = 20, step = 1, value = self.snap, showValue = true, decimals = 0, onChange = function(event)
                self.snap = event.value
                self:pixelate()
            end}},
            ui.toggle{id = 'quarterTurns', text = 'Quarter turns on the left', checked = self.quarterTurns, onChange = function(event)
                self.quarterTurns = event.checked
                self:pixelate()
            end},
            ui.toggle{id = 'steps', text = 'Palette steps on the left', checked = self.steps, onChange = function(event)
                self.steps = event.checked
                self:pixelate()
            end},
        },
    }
end

function PixelArt:update(dt)
    PixelArt.super.update(self, dt)
    self.clock = self.clock + dt
    if self:pressed() or self.clock > PixelArt.interval then
        self:explode()
    end
    local left = ParticleTest.updateAll(self.snapped, dt)
    local right = ParticleTest.updateAll(self.smooth, dt)
    self:report('Pixel snap %.0f   Quarter turns "%s"   Palette steps "%s"   Particles %d on the left and %d on the right', self.snap, self.quarterTurns, self.steps, left, right)
end

function PixelArt:draw(area)
    ParticleTest.backdrop({0.08, 0.06, 0.16}, {0.16, 0.1, 0.2})
    graphics2d.drawRect(PixelArt.divider, '#FF404060', PixelArt.order)
    ParticleTest.drawAll(self.emitters)
    ParticleTest.label('With "pixelSnap", "rotationStep" and "steps"', -480, 440)
    ParticleTest.label('Without them', 480, 440)
end

return PixelArt
