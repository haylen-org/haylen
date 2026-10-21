-- Rotation, stretch and frames: particles are born at a random `rotation`, snap their drawn angle to `rotationStep`, follow their motion with `alignToVelocity` and lengthen by their speed with `stretch`, take their width from `aspect`, and show the frames that `frameGrid` cuts from a sheet in one of the four `frameMode` values.
local haylen = require('haylen')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local RotationFrames = haylen.class('RotationFrames', ParticleTest)

RotationFrames.code = [[
local shards = particles2d.newEmitter({texture = shard, rotation = {0, 6.2832}, spin = {-1.6, 1.6}})
shards:configure({rotationStep = 0.7854})  -- Draws them at eighths of a turn.
local streaks = particles2d.newEmitter({texture = streak, alignToVelocity = true, stretch = 0.03})
local leaves = particles2d.newEmitter({texture = atlas.texture, frames = leaf, aspect = {0.45, 2.2}})
local runes = particles2d.newEmitter({texture = runeSheet, frameGrid = {columns = 4, rows = 1}})
runes:configure({frameMode = 'loop', frameRate = 3})  -- Or "overLife", "loopRandomStart" or "random".]]

RotationFrames.frameModes = {
    {mode = 'overLife', title = 'Once over its life'},
    {mode = 'loop', title = 'Loops'},
    {mode = 'loopRandomStart', title = 'Loops from any frame'},
    {mode = 'random', title = 'A random frame'},
}
RotationFrames.columns = {-720, -240, 240, 720}
RotationFrames.rows = {-60, 420}

function RotationFrames:init(entry)
    RotationFrames.super.init(self, entry)
    self.rotationStep = 0.7854
    self.stretch = 0.03
    self.frameRate = 3
    local columns = RotationFrames.columns
    local violet = {'#00D0A0FF', '#FFD0A0FF', '#FFD0A0FF', '#00D0A0FF'}
    local fadeTimes = {0, 0.15, 0.8, 1}

    self.random = particles2d.newEmitter(ParticleTest.look('star-5point', {rate = 4, lifetime = 3, speed = {10, 30}, spread = m.tau, startSize = 64, endSize = 64, rotation = {0, m.tau}, colors = {'#00FFD860', '#FFFFD860', '#FFFFD860', '#00FFD860'}, colorTimes = fadeTimes, shape = 'circle', shapeSize = {110, 0}, layer = 2, seed = 211}))
    self.stepped = particles2d.newEmitter(ParticleTest.look('shard', {rate = 4, lifetime = 3, speed = {10, 30}, spread = m.tau, startSize = 72, endSize = 72, rotation = {0, m.tau}, spin = {-1.6, 1.6}, rotationStep = self.rotationStep, colors = {'#00A0E0FF', '#FFA0E0FF', '#FFA0E0FF', '#00A0E0FF'}, colorTimes = fadeTimes, shape = 'circle', shapeSize = {110, 0}, layer = 2, seed = 212}))
    self.streaks = particles2d.newEmitter(ParticleTest.look('spark-streak', {rate = 60, lifetime = 1.1, speed = {560, 820}, spread = 0.8, gravity = {0, 1100}, startSize = {30, 38}, endSize = 14, alignToVelocity = true, stretch = self.stretch, colors = {'#FFFFFFFF', '#FFFFE070', '#C0FF9030', '#00FF5010'}, blend = 'additive', maxParticles = 256, layer = 2, seed = 213}))
    self.glow = particles2d.newEmitter(ParticleTest.look('soft-dot', {rate = 24, lifetime = 0.3, speed = 0, startSize = {90, 110}, endSize = 60, colors = {'#80FFD070', '#00FF9030'}, blend = 'additive', layer = 3, seed = 218}))
    self.aspects = particles2d.newEmitter(ParticleTest.look('leaf', {frameMode = 'random', rate = 6, lifetime = 3, speed = {10, 30}, spread = m.tau, startSize = 56, endSize = 56, rotation = {0, m.tau}, spin = {-0.8, 0.8}, aspect = {0.45, 2.2}, colors = {'#00F8D068', '#FFF8C860', '#FFF09848', '#00E87838'}, colorTimes = fadeTimes, shape = 'circle', shapeSize = {110, 0}, layer = 2, seed = 214}))
    self.random.x, self.random.y = columns[1], -250
    self.stepped.x, self.stepped.y = columns[2], -250
    self.streaks.x, self.streaks.y = columns[3], -110
    self.glow.x, self.glow.y = columns[3], -110
    self.aspects.x, self.aspects.y = columns[4], -250
    self.emitters = {self.random, self.stepped, self.streaks, self.glow, self.aspects}
    self.labels = {
        {'Random turns', 'rotation'},
        {'Snapped turns', 'rotationStep'},
        {'Stretched streaks', 'alignToVelocity, stretch'},
        {'Leaves of many widths', 'aspect'},
    }

    local runes = ParticleTest.library('rune-sheet')
    self.sheets = {}
    for index, frames in ipairs(RotationFrames.frameModes) do
        local emitter = particles2d.newEmitter({texture = runes, frameGrid = {columns = 4, rows = 1}, frameMode = frames.mode, frameRate = self.frameRate, rate = 2.5, lifetime = 4, speed = {40, 60}, spread = 0.2, startSize = 64, endSize = 64, colors = violet, colorTimes = fadeTimes, shape = 'rectangle', shapeSize = {90, 0}, layer = 2, seed = 214 + index})
        emitter.x, emitter.y = columns[index], 380
        self.sheets[index] = emitter
        self.emitters[#self.emitters + 1] = emitter
        self.labels[#self.labels + 1] = {frames.title, string.format("frameMode = '%s'", frames.mode)}
    end
end

function RotationFrames:enter()
    self:frame{
        hint = 'The top row turns and shapes its particles, and the bottom row cuts a sheet of four runes with "frameGrid" and plays it in each frame mode. Frame rates only change the two looping modes.',
        code = RotationFrames.code,
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
    self:report('%d particles turn in steps of %.2f radians, stretch by %.3f of their speed and play their runes at %.1f frames per second.', count, self.rotationStep, self.stretch, self.frameRate)
end

function RotationFrames:draw(area)
    ParticleTest.backdrop({0.04, 0.05, 0.1}, {0.1, 0.08, 0.18})
    ParticleTest.drawAll(self.emitters)
    for index, label in ipairs(self.labels) do
        local x = RotationFrames.columns[(index - 1) % 4 + 1]
        ParticleTest.label(label[1], x, RotationFrames.rows[index <= 4 and 1 or 2], label[2])
    end
end

return RotationFrames
