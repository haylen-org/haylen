-- Smoothing a noisy signal: a wave measured with noise and with spikes now and then, and the line each signal filter of `haylen.math` draws through it, so the delay, the smoothness and the handling of spikes of each one show side by side.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')

local Smoothing = haylen.class('Smoothing', AlgorithmTest)

Smoothing.samples = 360
Smoothing.rate = 60
Smoothing.width, Smoothing.height = 1500, 300
Smoothing.filters = {
    {id = 'lowPass', text = 'Low-pass', color = '#FF42A5F5'},
    {id = 'average', text = 'Moving average', color = '#FFFFD54F'},
    {id = 'median', text = 'Median', color = '#FFAB47BC'},
    {id = 'hampel', text = 'Hampel', color = '#FF66BB6A'},
    {id = 'kalman', text = 'Kalman', color = '#FFEF5350'},
}

function Smoothing:enter()
    self.noise, self.spikes, self.shown = 0.3, true, {lowPass = true, average = true, median = true, hampel = true, kalman = true}
    local toggles = {}
    for index, filter in ipairs(Smoothing.filters) do
        toggles[index] = ui.toggle{id = filter.id, text = filter.text, checked = true, onChange = function(event) self.shown[filter.id] = event.checked end}
    end
    local controls = {
        ui.formField{label = 'Noise', ui.slider{id = 'noise', min = 0, max = 1, step = 0.05, value = self.noise, showValue = true, decimals = 2, onChange = function(event) self.noise = event.value end}},
        ui.toggle{id = 'spikes', text = 'Spikes now and then', checked = self.spikes, onChange = function(event) self.spikes = event.checked end},
    }
    for _, toggle in ipairs(toggles) do
        controls[#controls + 1] = toggle
    end
    self:frame{
        hint = 'The gray dots are the measurements of the white wave. Turn the noise and the spikes up and down, and pick the filters to compare their delay, their smoothness and how they handle spikes. R or the X button starts the plot again.',
        controls = controls,
        focus = 'noise',
    }
    self.random = m.random(29)
    self:reset()
end

function Smoothing:reset()
    self.time, self.carry = 0, 0
    self.filter = {
        lowPass = m.lowPassFilter({cutoff = 1.5}),
        average = m.movingAverage({window = 12}),
        median = m.medianFilter({window = 9}),
        hampel = m.hampelFilter({window = 9, threshold = 3}),
        kalman = m.kalmanFilter({processNoise = 0.002, measurementNoise = 0.05}),
    }
    self.truth, self.measured, self.lines = {}, {}, {}
    for _, filter in ipairs(Smoothing.filters) do
        self.lines[filter.id] = {}
    end
end

-- Measures the wave at a fixed rate, so the plot scrolls the same at any frame rate, and feeds every filter the same measurement.
function Smoothing:sample()
    self.time = self.time + 1 / Smoothing.rate
    local truth = math.sin(self.time * 1.7) * 0.6 + math.sin(self.time * 0.45) * 0.3
    local measured = truth + self.random:range(-1, 1) * self.noise * 0.5
    if self.spikes and self.random:chance(0.03) then
        measured = measured + (self.random:chance(0.5) and 1 or -1) * self.random:range(1.2, 2)
    end
    table.insert(self.truth, truth)
    table.insert(self.measured, measured)
    table.insert(self.lines.lowPass, self.filter.lowPass:update(1 / Smoothing.rate, measured))
    table.insert(self.lines.average, self.filter.average:update(measured))
    table.insert(self.lines.median, self.filter.median:update(measured))
    table.insert(self.lines.hampel, self.filter.hampel:update(measured))
    table.insert(self.lines.kalman, self.filter.kalman:update(measured))
    for _, history in pairs({self.truth, self.measured, self.lines.lowPass, self.lines.average, self.lines.median, self.lines.hampel, self.lines.kalman}) do
        if #history > Smoothing.samples then
            table.remove(history, 1)
        end
    end
end

function Smoothing:update(dt)
    Smoothing.super.update(self, dt)
    if input.pressed('reset') then
        self:reset()
    end
    self.carry = self.carry + dt
    while self.carry >= 1 / Smoothing.rate do
        self.carry = self.carry - 1 / Smoothing.rate
        self:sample()
    end
    self:status(string.format('Noise %.2f   Spikes "%s"   Samples %d at %d per second', self.noise, tostring(self.spikes), #self.measured, Smoothing.rate))
end

function Smoothing.point(index, value)
    return {-Smoothing.width / 2 + (index - 1) * Smoothing.width / (Smoothing.samples - 1), -value * Smoothing.height / 2}
end

function Smoothing.polyline(values)
    local points = {}
    for index, value in ipairs(values) do
        points[index] = Smoothing.point(index, value)
    end
    return points
end

function Smoothing:draw(area)
    graphics2d.drawLine(-Smoothing.width / 2, 0, Smoothing.width / 2, 0, 2, '#30FFFFFF')
    for index, value in ipairs(self.measured) do
        local point = Smoothing.point(index, value)
        graphics2d.drawCircle(point[1], point[2], 3, '#80B0BEC5')
    end
    if #self.truth > 1 then
        graphics2d.drawPolyline(Smoothing.polyline(self.truth), 3, '#C0FFFFFF', false, {layer = 1})
    end
    for order, filter in ipairs(Smoothing.filters) do
        if self.shown[filter.id] and #self.lines[filter.id] > 1 then
            graphics2d.drawPolyline(Smoothing.polyline(self.lines[filter.id]), 3, filter.color, false, {layer = 1 + order})
        end
        Smoothing.caption(filter.text, -Smoothing.width / 2 + (order - 1) * 240, Smoothing.height / 2 + 110, {color = filter.color, size = 26})
    end
end

return Smoothing
