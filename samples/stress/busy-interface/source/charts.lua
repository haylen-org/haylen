-- The charts of the dashboard, drawn with "haylen.graphics2d" in the spaces the GUI leaves open for them: a line chart of the moves of the cranes, bars of the berths and a ring of the share of every crane. Each chart sits on a panel that a nine-slice of the panel art draws, in the colors of the cards of the theme.
local graphics2d = require('haylen.graphics2d')
local m = require('haylen.math')

local Charts = {}

Charts.text = '#FF8A9AAD'
Charts.grid = '#FF263243'
Charts.line = '#FF3C9BE0'
Charts.fill = '#403C9BE0'
Charts.target = '#FFE8A93A'
Charts.tones = {neutral = '#FF6B7A8F', information = '#FF3AA8E0', accent = '#FF3C9BE0', warning = '#FFE8A93A', success = '#FF35B37E', danger = '#FFE5534B'}
Charts.palette = {'#FF3C9BE0', '#FF35B37E', '#FFE8A93A', '#FFE5534B', '#FFA87FE0', '#FF4FC9C4', '#FFF07E4C', '#FF9BB4CC'}
-- The space the title label of a chart takes at its top, and the inner padding of the panel.
Charts.header = 52
Charts.padding = 16

function Charts.panel(slice, rect)
    graphics2d.drawNineSlice(slice, rect, nil, {layer = 0}, 0.5)
end

local function inner(rect)
    local padding = Charts.padding
    return rect.x + padding, rect.y + Charts.header, rect.width - padding * 2, rect.height - Charts.header - padding
end

local function label(text, x, y, anchor)
    graphics2d.drawText(nil, text, x, y, {size = 17, color = Charts.text, anchor = anchor, layer = 2})
end

-- A line of `values` with the area under it, a dashed target level and four grid lines labeled with their values.
function Charts.lineChart(rect, values, target)
    local x, y, width, height = inner(rect)
    x, width = x + 44, width - 44
    if #values < 2 or width <= 0 or height <= 0 then
        return
    end
    local low, high = target, target
    for _, value in ipairs(values) do
        low, high = math.min(low, value), math.max(high, value)
    end
    local span = math.max(1, high - low)
    low, high = low - span * 0.1, high + span * 0.1

    for line = 0, 3 do
        local lineY = y + height * line / 3
        graphics2d.drawLine(x, lineY, x + width, lineY, 1, Charts.grid, {layer = 1})
        label(string.format('%.0f', high - (high - low) * line / 3), x - 8, lineY, {1, 0.5})
    end
    local targetY = y + height * (1 - (target - low) / (high - low))
    for dash = 0, width - 12, 18 do
        graphics2d.drawLine(x + dash, targetY, x + dash + 10, targetY, 2, Charts.target, {layer = 2})
    end

    local points, area = {}, {}
    local step = width / (#values - 1)
    for index, value in ipairs(values) do
        local point = {x + (index - 1) * step, y + height * (1 - (value - low) / (high - low))}
        points[index] = point
        area[index] = point
    end
    area[#area + 1] = {x + width, y + height}
    area[#area + 1] = {x, y + height}
    graphics2d.drawPolygon(area, Charts.fill, {layer = 1})
    graphics2d.drawPolyline(points, 3, Charts.line, false, {layer = 2})
    local last = points[#points]
    graphics2d.drawCircle(last[1], last[2], 5, Charts.line, {layer = 3})
end

-- One bar for every value from 0 to 1, colored by the tone next to it.
function Charts.barChart(rect, values, tones, names)
    local x, y, width, height = inner(rect)
    height = height - 22
    local count = #values
    if count == 0 or width <= 0 or height <= 0 then
        return
    end
    local slot = width / count
    local bar = slot * 0.62
    graphics2d.drawLine(x, y + height, x + width, y + height, 1, Charts.grid, {layer = 1})
    for index, value in ipairs(values) do
        local barX = x + (index - 1) * slot + (slot - bar) / 2
        local barHeight = math.max(2, height * m.clamp(value, 0, 1))
        graphics2d.drawRect({barX, y, bar, height}, Charts.grid, {layer = 1})
        graphics2d.drawRect({barX, y + height - barHeight, bar, barHeight}, Charts.tones[tones[index]], {layer = 2})
        label(names[index], barX + bar / 2, y + height + 6, {0.5, 0})
    end
end

-- A ring of the shares of `values`, with the total in its middle.
function Charts.ringChart(rect, values, total, caption)
    local x, y, width, height = inner(rect)
    local radius = math.min(width, height) / 2 - 12
    if radius <= 8 then
        return
    end
    local centerX, centerY = x + width / 2, y + height / 2
    local sum = 0
    for _, value in ipairs(values) do
        sum = sum + value
    end
    local angle = -math.pi / 2
    local thickness = math.max(8, radius * 0.2)
    graphics2d.drawRing(centerX, centerY, radius - thickness / 2, thickness, Charts.grid, {layer = 1})
    if sum > 0 then
        for index, value in ipairs(values) do
            local sweep = math.pi * 2 * value / sum
            graphics2d.drawArc(centerX, centerY, radius - thickness / 2, thickness, angle + 0.01, angle + sweep - 0.01, Charts.palette[(index - 1) % #Charts.palette + 1], {layer = 2})
            angle = angle + sweep
        end
    end
    graphics2d.drawText(nil, total, centerX, centerY - 6, {size = 30, color = '#FFE6ECF2', anchor = {0.5, 0.5}, layer = 2})
    label(caption, centerX, centerY + 18, {0.5, 0})
end

return Charts
