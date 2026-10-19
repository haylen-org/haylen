-- Charts: line, area, bar, stacked bar, pie, donut, scatter, radar, histogram, candlestick and heat map charts drawn with "haylen.graphics2d", which keep working with data that is empty, negative, huge, missing, not a number or a single point.
-- The module needs only the engine, so it copies as it is into another project. Make a chart with `Charts.new(kind, data, options)` and call `chart:draw(rect)` every frame with the area it fills, `{x, y, width, height}`. The chart lays its shapes out once for every size it draws at and keeps them, so drawing it again allocates nothing, and `chart:check(rect)` raises an error when the laid out shapes hold a number that is not finite, which is how tests prove a chart survives bad data.
-- Values that are not finite numbers, such as `0 / 0`, `math.huge`, `false` or text, count as missing: a line breaks around them, a bar is left out and a heat map cell draws empty. A list never holds `nil`, which would end it early, so data marks a missing value with `false`. Values beyond 1e15 in size clamp to it, so the scale never overflows.
local graphics2d = require('haylen.graphics2d')

local Charts = {}

Charts.kinds = {'line', 'area', 'bar', 'stackedBar', 'pie', 'donut', 'scatter', 'radar', 'histogram', 'candles', 'heatmap'}
Charts.palette = {{0.24, 0.61, 0.88}, {0.21, 0.7, 0.49}, {0.91, 0.66, 0.23}, {0.9, 0.33, 0.29}, {0.66, 0.5, 0.88}, {0.31, 0.79, 0.77}, {0.94, 0.49, 0.3}, {0.61, 0.71, 0.8}}
Charts.colors = {text = {0.54, 0.6, 0.68, 1}, grid = {0.15, 0.2, 0.26, 1}, axis = {0.3, 0.36, 0.44, 1}, rise = {0.21, 0.7, 0.49, 1}, fall = {0.9, 0.33, 0.29, 1}, empty = {0.2, 0.24, 0.3, 1}}
Charts.limit = 1e15

local Chart = {}
Chart.__index = Chart

-- Turns a value into a finite number or nothing.
function Charts.finite(value)
    if type(value) ~= 'number' or value ~= value or value == math.huge or value == -math.huge then
        return nil
    end
    return math.max(-Charts.limit, math.min(Charts.limit, value))
end

-- Writes a value in a short form for an axis: 1.2k, 3.4M, 5B, 0.25 or -20.
function Charts.format(value)
    local size = math.abs(value)
    for _, unit in ipairs({{1e12, 'T'}, {1e9, 'B'}, {1e6, 'M'}, {1e3, 'k'}}) do
        if size >= unit[1] then
            return (string.format('%.1f', value / unit[1]):gsub('%.0$', '')) .. unit[2]
        end
    end
    if size == 0 or size >= 10 then
        return string.format('%d', math.floor(value + 0.5))
    end
    return (string.format('%.2f', value):gsub('0+$', ''):gsub('%.$', ''))
end

-- Returns a scale with round ends and a round step that covers `low` to `high` in about `count` steps, which also works when both are equal or zero.
function Charts.scale(low, high, count)
    if low == high then
        local pad = low == 0 and 1 or math.abs(low) * 0.5
        low, high = low - pad, high + pad
    end
    local raw = (high - low) / (count or 4)
    local magnitude = 10 ^ math.floor(math.log(raw, 10))
    local step = magnitude
    for _, factor in ipairs({1, 2, 2.5, 5, 10}) do
        step = factor * magnitude
        if step >= raw then
            break
        end
    end
    return math.floor(low / step) * step, math.ceil(high / step) * step, step
end

-- Makes a chart. `data` is a list of numbers for "line", "area", "bar", "pie", "donut", "histogram" and "radar", a list of lists for several series of "line", "area" and "stackedBar", a list of `{x, y}` for "scatter", a list of `{open, high, low, close}` for "candles" and a list of rows of numbers for "heatmap". Options: `labels`, the names of the bars, slices, axes or columns, `bins` of a histogram (from the data when nil) and `maximum` of a radar (from the data when nil).
function Charts.new(kind, data, options)
    local known = false
    for _, name in ipairs(Charts.kinds) do
        known = known or name == kind
    end
    if not known then
        error('The kind of chart "' .. tostring(kind) .. '" is unknown. It is one of "' .. table.concat(Charts.kinds, '", "') .. '".', 2)
    end
    return setmetatable({kind = kind, data = data or {}, options = options or {}, items = {}, width = -1, height = -1, x = 0, y = 0}, Chart)
end

