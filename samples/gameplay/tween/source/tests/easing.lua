-- Easing gallery: every Penner family in its in, out and in-out forms, back and elastic with their parameters, steps, cubic Bezier curves and curves by points, each plotted with a dot that a tween with that curve moves.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local sample = require('sample')

local Easing = haylen.class('Easing', sample.Test)

local kColumns = 8
local kSamples = 40
local kFamilies = {'sine', 'quad', 'cubic', 'quart', 'quint', 'expo', 'circ', 'back', 'elastic', 'bounce'}

-- Every curve of the gallery with the text that writes it in code.
local function curves()
    local list = {{name = 'linear', curve = 'linear', code = "'linear'"}}
    for _, family in ipairs(kFamilies) do
        for _, variant in ipairs({'in', 'out', 'in_out'}) do
            local name = family .. '_' .. variant
            list[#list + 1] = {name = name, curve = name, code = "'" .. name .. "'"}
        end
    end
    list[#list + 1] = {name = 'back overshoot 3', curve = {curve = 'back_out', overshoot = 3}, code = "{curve = 'back_out', overshoot = 3}"}
    list[#list + 1] = {name = 'elastic 1.5, 0.4', curve = {curve = 'elastic_out', amplitude = 1.5, period = 0.4}, code = "{curve = 'elastic_out', amplitude = 1.5, period = 0.4}"}
    list[#list + 1] = {name = 'steps 5 end', curve = {steps = 5, position = 'end'}, code = "{steps = 5, position = 'end'}"}
    list[#list + 1] = {name = 'steps 4 start', curve = {steps = 4, position = 'start'}, code = "{steps = 4, position = 'start'}"}
    list[#list + 1] = {name = 'css ease', curve = {bezier = {0.25, 0.1, 0.25, 1}}, code = '{bezier = {0.25, 0.1, 0.25, 1}}'}
    list[#list + 1] = {name = 'bezier overshoot', curve = {bezier = {0.68, -0.6, 0.32, 1.6}}, code = '{bezier = {0.68, -0.6, 0.32, 1.6}}'}
    list[#list + 1] = {name = 'points even', curve = {points = {0, 1.2, 0.8, 1}}, code = '{points = {0, 1.2, 0.8, 1}}'}
    list[#list + 1] = {name = 'points x, y', curve = {points = {{0, 0}, {0.3, 0.8}, {0.6, 0.2}, {1, 1}}}, code = '{points = {{0, 0}, {0.3, 0.8}, {0.6, 0.2}, {1, 1}}}'}
    return list
end

function Easing:enter()
    self.curves = curves()
    local items = {}
    for index, entry in ipairs(self.curves) do
        entry.dot = {value = 0}
        entry.handle = tween.to(entry.dot, 1.6, {value = 1}, {owner = self, ease = entry.curve, repeatCount = -1, repeatDelay = 0.6})
        entry.plot = {}
        for step = 0, kSamples do
            entry.plot[step + 1] = m.ease(entry.curve, step / kSamples)
        end
        items[index] = {id = tostring(index), text = entry.name}
    end
    self.selected = 1
    self:frame({
        hint = 'Pick a curve with the stepper, a click or a tap on its plot.',
        code = '',
        controls = {ui.formField{label = 'Curve', ui.stepper{id = 'curve', items = items, selected = '1', wrap = true, onChange = function(event) self:select(tonumber(event.value)) end}}},
        focus = 'curve',
    })
    self:select(1)
end

function Easing:select(index)
    self.selected = index
    self:set('code', {text = 'tween.to(dot, 1.6, {value = 1}, {ease = ' .. self.curves[index].code .. ', repeatCount = -1, repeatDelay = 0.6})'})
    self:set('curve', {selected = tostring(index)})
end

function Easing:cell(area, index)
    local rows = math.ceil(#self.curves / kColumns)
    local width, height = area.width / kColumns, area.height / rows
    local column, row = (index - 1) % kColumns, (index - 1) // kColumns
    return column * width, row * height, width, height
end

function Easing:update(dt)
    Easing.super.update(self, dt)
    local x, y, pressed = sample.pointer()
    if pressed and self.area then
        local localX, localY = self:toStage(x, y)
        for index = 1, #self.curves do
            local left, top, width, height = self:cell(self.area, index)
            if localX >= left and localX < left + width and localY >= top and localY < top + height then
                self:select(index)
            end
        end
    end
    local entry = self.curves[self.selected]
    self:status(string.format('%s   progress %.2f   value %.3f', entry.name, entry.handle.progress, entry.dot.value))
end

function Easing:draw(area)
    for index, entry in ipairs(self.curves) do
        local left, top, width, height = self:cell(area, index)
        local plotX, plotY, plotWidth, plotHeight = left + 14, top + 32, width - 28, height - 60
        local color = index == self.selected and sample.warm or sample.accent
        if index == self.selected then
            graphics2d.drawRect({left + 3, top + 3, width - 6, height - 6}, '#FF232A3A')
        end
        graphics2d.drawText(nil, entry.name, left + width / 2, top + 16, {size = 20, color = sample.ink, anchor = {0.5, 0.5}})
        graphics2d.drawLine(plotX, plotY + plotHeight, plotX + plotWidth, plotY + plotHeight, 1, sample.line)
        graphics2d.drawLine(plotX, plotY, plotX + plotWidth, plotY, 1, sample.line)

        local points = {}
        for step, value in ipairs(entry.plot) do
            points[step] = {plotX + plotWidth * (step - 1) / kSamples, plotY + plotHeight * (1 - value)}
        end
        graphics2d.drawPolyline(points, 2, color)
        graphics2d.drawCircle(plotX + plotWidth * entry.handle.progress, plotY + plotHeight * (1 - entry.dot.value), 6, sample.ink, {layer = 1})
    end
end

return Easing
