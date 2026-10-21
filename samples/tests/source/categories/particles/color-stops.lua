-- Color stops and tints: colors spread evenly over the life by default, sit at the times of `colorTimes` when it is given and hold until the next stop with `colorBlend = 'steps'`, while `tints` give every new piece of confetti a color that multiplies its colors, picked at random, in order or by its moment in the emission cycle.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local particles2d = require('haylen.particles2d')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')

local ColorStops = haylen.class('ColorStops', ParticleTest)

ColorStops.code = [[
local colors = {'#FFFF4060', '#FFFFC040', '#FF40E080', '#FF40A0FF'}
local column = particles2d.newEmitter({texture = dot, lifetime = 3.2, colors = colors})
column:configure({colorTimes = {0, 0.6, 0.8, 1}})  -- Where each color sits.
column:configure({colorBlend = 'steps'})  -- Each color holds until the next stop.
local confetti = particles2d.newEmitter({texture = strip, tints = {'#FFFF4060', '#FFFFD030'}})
confetti:configure({tintMode = 'cycleTime'})  -- Or "random", or "cycle" in order.]]

ColorStops.colors = {'#FFFF4060', '#FFFFC040', '#FF40E080', '#FF40A0FF'}
ColorStops.tints = {'#FFFF4060', '#FFFFD030', '#FF40D080', '#FF40A0FF', '#FFC060FF'}
ColorStops.tintModes = {{id = 'random', text = 'Random'}, {id = 'cycle', text = 'In order'}, {id = 'cycleTime', text = 'By cycle time'}}
ColorStops.base = 360
ColorStops.floor = 400
ColorStops.captionY = 430
ColorStops.rise = 560
ColorStops.tick = {layer = 4}

function ColorStops:init(entry)
    ColorStops.super.init(self, entry)
    self.stop = 0.6
    self.tintMode = 'random'
    local dot = 'soft-dot'
    local column = ParticleTest.look(dot, {rate = 40, lifetime = 3.2, speed = ColorStops.rise / 3.2, spread = 0, startSize = 60, endSize = 60, colors = ColorStops.colors, layer = 2})
    self.columns = {}
    for index, spec in ipairs({{label = 'Spread evenly', option = 'No colorTimes'}, {label = 'At their times', option = 'colorTimes', times = true}, {label = 'Held in steps', option = "colorBlend = 'steps'", times = true, steps = true}}) do
        column.seed = 190 + index
        column.colorBlend = spec.steps and 'steps' or 'smooth'
        local emitter = particles2d.newEmitter(column)
        emitter.x, emitter.y = -820 + (index - 1) * 380, ColorStops.base
        self.columns[index] = {label = spec.label, option = spec.option, times = spec.times, emitter = emitter}
    end
    self.emitters = {self.columns[1].emitter, self.columns[2].emitter, self.columns[3].emitter}
    self:placeStops()

    self.confetti = particles2d.newEmitter(ParticleTest.look('confetti', {frameMode = 'loopRandomStart', frameRate = 12, rate = 70, duration = 2.5, loop = true, lifetime = {2.6, 3.4}, speed = {700, 1000}, direction = -1.05, spread = 0.45, gravity = {0, 560}, damping = 1.4, startSize = {22, 30}, endSize = {22, 30}, spin = {-5, 5}, colors = {'#FFFFFFFF', '#FFFFFFFF', '#00FFFFFF'}, colorTimes = {0, 0.85, 1}, tints = ColorStops.tints, tintMode = self.tintMode, collision = {type = 'floor', y = ColorStops.floor - 246, bounce = 0.2, friction = 0.6}, shape = 'cone', shapeSize = {20, 0}, maxParticles = 512, layer = 3, seed = 194}))
    self.confetti.x, self.confetti.y = 578, 246
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
        code = ColorStops.code,
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
    self:report('The second stop sits at %.2f of the life, %d particles rise, and %d pieces of confetti take their tints by "%s".', self.stop, columns, self.confetti.count, self.tintMode)
end

function ColorStops:draw(area)
    ParticleTest.backdrop({0.06, 0.06, 0.12}, {0.14, 0.1, 0.2})
    ParticleTest.paveFloor('stone', ColorStops.floor, 140, '#FF6A6280')
    ParticleTest.prop('cannon', 520, ColorStops.floor + 6, 0.5, {pivotY = 1, layer = 4})
    ParticleTest.drawAll(self.emitters)
    local second = ColorStops.base - self.stop * ColorStops.rise
    local third = ColorStops.base - (self.stop + (1 - self.stop) / 2) * ColorStops.rise
    for _, column in ipairs(self.columns) do
        local x = column.emitter.x
        if column.times then
            for _, stop in ipairs({{second, self.stop}, {third, self.stop + (1 - self.stop) / 2}}) do
                graphics2d.drawLine(x - 80, stop[1], x - 46, stop[1], 4, '#E0FFFFFF', ColorStops.tick)
                graphics2d.drawText(nil, string.format('%.2f', stop[2]), x - 90, stop[1], {size = 26, anchor = {1, 0.5}, outlineWidth = 3, layer = 4})
            end
        end
        ParticleTest.label(column.label, x, ColorStops.captionY, column.option)
    end
    ParticleTest.label('Tinted confetti', self.confetti.x, ColorStops.captionY, "tintMode = '" .. self.tintMode .. "'")
end

return ColorStops