-- Replaces the data, so the next draw lays the chart out again.
function Chart:setData(data)
    self.data = data or {}
    self.width = -1
end

function Chart:add(item)
    self.items[#self.items + 1] = item
    return item
end

function Chart:text(text, x, y, anchor, color)
    return self:add({'text', text, x, y, {size = 16, color = color or Charts.colors.text, anchor = anchor or {0.5, 0.5}, layer = 3}})
end

function Chart:message(text, rect)
    self:text(text, rect[1] + rect[3] / 2, rect[2] + rect[4] / 2)
end

local function color(index, alpha)
    local tone = Charts.palette[(index - 1) % #Charts.palette + 1]
    return {tone[1], tone[2], tone[3], alpha or 1}
end

-- The area inside the axis labels, and the horizontal grid with its labels for a value scale from `low` to `high`.
function Chart:plot(rect, low, high, step)
    local left, top = rect[1] + 56, rect[2] + 10
    local width, height = rect[3] - 70, rect[4] - 42
    for value = low, high + step * 0.5, step do
        local y = top + height - (value - low) / (high - low) * height
        self:add({'line', left, y, left + width, y, value == 0 and 2 or 1, value == 0 and Charts.colors.axis or Charts.colors.grid, {layer = 1}})
        self:text(Charts.format(value), left - 8, y, {1, 0.5})
    end
    return left, top, width, height
end

-- Builds a list of series from data that is a list of numbers or a list of lists.
local function series(data)
    if type(data[1]) == 'table' then
        return data
    end
    return {data}
end

-- Finds the lowest and highest finite values of several series, and the length of the longest.
local function range(list)
    local low, high, length = nil, nil, 0
    for _, values in ipairs(list) do
        length = math.max(length, #values)
        for index = 1, #values do
            local value = Charts.finite(values[index])
            if value then
                low, high = math.min(low or value, value), math.max(high or value, value)
            end
        end
    end
    return low, high, length
end

function Chart:layoutLines(rect, filled)
    local list = series(self.data)
    local low, high, length = range(list)
    if not low then
        return self:message('No data', rect)
    end
    if filled then
        low, high = math.min(low, 0), math.max(high, 0)
    end
    local step
    low, high, step = Charts.scale(low, high, 4)
    local left, top, width, height = self:plot(rect, low, high, step)
    local function at(index, value)
        return left + (length > 1 and (index - 1) / (length - 1) or 0.5) * width, top + height - (value - low) / (high - low) * height
    end
    local baseY = select(2, at(1, math.max(low, math.min(high, 0))))

    for number, values in ipairs(list) do
        -- A run is a stretch of finite values, and a missing value starts a new one.
        local run = {}
        local function finish()
            if #run == 1 then
                self:add({'circle', run[1][1], run[1][2], 5, color(number), {layer = 2}})
            elseif #run > 1 then
                if filled then
                    self:fill(run, baseY, color(number, 0.3))
                end
                self:add({'polyline', run, 3, color(number), false, {layer = 2}})
            end
            run = {}
        end
        for index = 1, length do
            local value = Charts.finite(values[index])
            if value then
                local x, y = at(index, value)
                run[#run + 1] = {x, y}
            else
                finish()
            end
        end
        finish()
    end
end

-- Fills the area between a run of points and the baseline, as one polygon on each side of the baseline, so a line that crosses it never makes a polygon that crosses itself.
function Chart:fill(run, baseY, tone)
    local shape = {{run[1][1], baseY}}
    for index = 1, #run do
        local point = run[index]
        local previous = run[index - 1]
        if previous and (previous[2] - baseY) * (point[2] - baseY) < 0 then
            local t = (baseY - previous[2]) / (point[2] - previous[2])
            local crossX = previous[1] + (point[1] - previous[1]) * t
            shape[#shape + 1] = {crossX, baseY}
            self:add({'polygon', shape, tone, {layer = 1}})
            shape = {{crossX, baseY}}
        end
        shape[#shape + 1] = {point[1], point[2]}
    end
    shape[#shape + 1] = {run[#run][1], baseY}
    self:add({'polygon', shape, tone, {layer = 1}})
end

function Chart:categoryLabels(left, width, count, top, height)
    local labels = self.options.labels or {}
    local slot = width / count
    for index = 1, count do
        if labels[index] then
            self:text(tostring(labels[index]), left + slot * (index - 0.5), top + height + 16)
        end
    end
end

function Chart:layoutBars(rect)
    local values = self.data
    local low, high = range({values})
    if not low then
        return self:message('No data', rect)
    end
    local step
    low, high, step = Charts.scale(math.min(low, 0), math.max(high, 0), 4)
    local left, top, width, height = self:plot(rect, low, high, step)
    local slot = width / #values
    local function y(value)
        return top + height - (value - low) / (high - low) * height
    end
    for index = 1, #values do
        local value = Charts.finite(values[index])
        if value then
            local from, to = y(0), y(value)
            self:add({'shape', {left + slot * (index - 1) + slot * 0.15, math.min(from, to), slot * 0.7, math.max(1, math.abs(to - from))}, {radius = 3, color = value < 0 and Charts.colors.fall or color(1), layer = 2}})
        end
    end
    self:categoryLabels(left, width, #values, top, height)
end

function Chart:layoutStacked(rect)
    local list = series(self.data)
    local length = select(3, range(list))
    -- Positive values stack up from zero and negative ones stack down from it, so a column never hides a part behind another.
    local low, high = 0, 0
    for index = 1, length do
        local up, down = 0, 0
        for _, values in ipairs(list) do
            local value = Charts.finite(values[index]) or 0
            if value > 0 then
                up = up + value
            else
                down = down + value
            end
        end
        low, high = math.min(low, down), math.max(high, up)
    end
    if length == 0 or (low == 0 and high == 0) then
        return self:message('No data', rect)
    end
    local step
    low, high, step = Charts.scale(low, high, 4)
    local left, top, width, height = self:plot(rect, low, high, step)
    local slot = width / length
    local function y(value)
        return top + height - (value - low) / (high - low) * height
    end
    for index = 1, length do
        local up, down = 0, 0
        for number, values in ipairs(list) do
            local value = Charts.finite(values[index]) or 0
            local from = value > 0 and up or down
            local to = from + value
            if value > 0 then
                up = to
            else
                down = to
            end
            if value ~= 0 then
                self:add({'shape', {left + slot * (index - 1) + slot * 0.15, math.min(y(from), y(to)), slot * 0.7, math.abs(y(to) - y(from))}, {color = color(number), layer = 2}})
            end
        end
    end
    self:categoryLabels(left, width, length, top, height)
end

function Chart:layoutPie(rect, hole)
    -- Only positive values have a share, so negative and missing values leave no slice.
    local total = 0
    for index = 1, #self.data do
        local value = Charts.finite(self.data[index])
        if value and value > 0 then
            total = total + value
        end
    end
    if total <= 0 then
        return self:message('No data', rect)
    end
    local radius = math.min(rect[3], rect[4]) / 2 - 26
    local centerX, centerY = rect[1] + rect[3] / 2, rect[2] + rect[4] / 2
    local angle = -math.pi / 2
    for index = 1, #self.data do
        local value = Charts.finite(self.data[index])
        if value and value > 0 then
            local sweep = value / total * math.pi * 2
            if hole then
                self:add({'arc', centerX, centerY, radius * 0.8, radius * 0.4, angle, angle + sweep, color(index), {layer = 2}})
            else
                self:add({'shape', {centerX - radius, centerY - radius, radius * 2, radius * 2}, {kind = 'ellipse', startAngle = angle, sweep = sweep, color = color(index), layer = 2}})
            end
            local middle = angle + sweep / 2
            if sweep > 0.4 then
                self:text(string.format('%d%%', math.floor(value / total * 100 + 0.5)), centerX + math.cos(middle) * (radius + 16), centerY + math.sin(middle) * (radius + 16))
            end
            angle = angle + sweep
        end
    end
    if hole then
        self:text(Charts.format(total), centerX, centerY, {0.5, 0.5}, {0.9, 0.92, 0.95, 1})
    end
end

function Chart:layoutScatter(rect)
    local lowX, highX, lowY, highY
    for _, point in ipairs(self.data) do
        local x, y = Charts.finite(point[1]), Charts.finite(point[2])
        if x and y then
            lowX, highX = math.min(lowX or x, x), math.max(highX or x, x)
            lowY, highY = math.min(lowY or y, y), math.max(highY or y, y)
        end
    end
    if not lowX then
        return self:message('No data', rect)
    end
    local stepX, stepY
    lowY, highY, stepY = Charts.scale(lowY, highY, 4)
    lowX, highX, stepX = Charts.scale(lowX, highX, 4)
    local left, top, width, height = self:plot(rect, lowY, highY, stepY)
    for value = lowX, highX + stepX * 0.5, stepX do
        self:text(Charts.format(value), left + (value - lowX) / (highX - lowX) * width, top + height + 16)
    end
    for _, point in ipairs(self.data) do
        local x, y = Charts.finite(point[1]), Charts.finite(point[2])
        if x and y then
            self:add({'circle', left + (x - lowX) / (highX - lowX) * width, top + height - (y - lowY) / (highY - lowY) * height, 5, color(1, 0.85), {layer = 2}})
        end
    end
end

function Chart:layoutRadar(rect)
    local values = self.data
    local count = #values
    if count < 3 then
        return self:message('A radar needs three axes', rect)
    end
    local maximum = Charts.finite(self.options.maximum)
    if not maximum then
        maximum = select(2, range({values})) or 0
    end
    if maximum <= 0 then
        maximum = 1
    end
    local radius = math.min(rect[3], rect[4]) / 2 - 30
    local centerX, centerY = rect[1] + rect[3] / 2, rect[2] + rect[4] / 2
    local function corner(index, share)
        local angle = -math.pi / 2 + (index - 1) / count * math.pi * 2
        return centerX + math.cos(angle) * radius * share, centerY + math.sin(angle) * radius * share
    end
    for ring = 1, 4 do
        local web = {}
        for index = 1, count do
            local x, y = corner(index, ring / 4)
            web[index] = {x, y}
        end
        self:add({'polyline', web, 1, Charts.colors.grid, true, {layer = 1}})
    end
    local labels = self.options.labels or {}
    local shape = {}
    for index = 1, count do
        local x, y = corner(index, 1)
        self:add({'line', centerX, centerY, x, y, 1, Charts.colors.grid, {layer = 1}})
        if labels[index] then
            local labelX, labelY = corner(index, 1.24)
            self:text(tostring(labels[index]), labelX, labelY)
        end
        -- Negative and missing values sit at the center, and values past the maximum at the edge.
        local share = math.max(0, math.min(1, (Charts.finite(values[index]) or 0) / maximum))
        local pointX, pointY = corner(index, share)
        shape[index] = {pointX, pointY}
    end
    self:add({'polygon', shape, color(5, 0.35), {layer = 2}})
    self:add({'polyline', shape, 2, color(5), true, {layer = 2}})
end

function Chart:layoutHistogram(rect)
    local values = {}
    for index = 1, #self.data do
        values[#values + 1] = Charts.finite(self.data[index])
    end
    if #values == 0 then
        return self:message('No data', rect)
    end
    local low, high = range({values})
    local bins = math.max(1, math.min(30, math.floor(self.options.bins or math.sqrt(#values))))
    if low == high then
        bins = 1
    end
    local counts = {}
    for index = 1, bins do
        counts[index] = 0
    end
    for _, value in ipairs(values) do
        local bin = low == high and 1 or math.min(bins, math.floor((value - low) / (high - low) * bins) + 1)
        counts[bin] = counts[bin] + 1
    end
    local _, top, step = Charts.scale(0, math.max(table.unpack(counts)), 4)
    local left, plotTop, width, height = self:plot(rect, 0, top, step)
    local slot = width / bins
    for index = 1, bins do
        local barHeight = counts[index] / top * height
        if barHeight > 0 then
            self:add({'shape', {left + slot * (index - 1) + 1, plotTop + height - barHeight, slot - 2, barHeight}, {color = color(6), layer = 2}})
        end
    end
    self:text(Charts.format(low), left, plotTop + height + 16, {0, 0.5})
    self:text(Charts.format(high), left + width, plotTop + height + 16, {1, 0.5})
end

function Chart:layoutCandles(rect)
    local candles = {}
    for _, entry in ipairs(self.data) do
        local open, high, low, close = Charts.finite(entry[1]), Charts.finite(entry[2]), Charts.finite(entry[3]), Charts.finite(entry[4])
        if open and high and low and close then
            -- A candle with its high and low swapped or inside its body still draws, with the wick around the body.
            candles[#candles + 1] = {open, math.max(open, close, high, low), math.min(open, close, high, low), close}
        else
            candles[#candles + 1] = false
        end
    end
    local low, high
    for _, candle in ipairs(candles) do
        if candle then
            low, high = math.min(low or candle[3], candle[3]), math.max(high or candle[2], candle[2])
        end
    end
    if not low then
        return self:message('No data', rect)
    end
    local step
    low, high, step = Charts.scale(low, high, 4)
    local left, top, width, height = self:plot(rect, low, high, step)
    local slot = width / #candles
    local function y(value)
        return top + height - (value - low) / (high - low) * height
    end
    for index, candle in ipairs(candles) do
        if candle then
            local x = left + slot * (index - 0.5)
            local tone = candle[4] >= candle[1] and Charts.colors.rise or Charts.colors.fall
            self:add({'line', x, y(candle[2]), x, y(candle[3]), 2, tone, {layer = 2}})
            local bodyTop, bodyBottom = y(math.max(candle[1], candle[4])), y(math.min(candle[1], candle[4]))
            self:add({'shape', {x - slot * 0.32, bodyTop, slot * 0.64, math.max(2, bodyBottom - bodyTop)}, {color = tone, layer = 3}})
        end
    end
end

-- Mixes the cold, middle and hot colors of the heat map for a share from 0 to 1.
local function heat(share)
    local cold, middle, hot = {0.17, 0.32, 0.62}, {0.95, 0.8, 0.3}, {0.88, 0.25, 0.22}
    local from, to, t = cold, middle, share * 2
    if share > 0.5 then
        from, to, t = middle, hot, (share - 0.5) * 2
    end
    return {from[1] + (to[1] - from[1]) * t, from[2] + (to[2] - from[2]) * t, from[3] + (to[3] - from[3]) * t, 1}
end

function Chart:layoutHeatmap(rect)
    local rows = self.data
    local low, high, columns = range(rows)
    if #rows == 0 or columns == 0 then
        return self:message('No data', rect)
    end
    local left, top = rect[1] + 8, rect[2] + 8
    local cellWidth, cellHeight = (rect[3] - 16) / columns, (rect[4] - 16) / #rows
    for row, values in ipairs(rows) do
        for column = 1, columns do
            local value = Charts.finite(values[column])
            local tone = Charts.colors.empty
            if value then
                tone = heat(high > low and (value - low) / (high - low) or 0.5)
            end
            self:add({'rect', {left + (column - 1) * cellWidth + 1, top + (row - 1) * cellHeight + 1, cellWidth - 2, cellHeight - 2}, tone, {layer = 2}})
        end
    end
end

function Chart:layout(rect)
    self.items = {}
    local kind = self.kind
    if kind == 'line' or kind == 'area' then
        self:layoutLines(rect, kind == 'area')
    elseif kind == 'bar' then
        self:layoutBars(rect)
    elseif kind == 'stackedBar' then
        self:layoutStacked(rect)
    elseif kind == 'pie' or kind == 'donut' then
        self:layoutPie(rect, kind == 'donut')
    elseif kind == 'scatter' then
        self:layoutScatter(rect)
    elseif kind == 'radar' then
        self:layoutRadar(rect)
    elseif kind == 'histogram' then
        self:layoutHistogram(rect)
    elseif kind == 'candles' then
        self:layoutCandles(rect)
    else
        self:layoutHeatmap(rect)
    end
    self.x, self.y, self.width, self.height = rect[1], rect[2], rect[3], rect[4]
end

-- Lays the chart out again only when it draws at another place or size than before.
function Chart:prepare(rect)
    if rect[1] ~= self.x or rect[2] ~= self.y or rect[3] ~= self.width or rect[4] ~= self.height then
        self:layout(rect)
    end
end

function Chart:draw(rect)
    self:prepare(rect)
    for _, item in ipairs(self.items) do
        local kind = item[1]
        if kind == 'line' then
            graphics2d.drawLine(item[2], item[3], item[4], item[5], item[6], item[7], item[8])
        elseif kind == 'polyline' then
            graphics2d.drawPolyline(item[2], item[3], item[4], item[5], item[6])
        elseif kind == 'polygon' then
            graphics2d.drawPolygon(item[2], item[3], item[4])
        elseif kind == 'circle' then
            graphics2d.drawCircle(item[2], item[3], item[4], item[5], item[6])
        elseif kind == 'shape' then
            graphics2d.drawShape(item[2], item[3])
        elseif kind == 'rect' then
            graphics2d.drawRect(item[2], item[3], item[4])
        elseif kind == 'arc' then
            graphics2d.drawArc(item[2], item[3], item[4], item[5], item[6], item[7], item[8], item[9])
        elseif kind == 'text' then
            graphics2d.drawText(nil, item[2], item[3], item[4], item[5])
        end
    end
end

-- Raises an error that names the chart when one of its laid out shapes holds a number that is not finite, and answers how many shapes it laid out.
function Chart:check(rect)
    self:layout(rect)
    local function visit(value, path)
        if type(value) == 'number' then
            if value ~= value or value == math.huge or value == -math.huge then
                error('The "' .. self.kind .. '" chart laid out a number that is not finite at ' .. path .. '.', 0)
            end
        elseif type(value) == 'table' then
            for key, inner in pairs(value) do
                visit(inner, path .. '.' .. tostring(key))
            end
        end
    end
    visit(self.items, 'items')
    return #self.items
end

return Charts
