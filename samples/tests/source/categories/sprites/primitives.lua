-- Primitives: lines of several widths, open and closed polylines, filled and outlined rectangles, circles of every size, rings, arcs that fill up, concave and regular polygons, meshes with vertex colors and a texture, and shapes: rounded rectangles with borders, a soft shadow, a turning capsule and a pill rounded on one side.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')

local SpriteTest = require('categories.sprites.sprite-test')
local Test = require('harness.test')

local Primitives = haylen.class('Primitives', SpriteTest)

Primitives.names = {'drawLine', 'drawPolyline', 'drawRect', 'drawCircle', 'drawRing', 'drawArc', 'drawPolygon', 'drawMesh', 'drawShape'}

Primitives.code = [[
graphics2d.drawLine(x1, y1, x2, y2, 4, color)  graphics2d.drawPolyline(points, 3, color, true)  graphics2d.drawRectOutline(rect, 4, color)
graphics2d.drawCircle(x, y, 40, color)  graphics2d.drawRing(x, y, 40, 8, color)  graphics2d.drawArc(x, y, 40, 10, start, start + m.tau * progress, color)
graphics2d.drawPolygon(star, color)  graphics2d.drawMesh(nil, {{x = 0, y = 0, color = '#FFFF0000'}, ...}, {1, 2, 3})
graphics2d.drawShape(rect, {radius = 14, color = color, borderWidth = 3, borderColor = border, softness = 0, rotation = 0})]]

-- The points of a regular polygon around a center, with its first corner at the angle of the turn.
function Primitives.regular(x, y, radius, sides, turn)
    local points = {}
    for index = 0, sides - 1 do
        local angle = turn + index * m.tau / sides
        points[index + 1] = {x + math.cos(angle) * radius, y + math.sin(angle) * radius}
    end
    return points
end

-- The points of a star around a center.
function Primitives.star(x, y, outer, inner, turn)
    local points = {}
    for index = 0, 9 do
        local radius = index % 2 == 0 and outer or inner
        local angle = turn + index * math.pi / 5
        points[index + 1] = {x + math.cos(angle) * radius, y + math.sin(angle) * radius}
    end
    return points
end

function Primitives:enter()
    self.hero = SpriteTest.texture('images/hero.png')
    self.time = 0
    self:frame{code = Primitives.code, hint = 'Each cell draws with the function named on it, and the shapes move with the time.'}
end

function Primitives:update(dt)
    Primitives.super.update(self, dt)
    self.time = self.time + dt
    self:status(string.format('Time %.1f', self.time))
end

-- Draws the shapes of one function around the center of a cell.
function Primitives:cell(index, x, y, size)
    local time = self.time
    local half = size * 0.36
    if index == 1 then
        for line = 0, 3 do
            graphics2d.drawLine(x - half, y - half + line * 24, x + half, y - half + line * 24 + 10, 1 + line * 3, Test.accent)
        end
        graphics2d.drawLine(x, y + 40, x + math.cos(time) * half, y + 40 + math.sin(time) * half * 0.5, 4, Test.warm)
    elseif index == 2 then
        local wave = {}
        for step = 0, 12 do
            wave[step + 1] = {x - half + step * half / 6, y - 30 + math.sin(time * 3 + step * 0.8) * 20}
        end
        graphics2d.drawPolyline(wave, 3, Test.green)
        graphics2d.drawPolyline(Primitives.star(x, y + 45, 40, 18, time * 0.5), 3, Test.warm, true)
    elseif index == 3 then
        graphics2d.drawRect({x - half, y - half, half * 1.2, half * 1.2}, Test.accent)
        graphics2d.drawRectOutline({x - half * 0.3, y - half * 0.3, half * 1.3, half * 1.3}, 4, Test.warm, {layer = 1})
    elseif index == 4 then
        graphics2d.drawCircle(x - half * 0.5, y - 10, half * 0.55, Test.accent)
        graphics2d.drawCircle(x + half * 0.55, y - 20, 28, Test.warm)
        graphics2d.drawCircle(x + half * 0.3, y + 45, 14 + math.sin(time * 2) * 6, Test.green)
        graphics2d.drawCircle(x - half * 0.6, y + 50, 3, Test.ink)
    elseif index == 5 then
        for ring = 1, 4 do
            local radius = ring * 16 + math.sin(time * 2 + ring) * 4
            graphics2d.drawRing(x, y, radius, 3 + ring, m.fromHsv(ring / 5, 0.6, 1))
        end
    elseif index == 6 then
        local progress = (time * 0.35) % 1
        local start = -math.pi / 2
        graphics2d.drawRing(x, y, half * 0.9, 12, Test.line)
        graphics2d.drawArc(x, y, half * 0.9, 12, start, start + m.tau * progress, Test.green, {layer = 1})
        graphics2d.drawArc(x, y, half * 0.5, 8, time * 2, time * 2 + 1.5, Test.warm)
        Test.caption(string.format('%.0f%%', progress * 100), x, y, {size = 24, color = Test.ink, anchor = {0.5, 0.5}})
    elseif index == 7 then
        graphics2d.drawPolygon(Primitives.star(x - 30, y - 20, half * 0.8, half * 0.35, time * 0.3), Test.warm)
        graphics2d.drawPolygon({{x + 10, y + 10}, {x + half, y + 10}, {x + half, y + 30}, {x + 34, y + 30}, {x + 34, y + half}, {x + 10, y + half}}, Test.accent)
        graphics2d.drawPolygon(Primitives.regular(x + half * 0.55, y - half * 0.55, 24, 6, time * 0.5), Test.green)
    elseif index == 8 then
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
    else
        local card = {x - half, y - half * 0.85, half * 2, half}
        graphics2d.drawShape({card[1], card[2] + 8, card[3], card[4]}, {radius = 14, color = '#90000000', softness = 18})
        graphics2d.drawShape(card, {radius = 14, color = Test.surface, borderWidth = 3, borderColor = Test.accent})
        graphics2d.drawShape({x - half * 0.8, y + half * 0.45 - 10, half * 1.6, 20}, {radius = 10, rotation = math.sin(time) * 0.4, color = Test.warm})
        graphics2d.drawShape({x - half, y + half * 0.8 - 14, half, 28}, {radius = {14, 0, 0, 14}, color = Test.green})
    end
end

function Primitives:draw(area)
    local columns, rows = 5, 2
    local width, height = area.width / columns, area.height / rows
    for index, name in ipairs(Primitives.names) do
        local column, row = (index - 1) % columns, (index - 1) // columns
        local x, y = column * width + width / 2, row * height + height / 2 + 16
        graphics2d.drawRectOutline({column * width + 6, row * height + 6, width - 12, height - 12}, 1, Test.line)
        Test.caption('Function "' .. name .. '"', x, row * height + 28, {size = 24, color = Test.ink, anchor = {0.5, 0.5}})
        self:cell(index, x, y, math.min(width, height))
    end
end

return Primitives
