-- Rotation, stretch and frames: particles are born at a random `rotation`, snap their drawn angle to `rotationStep`, follow their motion with `alignToVelocity` and lengthen by their speed with `stretch`, take their width from `aspect`, and show the frames that `frameGrid` cuts from a sheet in one of the four `frameMode` values.
local haylen = require('haylen')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local RotationFrames = haylen.class('RotationFrames', ParticleTest)

RotationFrames.frameModes = {'overLife', 'loop', 'loopRandomStart', 'random'}
RotationFrames.columns = {-720, -240, 240, 720}

function RotationFrames:init(entry)
    RotationFrames.super.init(self, entry)
    self.rotationStep = 0.7854
    self.stretch = 0.03
    self.frameRate = 3
    local columns = RotationFrames.columns
    local violet = {'#00D0A0FF', '#FFD0A0FF', '#FFD0A0FF', '#00D0A0FF'}
    local fadeTimes = {0, 0.15, 0.8, 1}

    self.random = particles2d.newEmitter({texture = ParticleTest.library('star_5point'), rate = 4, lifetime = 3, speed = {10, 30}, spread = m.tau, startSize = 64, endSize = 64, rotation = {0, m.tau}, colors = {'#00FFD860', '#FFFFD860', '#FFFFD860', '#00FFD860'}, colorTimes = fadeTimes, shape = 'circle', shapeSize = {110, 0}, layer = 2, seed = 211})
    self.stepped = particles2d.newEmitter({texture = ParticleTest.library('shard'), rate = 4, lifetime = 3, speed = {10, 30}, spread = m.tau, startSize = 72, endSize = 72, rotation = {0, m.tau}, spin = {-1.6, 1.6}, rotationStep = self.rotationStep, colors = {'#00A0E0FF', '#FFA0E0FF', '#FFA0E0FF', '#00A0E0FF'}, colorTimes = fadeTimes, shape = 'circle', shapeSize = {110, 0}, layer = 2, seed = 212})
    self.streaks = particles2d.newEmitter({texture = ParticleTest.library('spark_streak'), rate = 70, lifetime = 1.2, speed = {520, 800}, spread = 0.9, gravity = {0, 900}, startSize = 18, endSize = 8, alignToVelocity = true, stretch = self.stretch, colors = {'#FFFFFFE0', '#FFFFB040', '#00FF6020'}, blend = 'additive', maxParticles = 256, layer = 2, seed = 213})
    self.aspects = particles2d.newEmitter({texture = ParticleTest.library('hard_dot'), rate = 6, lifetime = 3, speed = {10, 30}, spread = m.tau, startSize = 40, endSize = 40, aspect = {0.3, 3}, colors = {'#0060F0C0', '#FF60F0C0', '#FF60F0C0', '#0060F0C0'}, colorTimes = fadeTimes, shape = 'circle', shapeSize = {110, 0}, layer = 2, seed = 214})
    self.random.x, self.random.y = columns[1], -250
    self.stepped.x, self.stepped.y = columns[2], -250
    self.streaks.x, self.streaks.y = columns[3], -100
    self.aspects.x, self.aspects.y = columns[4], -250
    self.emitters = {self.random, self.stepped, self.streaks, self.aspects}
    self.labels = {'Random "rotation"', 'Snapped by "rotationStep"', 'Along the motion, stretched', 'Widths from "aspect"'}

    local runes = ParticleTest.library('rune_sheet')
    self.sheets = {}
    for index, mode in ipairs(RotationFrames.frameModes) do
        local emitter = particles2d.newEmitter({texture = runes, frameGrid = {columns = 4, rows = 1}, frameMode = mode, frameRate = self.frameRate, rate = 2.5, lifetime = 4, speed = {40, 60}, spread = 0.2, startSize = 64, endSize = 64, colors = violet, colorTimes = fadeTimes, shape = 'rectangle', shapeSize = {90, 0}, layer = 2, seed = 214 + index})
        emitter.x, emitter.y = columns[index], 380
        self.sheets[index] = emitter
        self.emitters[#self.emitters + 1] = emitter
        self.labels[#self.labels + 1] = 'Frame mode "' .. mode .. '"'
    end
end

function RotationFrames:enter()
    self:frame{
        hint = 'The top row turns and shapes its particles, and the bottom row cuts a sheet of four runes with "frameGrid" and plays it in each frame mode. Frame rates only change the two looping modes.',
        controls = {
            ui.formField{label = 'Rotation step in radians', ui.slider{id = 'rotationStep', min = 0, max = 1.5708, value = self.rotationStep, showValue = true, onChange = function(event)
                self.rotationStep = event.value
                self.stepped:configure({rotationStep = event.value})
            end}},
            ui.formField{label = 'Stretch by speed', ui.slider{id = 'stretch', min = 0, max = 0.08, value = self.stretch, showValue = true, decimals = 3, onChange = function(event)
                self.stretch = event.value
                self.streaks:configure({stretch = event.value})
            end}},
            ui.formField{label = 'Frame rate of the runes', ui.slider{id = 'frameRate', min = 0.5, max = 12, value = self.frameRate, showValue = true, decimals = 1, onChange = function(event)
                self.frameRate = event.value
                for _, emitter in ipairs(self.sheets) do
                    emitter:configure({frameRate = event.value})
                end
            end}},
        },
        focus = 'rotationStep',
    }
end

function RotationFrames:update(dt)
    RotationFrames.super.update(self, dt)
    local count = ParticleTest.updateAll(self.emitters, dt)
    self:report('Rotation step %.2f   Stretch %.3f   Frame rate %.1f   Particles %d', self.rotationStep, self.stretch, self.frameRate, count)
end

function RotationFrames:draw(area)
    ParticleTest.backdrop({0.04, 0.05, 0.1}, {0.1, 0.08, 0.18})
    ParticleTest.drawAll(self.emitters)
    for index, text in ipairs(self.labels) do
        local x = RotationFrames.columns[(index - 1) % 4 + 1]
        ParticleTest.label(text, x, index <= 4 and -60 or 440)
    end
end

return RotationFrames
