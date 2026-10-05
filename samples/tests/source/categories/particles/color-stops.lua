-- Color stops and tints: colors spread evenly over the life by default, sit at the times of `colorTimes` when it is given and hold until the next stop with `colorBlend = 'steps'`, while `tints` give every new piece of confetti a color that multiplies its colors, picked at random, in order or by its moment in the emission cycle.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local ColorStops = haylen.class('ColorStops', ParticleTest)

ColorStops.colors = {'#FFFF4060', '#FFFFC040', '#FF40E080', '#FF40A0FF'}
ColorStops.tints = {'#FFFF4060', '#FFFFD030', '#FF40D080', '#FF40A0FF', '#FFC060FF'}
ColorStops.tintModes = {{id = 'random', text = 'Random'}, {id = 'cycle', text = 'In order'}, {id = 'cycleTime', text = 'By cycle time'}}
ColorStops.base = 330
ColorStops.rise = 560
ColorStops.tick = {layer = 4}

function ColorStops:init(entry)
    ColorStops.super.init(self, entry)
    self.stop = 0.6
    self.tintMode = 'random'
    local dot = ParticleTest.library('soft_dot')
    local column = {texture = dot, rate = 22, lifetime = 3.2, speed = ColorStops.rise / 3.2, spread = 0, startSize = 46, endSize = 46, colors = ColorStops.colors, layer = 2}
    self.columns = {}
    for index, spec in ipairs({{label = 'Spread evenly'}, {label = 'At "colorTimes"', times = true}, {label = 'Held with "steps"', times = true, steps = true}}) do
        column.seed = 190 + index
        column.colorBlend = spec.steps and 'steps' or 'smooth'
        local emitter = particles2d.newEmitter(column)
        emitter.x, emitter.y = -760 + (index - 1) * 300, ColorStops.base
        self.columns[index] = {label = spec.label, times = spec.times, emitter = emitter}
    end
    self.emitters = {self.columns[1].emitter, self.columns[2].emitter, self.columns[3].emitter}
    self:placeStops()

    self.confetti = particles2d.newEmitter({texture = ParticleTest.library('confetti_sheet'), frameGrid = {columns = 4, rows = 1}, frameMode = 'loopRandomStart', frameRate = 12, rate = 70, duration = 2.5, loop = true, lifetime = {2.6, 3.4}, speed = {700, 1000}, spread = 0.6, gravity = {0, 560}, damping = 1.4, startSize = {22, 30}, endSize = {22, 30}, spin = {-5, 5}, colors = {'#FFFFFFFF', '#FFFFFFFF', '#00FFFFFF'}, colorTimes = {0, 0.85, 1}, tints = ColorStops.tints, tintMode = self.tintMode, shape = 'cone', shapeSize = {20, 0}, maxParticles = 512, layer = 3, seed = 194})
    self.confetti.x, self.confetti.y = 520, 420
    self.emitters[4] = self.confetti
end

-- Places the second stop at the time of the slider and the third halfway to the end, for the two columns that take `colorTimes`.
function ColorStops:placeStops()
    local times = {0, self.stop, self.stop + (1 - self.stop) / 2, 1}
    for _, column in ipairs(self.columns) do
        if column.times then
            column.emitter:configure({colorTimes = times})
        end
    end
end

function ColorStops:enter()
    self:frame{
        hint = 'The columns rise with the same four colors over a life of 3.2 seconds, and the marks show where the stops sit. The confetti cannon tints every new piece with one of five colors.',
        controls = {
            ui.formField{label = 'Time of the second color stop', ui.slider{id = 'stop', min = 0.05, max = 0.9, value = self.stop, showValue = true, onChange = function(event)
                self.stop = event.value
                self:placeStops()
            end}},
            ui.formField{label = 'Tint mode of the confetti', ui.radioGroup{id = 'tintMode', items = ColorStops.tintModes, selected = self.tintMode, onChange = function(event)
                self.tintMode = event.value
                self.confetti:configure({tintMode = event.value})
            end}},
        },
        focus = 'stop',
    }
end

function ColorStops:update(dt)
    ColorStops.super.update(self, dt)
    ParticleTest.updateAll(self.emitters, dt)
    local columns = self.columns[1].emitter.count + self.columns[2].emitter.count + self.columns[3].emitter.count
    self:report('Second stop at %.2f   Tint mode "%s"   Particles in the columns %d   Confetti %d', self.stop, self.tintMode, columns, self.confetti.count)
end

function ColorStops:draw(area)
    ParticleTest.backdrop({0.06, 0.06, 0.12}, {0.14, 0.1, 0.2})
    ParticleTest.drawAll(self.emitters)
    local second = ColorStops.base - self.stop * ColorStops.rise
    local third = ColorStops.base - (self.stop + (1 - self.stop) / 2) * ColorStops.rise
    for _, column in ipairs(self.columns) do
        local x = column.emitter.x
        if column.times then
            graphics2d.drawLine(x - 70, second, x - 40, second, 3, '#C0FFFFFF', ColorStops.tick)
            graphics2d.drawLine(x - 70, third, x - 40, third, 3, '#C0FFFFFF', ColorStops.tick)
        end
        ParticleTest.label(column.label, x, ColorStops.base + 50)
    end
    ParticleTest.label('Tints of the confetti', self.confetti.x, 450)
end

return ColorStops
