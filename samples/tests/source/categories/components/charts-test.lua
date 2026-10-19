-- Charts: the eleven kinds of `charts.lua` on one screen, with data sets that a chart must survive: normal values, no values, negative values, huge values, values that are missing, infinite or not numbers, a single value and values that are all equal. At start the test lays every kind out with every data set and fails when a chart lays out a number that is not finite.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Charts = require('categories.components.charts')
local Test = require('harness.test')

local ChartsTest = haylen.class('ChartsTest', Test)

ChartsTest.code = [[
local Charts = require('charts')  -- One file, copied from samples/tests.
local sales = Charts.new('bar', {12, -4, 30, 0 / 0, 18}, {labels = {'Mon', 'Tue', 'Wed', 'Thu', 'Fri'}})
local share = Charts.new('donut', {40, 25, 35})  local prices = Charts.new('candles', {{10, 14, 9, 13}, {13, 15, 11, 12}})
sales:draw({40, 40, 480, 300})  -- Lays out once for each size, then draws without allocating.
sales:setData(newValues)  sales:check({0, 0, 480, 300})  -- Raises when a shape holds a number that is not finite.]]

ChartsTest.sets = {'normal', 'empty', 'negative', 'huge', 'missing', 'single', 'flat'}
ChartsTest.titles = {line = 'Line, two series', area = 'Area', bar = 'Bar', stackedBar = 'Stacked bar', pie = 'Pie', donut = 'Donut', scatter = 'Scatter', radar = 'Radar', histogram = 'Histogram', candles = 'Candles', heatmap = 'Heat map'}

-- Returns the values of a data set, with `wave` giving normal values that change with `seed`.
function ChartsTest.values(set, count, seed)
    local values = {}
    for index = 1, count do
        local wave = 20 + math.sin(index * 0.9 + seed) * 12 + math.cos(index * 0.37 + seed * 2) * 6
        if set == 'normal' then
            values[index] = wave
        elseif set == 'negative' then
            values[index] = wave - 26
        elseif set == 'huge' then
            values[index] = wave * 1e11
        elseif set == 'missing' then
            -- A list cannot hold nil, so a missing value is false, as in data read from a file.
            local odd = {wave, wave, 0 / 0, wave, math.huge, wave, wave, -math.huge, wave, 'seven', wave, false}
            values[index] = odd[(index - 1) % #odd + 1]
        elseif set == 'single' then
            values[index] = index == 1 and wave or nil
        elseif set == 'flat' then
            values[index] = 7
        end
    end
    if set == 'empty' then
        return {}
    end
    return values
end

-- Returns the data of a kind of chart for a data set.
function ChartsTest.data(kind, set, seed)
    local values = function(count, offset)
        return ChartsTest.values(set, count, seed + (offset or 0))
    end
    if kind == 'line' or kind == 'area' then
        return kind == 'line' and {values(14), values(14, 3)} or values(14)
    elseif kind == 'stackedBar' then
        return {values(6), values(6, 1), values(6, 2)}
    elseif kind == 'scatter' then
        local xs, ys, points = values(40, 5), values(40, 9), {}
        for index = 1, #xs do
            points[index] = {xs[index], ys[index]}
        end
        return points
    elseif kind == 'candles' then
        local opens, closes, candles = values(16), values(16, 0.4), {}
        for index = 1, #opens do
            local open, close = opens[index], closes[index]
            local high = type(open) == 'number' and type(close) == 'number' and math.max(open, close) * 1.08 or open
            local low = type(open) == 'number' and type(close) == 'number' and math.min(open, close) * 0.92 or close
            candles[index] = {open, high, low, close}
        end
        return candles
    elseif kind == 'heatmap' then
        local rows = {}
        for row = 1, 6 do
            rows[row] = values(10, row)
        end
        return set == 'empty' and {} or rows
    elseif kind == 'histogram' then
        return values(80, 2)
    elseif kind == 'radar' then
        return values(6, 4)
    elseif kind == 'pie' or kind == 'donut' then
        return values(5, 6)
    end
    return values(7)
end

ChartsTest.options = {
    bar = {labels = {'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'}},
    stackedBar = {labels = {'Q1', 'Q2', 'Q3', 'Q4', 'Q5', 'Q6'}},
    radar = {labels = {'Speed', 'Power', 'Range', 'Armor', 'Luck', 'Magic'}},
}

function ChartsTest:enter()
    self.set = 'normal'
    self.seed = 1
    self.charts = {}
    for _, kind in ipairs(Charts.kinds) do
        self.charts[#self.charts + 1] = Charts.new(kind, ChartsTest.data(kind, self.set, self.seed), ChartsTest.options[kind])
    end
    self:proveEveryData()
    self:frame{
        hint = 'Pick a data set to see every chart handle it, and draw new random values for the normal one.',
        code = ChartsTest.code,
        controls = {
            ui.radioGroup{id = 'set', selected = 'normal', items = {{id = 'normal', text = 'Normal values'}, {id = 'empty', text = 'No values'}, {id = 'negative', text = 'Negative values'}, {id = 'huge', text = 'Huge values'}, {id = 'missing', text = 'Missing, infinite and not numbers'}, {id = 'single', text = 'One value'}, {id = 'flat', text = 'All equal'}}, onChange = function(event)
                self:show(event.value)
            end},
            ui.button{id = 'shuffle', text = 'New random values', variant = 'primary', onClick = function()
                self.seed = self.seed + 1.7
                self:show(self.set)
            end},
        },
    }
end

-- Lays every kind out with every data set at two sizes, which raises when a chart lays out a number that is not finite.
function ChartsTest:proveEveryData()
    local shapes = 0
    for _, set in ipairs(ChartsTest.sets) do
        for _, kind in ipairs(Charts.kinds) do
            local chart = Charts.new(kind, ChartsTest.data(kind, set, 3), ChartsTest.options[kind])
            shapes = shapes + chart:check({0, 0, 420, 260}) + chart:check({10, 10, 90, 60})
        end
    end
    self:log('Every one of the %d kinds of chart laid out every one of the %d data sets at two sizes, %d shapes with finite numbers.', #Charts.kinds, #ChartsTest.sets, shapes)
end

function ChartsTest:show(set)
    self.set = set
    for index, kind in ipairs(Charts.kinds) do
        self.charts[index]:setData(ChartsTest.data(kind, set, self.seed))
    end
end

function ChartsTest:update(dt)
    ChartsTest.super.update(self, dt)
    if self.stage then
        self:status(string.format('Data set "%s", draw calls %d', self.set, graphics2d.stats().drawCalls))
    end
end

function ChartsTest:draw(area)
    local columns = 4
    local rows = math.ceil(#self.charts / columns)
    local gap = 12
    local width = (area.width - gap * (columns + 1)) / columns
    local height = (area.height - gap * (rows + 1)) / rows
    for index, chart in ipairs(self.charts) do
        local column, row = (index - 1) % columns, math.floor((index - 1) / columns)
        local x, y = gap + column * (width + gap), gap + row * (height + gap)
        graphics2d.drawShape({x, y, width, height}, {radius = 10, color = '#FF1B2430', borderWidth = 2, borderColor = '#FF2A3646'})
        Test.caption(ChartsTest.titles[chart.kind], x + 12, y + 8, {color = Test.ink})
        chart:draw({x + 4, y + 30, width - 8, height - 34})
    end
end

return ChartsTest
