-- Primitives: lines of several widths, open and closed polylines, filled and outlined rectangles, circles with their segment counts, rings, arcs that fill up, concave polygons and meshes with vertex colors and a texture.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')

local sample = require('sample')

local Primitives = haylen.class('Primitives', sample.Test)

local kNames = {'drawLine', 'drawPolyline', 'drawRect', 'drawCircle', 'drawRing', 'drawArc', 'drawPolygon', 'drawMesh'}
local kCode = [[
graphics2d.drawLine(x1, y1, x2, y2, 4, color)  graphics2d.drawPolyline(points, 3, color, true)  graphics2d.drawRectOutline(rect, 4, color)
graphics2d.drawCircle(x, y, 40, color, nil, 6)  graphics2d.drawRing(x, y, 40, 8, color)  graphics2d.drawArc(x, y, 40, 10, start, start + m.tau * progress, color)
graphics2d.drawPolygon(star, color)  graphics2d.drawMesh(nil, {{x = 0, y = 0, color = '#FFFF0000'}, ...}, {1, 2, 3})]]

-- The points of a star around a center.
local function star(x, y, outer, inner, turn)
    local points = {}
    for index = 0, 9 do
        local radius = index % 2 == 0 and outer or inner
        local angle = turn + index * math.pi / 5
        points[index + 1] = {x + math.cos(angle) * radius, y + math.sin(angle) * radius}
    end
    return points
end

function Primitives:enter()
    self.hero = sample.texture('images/hero.png')
    self.time = 0
    self:frame({code = kCode})
end

function Primitives:update(dt)
    Primitives.super.update(self, dt)
    self.time = self.time + dt
    self:status(string.format('Time %.1f', self.time))
end

-- Draws one cell of the grid: its title and the shapes of one function around the center of the cell.
function Primitives:cell(index, x, y, size)
    local time = self.time
    local half = size * 0.36
    if index == 1 then
        for line = 0, 3 do
            graphics2d.drawLine(x - half, y - half + line * 24, x + half, y - half + line * 24 + 10, 1 + line * 3, sample.accent)
        end
        local angle = time
        graphics2d.drawLine(x, y + 40, x + math.cos(angle) * half, y + 40 + math.sin(angle) * half * 0.5, 4, sample.warm)
    elseif index == 2 then
        local wave = {}
        for step = 0, 12 do
            wave[step + 1] = {x - half + step * half / 6, y - 30 + math.sin(time * 3 + step * 0.8) * 20}
        end
        graphics2d.drawPolyline(wave, 3, sample.green)
        graphics2d.drawPolyline(star(x, y + 45, 40, 18, time * 0.5), 3, sample.warm, true)
    elseif index == 3 then
        graphics2d.drawRect({x - half, y - half, half * 1.2, half * 1.2}, sample.accent)
        graphics2d.drawRectOutline({x - half * 0.3, y - half * 0.3, half * 1.3, half * 1.3}, 4, sample.warm, {layer = 1})
    elseif index == 4 then
        graphics2d.drawCircle(x - half * 0.5, y - 10, half * 0.55, sample.accent)
        graphics2d.drawCircle(x + half * 0.55, y - 20, 28, sample.warm, nil, 6)
        graphics2d.drawCircle(x + half * 0.3, y + 45, 22, sample.green, nil, 3)
    elseif index == 5 then
        for ring = 1, 4 do
            local radius = ring * 16 + math.sin(time * 2 + ring) * 4
            graphics2d.drawRing(x, y, radius, 3 + ring, m.fromHsv(ring / 5, 0.6, 1))
        end
    elseif index == 6 then
        local progress = (time * 0.35) % 1
        local start = -math.pi / 2
        graphics2d.drawRing(x, y, half * 0.9, 12, sample.line)
        graphics2d.drawArc(x, y, half * 0.9, 12, start, start + m.tau * progress, sample.green, {layer = 1})
        graphics2d.drawArc(x, y, half * 0.5, 8, time * 2, time * 2 + 1.5, sample.warm)
        graphics2d.drawText(nil, string.format('%.0f%%', progress * 100), x, y, {size = 24, color = sample.ink, anchor = {0.5, 0.5}})
    elseif index == 7 then
        graphics2d.drawPolygon(star(x - 30, y - 20, half * 0.8, half * 0.35, time * 0.3), sample.warm)
        graphics2d.drawPolygon({{x + 10, y + 10}, {x + half, y + 10}, {x + half, y + 30}, {x + 34, y + 30}, {x + 34, y + half}, {x + 10, y + half}}, sample.accent)
    else
        local reach = half * 0.8
        graphics2d.drawMesh(nil, {
            {x = x - reach * 1.2, y = y + reach * 0.4, color = '#FFFF4060'},
            {x = x - reach * 0.2, y = y - reach * 1.2, color = '#FF40FF80'},
            {x = x + reach * 0.4, y = y + reach * 0.4, color = '#FF4080FF'},
        }, {1, 2, 3})
        local left, top = x + 10, y - 10
        graphics2d.drawMesh(self.hero, {
            {x = left, y = top, u = 0, v = 0},
            {x = left + 80 + math.sin(time * 2) * 20, y = top - 10, u = 1, v = 0},
            {x = left + 80, y = top + 80, u = 1, v = 1},
            {x = left - math.sin(time * 2) * 20, y = top + 80, u = 0, v = 1},
        }, {1, 2, 3, 1, 3, 4})
    end
end

function Primitives:draw(area)
    local columns, rows = 4, 2
    local width, height = area.width / columns, area.height / rows
    for index = 1, #kNames do
        local column, row = (index - 1) % columns, (index - 1) // columns
        local x, y = column * width + width / 2, row * height + height / 2 + 16
        graphics2d.drawRectOutline({column * width + 6, row * height + 6, width - 12, height - 12}, 1, sample.line)
        graphics2d.drawText(nil, 'Function "' .. kNames[index] .. '"', x, row * height + 28, {size = 24, color = sample.ink, anchor = {0.5, 0.5}})
        self:cell(index, x, y, math.min(width, height))
    end
end

return Primitives
