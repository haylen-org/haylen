-- Easing gallery: every easing family in its in, out and in-out forms, back and elastic with their parameters, steps, cubic Bezier curves and curves by points, each plotted under its name with a dot that a tween with that curve moves.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Pointer = require('harness.pointer')
local Test = require('harness.test')
local TweenTest = require('categories.tween.tween-test')
local code = require('harness.code')

local Easing = haylen.class('Easing', TweenTest)

local kColumns = 8
local kSamples = 48
local kFamilies = {'sine', 'quad', 'cubic', 'quart', 'quint', 'expo', 'circ', 'back', 'elastic', 'bounce'}

-- Every curve of the gallery with the text that writes it in code.
local function curves()
    local list = {{name = 'Linear', curve = 'linear', code = "'linear'"}}
    for _, family in ipairs(kFamilies) do
        for _, variant in ipairs({'In', 'Out', 'InOut'}) do
            local name = family .. variant
            list[#list + 1] = {name = (name:gsub('%u', function(letter) return ' ' .. letter:lower() end):gsub('^%l', string.upper)), curve = name, code = "'" .. name .. "'"}
        end
    end
    list[#list + 1] = {name = 'Back overshoot 3', curve = {curve = 'backOut', overshoot = 3}, code = "{curve = 'backOut', overshoot = 3}"}
    list[#list + 1] = {name = 'Elastic 1.5, 0.4', curve = {curve = 'elasticOut', amplitude = 1.5, period = 0.4}, code = "{curve = 'elasticOut', amplitude = 1.5, period = 0.4}"}
    list[#list + 1] = {name = 'Steps 5 end', curve = {steps = 5, position = 'end'}, code = "{steps = 5, position = 'end'}"}
    list[#list + 1] = {name = 'Steps 4 start', curve = {steps = 4, position = 'start'}, code = "{steps = 4, position = 'start'}"}
    list[#list + 1] = {name = 'Bezier ease', curve = {cubicBezier = {0.25, 0.1, 0.25, 1}}, code = '{cubicBezier = {0.25, 0.1, 0.25, 1}}'}
    list[#list + 1] = {name = 'Bezier overshoot', curve = {cubicBezier = {0.68, -0.6, 0.32, 1.6}}, code = '{cubicBezier = {0.68, -0.6, 0.32, 1.6}}'}
    list[#list + 1] = {name = 'Points even', curve = {points = {0, 1.2, 0.8, 1}}, code = '{points = {0, 1.2, 0.8, 1}}'}
    list[#list + 1] = {name = 'Points x, y', curve = {points = {{0, 0}, {0.3, 0.8}, {0.6, 0.2}, {1, 1}}}, code = '{points = {{0, 0}, {0.3, 0.8}, {0.6, 0.2}, {1, 1}}}'}
    return list
end

function Easing:enter()
    self.curves = curves()
    local items = {}
    for index, entry in ipairs(self.curves) do
        entry.dot = {value = 0}
        entry.handle = tween.to(entry.dot, 1.6, {value = 1}, {owner = self, ease = entry.curve, repeatCount = -1, repeatDelay = 0.6})
        entry.values, entry.low, entry.high = TweenTest.sample(entry.curve, kSamples)
        entry.points = {}
        items[index] = {id = tostring(index), text = entry.name}
    end
    self.selected = 1
    self.pointer = Pointer()
    self:frame({
        hint = 'Pick a curve with the stepper, a click or a tap on its plot. The dashed lines mark 0 and 1, and every plot spans what its curve reaches.',
        code = '',
        controls = {ui.formField{label = 'Curve', ui.stepper{id = 'curve', items = items, selected = '1', wrap = true, onChange = function(event) self:select(tonumber(event.value)) end}}},
        focus = 'curve',
        actions = Pointer.actions,
    })
    self:select(1)
end

-- Shows the code of the picked curve in the card, which keeps it through changes of theme.
function Easing:select(index)
    self.selected = index
    self.codeSource = 'tween.to(dot, 1.6, {value = 1}, {ease = ' .. self.curves[index].code .. ', repeatCount = -1, repeatDelay = 0.6})'
    self:set('code', {text = code.markup(self.codeSource)})
    self:set('curve', {selected = tostring(index)})
end

-- The cells fill the area in rows of `kColumns`, each with a band for its name above its plot.
function Easing:cell(area, index)
    local rows = math.ceil(#self.curves / kColumns)
    local width, height = area.width / kColumns, area.height / rows
    local column, row = (index - 1) % kColumns, (index - 1) // kColumns
    return column * width, row * height, width, height
end

function Easing:update(dt)
    Easing.super.update(self, dt)
    self.pointer:update(dt, self)
    if self.pointer.pressed then
        local localX, localY = self.pointer.worldX, self.pointer.worldY
        for index = 1, #self.curves do
            local left, top, width, height = self:cell(self.area, index)
            if localX >= left and localX < left + width and localY >= top and localY < top + height then
                self:select(index)
            end
        end
    end
    local entry = self.curves[self.selected]
    self:status(string.format('The curve "%s" is %.0f%% through its time, with the value %.3f.', entry.name, entry.handle.progress * 100, entry.dot.value))
end

function Easing:renderUi()
    self.pointer:draw()
end

function Easing:draw(area)
    for index, entry in ipairs(self.curves) do
        local left, top, width, height = self:cell(area, index)
        local selected = index == self.selected
        if selected then
            graphics2d.drawShape({left + 3, top + 2, width - 6, height - 4}, {radius = 12, color = '#FF242B3D', borderWidth = 2, borderColor = Test.warm})
        end
        graphics2d.drawText(nil, entry.name, left + width / 2, top + 8, {size = 19, color = selected and Test.warm or Test.ink, anchor = {0.5, 0}, maxWidth = width - 16, wrap = false, minScale = 0.7})
        TweenTest.drawPlot({left + 10, top + 34, width - 20, height - 42}, entry.values, entry.low, entry.high, {color = selected and Test.warm or Test.accent, points = entry.points, progress = entry.handle.progress, value = entry.dot.value})
    end
end

return Easing
