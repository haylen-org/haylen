-- Smoothing a noisy signal: a wave measured with noise and with spikes now and then, and the line each signal filter of `haylen.math` draws through it, so the delay, the smoothness and the handling of spikes of each one show side by side.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')

local Smoothing = haylen.class('Smoothing', AlgorithmTest)

Smoothing.code = [[
local lowPass = m.lowPassFilter({cutoff = 1.5})
local hampel = m.hampelFilter({window = 9, threshold = 3})
local kalman = m.kalmanFilter({processNoise = 0.002, measurementNoise = 0.05})
local smooth = lowPass:update(dt, measured)  -- The low-pass filter takes the time step.
local clean = hampel:update(measured)  -- Replaces a spike with the median around it.
local tracked = kalman:update(measured)]]

Smoothing.samples = 360
Smoothing.rate = 60
-- Every filter has a row of its own in this box of the world, with the plot after its name.
Smoothing.box = {left = -790, top = -415, width = 1580, height = 830}
Smoothing.plotLeft, Smoothing.plotWidth = -500, 1270
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
        hint = 'Each row draws one filter through the same measurements, the gray dots, of the white wave. Turn the noise and the spikes up and down, and pick the filters to compare their delay, their smoothness and how they handle spikes. R or the X button starts the plot again.',
        code = Smoothing.code,
        controls = controls,
        focus = 'noise',
    }
    self.random = m.random(29)
    self:reset()
    -- The plot opens full, as if it had been measuring for a while.
    for _ = 1, Smoothing.samples do
        self:sample()
    end
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
    self:status(string.format('Noise %.2f, spikes %s, %d samples at %d per second', self.noise, self.spikes and 'on' or 'off', #self.measured, Smoothing.rate))
end

-- Turns a history into the flat points of a row whose middle is `middle` and which draws `scale` units for a value of 1.
function Smoothing.flat(values, middle, scale)
    local points = {}
    local step = Smoothing.plotWidth / (Smoothing.samples - 1)
    for index, value in ipairs(values) do
        points[#points + 1] = Smoothing.plotLeft + (index - 1) * step
        points[#points + 1] = middle - value * scale
    end
    return points
end

function Smoothing:draw(area)
    local shown = {}
    for _, filter in ipairs(Smoothing.filters) do
        if self.shown[filter.id] then
            shown[#shown + 1] = filter
        end
    end
    local box = Smoothing.box
    if #shown == 0 then
        Smoothing.caption('Turn a filter on to see its line through the measurements.', 0, 0, {anchor = {0.5, 0.5}, size = 26})
        return
    end

    local gap = 14
    local rowHeight = (box.height - gap * (#shown - 1)) / #shown
    for index, filter in ipairs(shown) do
        local top = box.top + (index - 1) * (rowHeight + gap)
        local middle, scale = top + rowHeight / 2, rowHeight * 0.3
        graphics2d.drawShape({box.left, top, box.width, rowHeight}, {radius = 14, color = '#FF1C2230', borderWidth = 2, borderColor = '#FF2C3147'})
        graphics2d.drawShape({box.left + 22, middle - 5, 26, 10}, {radius = 5, color = filter.color, layer = 1})
        Smoothing.caption(filter.text, box.left + 60, middle, {anchor = {0, 0.5}, color = '#FFE8EAF2', size = 26})
        graphics2d.pushClip({Smoothing.plotLeft - 10, top + 4, Smoothing.plotWidth + 20, rowHeight - 8})
        graphics2d.drawLine(Smoothing.plotLeft, middle, Smoothing.plotLeft + Smoothing.plotWidth, middle, 1.5, '#30FFFFFF')
        if #self.measured > 1 then
            graphics2d.drawPoints(Smoothing.flat(self.measured, middle, scale), {radius = 2.5, color = '#70B0BEC5', layer = 1})
            graphics2d.drawStroke(Smoothing.flat(self.truth, middle, scale), {width = 2, color = '#A0FFFFFF', join = 'round', layer = 2})
            graphics2d.drawStroke(Smoothing.flat(self.lines[filter.id], middle, scale), {width = 3.5, color = filter.color, join = 'round', layer = 3})
        end
        graphics2d.popClip()
    end
end

return Smoothing
