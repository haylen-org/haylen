-- Primitives: a card for each drawing function with its name in code: lines of several widths and a turning hand, open and closed polylines, filled and outlined rectangles, circles from large to tiny, rings, arcs that fill up, a star, a hexagon and a concave polygon, meshes with vertex colors and a texture, shapes with a gradient, a border, a soft shadow and corners of their own, ellipses filled, outlined, cut into a sector and holding a portrait, strokes along a Bézier curve and a dashed loop, and a field of points that twinkle.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')

local SpriteTest = require('categories.sprites.sprite-test')
local Test = require('harness.test')

local Primitives = haylen.class('Primitives', SpriteTest)

Primitives.dotCount = 90
Primitives.names = {'drawLine', 'drawPolyline', 'drawRect', 'drawCircle', 'drawRing', 'drawArc', 'drawPolygon', 'drawMesh', 'drawShape', 'drawEllipse', 'drawStroke', 'drawPoints'}
Primitives.gap = 14
Primitives.titleHeight = 56

Primitives.code = [[
graphics2d.drawLine(x1, y1, x2, y2, 4, color)
graphics2d.drawPolyline(points, 3, color, true)
graphics2d.drawRectOutline(rect, 4, color)
graphics2d.drawCircle(x, y, 40, color)
graphics2d.drawRing(x, y, 40, 8, color)
graphics2d.drawArc(x, y, 40, 10, start, start + m.tau * progress, color)
graphics2d.drawPolygon(star, color)
graphics2d.drawMesh(nil, {{x = 0, y = 0, color = '#FFFF4060'}, ...}, {1, 2, 3})
graphics2d.drawShape(rect, {radius = 14, texture = gradient, borderWidth = 3, borderColor = border})
graphics2d.drawEllipse(x, y, 60, 30, color)
graphics2d.drawShape(rect, {kind = 'ellipse', startAngle = time, sweep = m.tau * 0.75, color = color})
graphics2d.drawStroke(m.spline(controls, {kind = 'bezier'}), {width = 6, cap = 'round', color = color})
graphics2d.drawPoints(points, {radius = 3, colors = colors})]]

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
    local gradient = graphics.newImage(8, 128)
    gradient:fillGradient({angle = math.pi / 2, stops = {{0, '#FF3A4E8C'}, {0.55, '#FF233056'}, {1, '#FF141A2E'}}})
    self.gradient = graphics.newTexture(gradient, {filter = 'linear'})
    self.titles = {}
    for index, name in ipairs(Primitives.names) do
        self.titles[index] = '[code]' .. name .. '[/code]'
    end
    self.dots, self.dotColors, self.dotSeeds = {}, {}, {}
    local random = m.random(11)
    for index = 1, Primitives.dotCount do
        self.dotSeeds[index] = {random:range(-1, 1), random:range(-1, 1), random:range(0, m.tau)}
        self.dotColors[index] = m.fromHsv(random:range(0, 1), 0.55, 1)
    end
    self.time = 0
    self:frame{code = Primitives.code, hint = 'Each card draws with the function named on it, and its shapes move with the time.'}
end

function Primitives:update(dt)
    Primitives.super.update(self, dt)
    self.time = self.time + dt
    self:status(string.format('Twelve functions draw these shapes in %d draw calls, the interface included.', graphics2d.stats().drawCalls))
end

-- Draws the shapes of one function around the center `x`, `y` of the room of a card, which reaches `rx` to each side and `ry` up and down.
function Primitives:cell(index, x, y, rx, ry)
    local time = self.time
    local reach = math.min(rx, ry)
    if index == 1 then
        for line = 0, 3 do
            local top = y - ry * 0.85 + line * ry * 0.18
            graphics2d.drawLine(x - rx * 0.85, top, x + rx * 0.85, top + ry * 0.1, 1 + line * 3, Test.accent)
        end
        local hand = math.min(rx, ry * 0.4) * 0.8
        local hubY = y + ry - hand - 12
        graphics2d.drawRing(x, hubY, hand + 8, 3, Test.line)
        graphics2d.drawLine(x, hubY, x + math.cos(time) * hand, hubY + math.sin(time) * hand, 5, Test.warm, {layer = 1})
        graphics2d.drawCircle(x, hubY, 7, Test.warm, {layer = 2})
    elseif index == 2 then
        local wave = {}
        for step = 0, 12 do
            wave[step + 1] = {x - rx * 0.9 + step * rx * 0.15, y - ry * 0.45 + math.sin(time * 3 + step * 0.8) * ry * 0.16}
        end
        graphics2d.drawPolyline(wave, 4, Test.green)
        graphics2d.drawPolyline(Primitives.star(x, y + ry * 0.42, reach * 0.48, reach * 0.22, time * 0.5), 4, Test.warm, true)
    elseif index == 3 then
        local side = reach * 1.05
        graphics2d.drawRect({x - rx * 0.85, y - ry * 0.7, side, side}, Test.accent)
        graphics2d.drawRectOutline({x + rx * 0.85 - side, y + ry * 0.7 - side, side, side}, 5, Test.warm, {layer = 1})
    elseif index == 4 then
        graphics2d.drawCircle(x - rx * 0.38, y - ry * 0.3, reach * 0.48, Test.accent)
        graphics2d.drawCircle(x + rx * 0.55, y - ry * 0.62, reach * 0.24, Test.warm)
        graphics2d.drawCircle(x + rx * 0.5, y + ry * 0.05, reach * 0.14 + math.sin(time * 2) * 5, Test.green)
        for dot = 1, 5 do
            graphics2d.drawCircle(x - rx * 0.8 + (dot - 1) * rx * 0.4, y + ry * 0.68, 1.5 + dot * 1.6, Test.ink)
        end
    elseif index == 5 then
        for ring = 1, 4 do
            local radius = reach * (0.2 + ring * 0.17) + math.sin(time * 2 + ring) * 4
            graphics2d.drawRing(x, y, radius, 3 + ring, m.fromHsv(ring / 5, 0.6, 1))
        end
    elseif index == 6 then
        local progress = (time * 0.35) % 1
        local start = -math.pi / 2
        graphics2d.drawRing(x, y, reach * 0.8, 12, Test.line)
        graphics2d.drawArc(x, y, reach * 0.8, 12, start, start + m.tau * progress, Test.green, {layer = 1})
        graphics2d.drawArc(x, y, reach * 0.48, 8, time * 2, time * 2 + 1.5, Test.warm)
        graphics2d.drawText(nil, string.format('%.0f%%', progress * 100), x, y, {size = 26, color = Test.ink, anchor = {0.5, 0.5}})
    elseif index == 7 then
        graphics2d.drawPolygon(Primitives.star(x - rx * 0.4, y - ry * 0.38, reach * 0.5, reach * 0.22, time * 0.3), Test.warm)
        graphics2d.drawPolygon(Primitives.regular(x + rx * 0.58, y - ry * 0.62, reach * 0.24, 6, time * 0.5), Test.green)
        local left, top, wide, tall, thick = x - rx * 0.1, y + ry * 0.12, rx * 0.95, ry * 0.68, reach * 0.26
        graphics2d.drawPolygon({{left, top}, {left + thick, top}, {left + thick, top + tall - thick}, {left + wide, top + tall - thick}, {left + wide, top + tall}, {left, top + tall}}, Test.accent)
    elseif index == 8 then
        graphics2d.drawMesh(nil, {
            {x = x - rx * 0.9, y = y - ry * 0.05, color = '#FFFF4060'},
            {x = x - rx * 0.35, y = y - ry * 0.85, color = '#FF40FF80'},
            {x = x + rx * 0.2, y = y - ry * 0.05, color = '#FF4080FF'},
        }, {1, 2, 3})
        local side = reach * 0.72
        local left, top, sway = x + rx * 0.85 - side, y + ry * 0.75 - side, math.sin(time * 2) * side * 0.2
        graphics2d.drawMesh(self.hero, {
            {x = left, y = top, u = 0, v = 0},
            {x = left + side + sway, y = top - side * 0.1, u = 1, v = 0},
            {x = left + side, y = top + side, u = 1, v = 1},
            {x = left - sway, y = top + side, u = 0, v = 1},
        }, {1, 2, 3, 1, 3, 4})
    elseif index == 9 then
        local card = {x - rx * 0.85, y - ry * 0.85, rx * 1.7, ry * 0.62}
        graphics2d.drawShape({card[1], card[2] + 8, card[3], card[4]}, {radius = 14, color = '#90000000', softness = 18})
        graphics2d.drawShape(card, {radius = 14, texture = self.gradient, borderWidth = 3, borderColor = Test.accent})
        graphics2d.drawShape({x - rx * 0.7, y + ry * 0.02 - 11, rx * 1.4, 22}, {radius = 11, rotation = math.sin(time) * 0.35, color = Test.warm})
        local bubble = {x - rx * 0.8, y + ry * 0.36, rx * 1.4, ry * 0.42}
        graphics2d.drawShape(bubble, {radius = {16, 16, 16, 3}, color = Test.green})
        for dot = -1, 1 do
            graphics2d.drawCircle(bubble[1] + bubble[3] / 2 + dot * 18, bubble[2] + bubble[4] / 2, 5, '#FF1E3B2C', {layer = 1})
        end
    elseif index == 10 then
        graphics2d.drawEllipse(x - rx * 0.45, y - ry * 0.72, rx * 0.4, ry * 0.13, Test.accent)
        graphics2d.drawShape({x - rx * 0.83, y - ry * 0.05 - rx * 0.2, rx * 0.76, rx * 0.4}, {kind = 'ellipse', rotation = time * 0.6, color = '#00000000', borderWidth = 5, borderColor = Test.warm})
        graphics2d.drawShape({x - rx * 0.85, y + ry * 0.4, rx * 0.8, ry * 0.5}, {kind = 'ellipse', startAngle = time, sweep = m.tau * 0.75, color = Test.green})
        local portrait = {x + rx * 0.08, y - ry * 0.5, rx * 0.8, ry * 1.0}
        graphics2d.drawShape(portrait, {kind = 'ellipse', color = Test.surface})
        graphics2d.drawShape(portrait, {kind = 'ellipse', texture = self.hero, borderWidth = 4, borderColor = Test.accent, layer = 1})
    elseif index == 11 then
        local swing = math.sin(time * 1.5) * ry * 0.3
        local curve = m.spline({{x - rx * 0.85, y - ry * 0.3}, {x - rx * 0.35, y - ry * 0.85 - swing}, {x + rx * 0.35, y - ry * 0.25 + swing}, {x + rx * 0.85, y - ry * 0.7}}, {kind = 'bezier'})
        graphics2d.drawStroke(curve, {width = 6, cap = 'round', color = Test.warm})
        local loop = m.spline({{x - rx * 0.8, y + ry * 0.45}, {x, y + ry * 0.15}, {x + rx * 0.8, y + ry * 0.45}, {x, y + ry * 0.78}}, {closed = true})
        graphics2d.drawStroke(loop, {width = 4, color = Test.green, dashes = {14, 8}, dashOffset = -time * 40, cap = 'round'})
    else
        -- Every point drifts around its place, and its color dims and brightens with its own phase.
        for dot, seed in ipairs(self.dotSeeds) do
            self.dots[dot * 2 - 1] = x + seed[1] * rx * 0.85 + math.sin(time + seed[3]) * 6
            self.dots[dot * 2] = y + seed[2] * ry * 0.85 + math.cos(time * 0.8 + seed[3]) * 6
            self.dotColors[dot].a = 0.45 + 0.55 * (0.5 + 0.5 * math.sin(time * 3 + seed[3]))
        end
        graphics2d.drawPoints(self.dots, {radius = 3.5, colors = self.dotColors})
    end
end

-- Lays the cards in six columns on a wide stage and in fewer on a taller one, with the name of each function in code at its top.
function Primitives:draw(area)
    local columns = area.width > area.height * 1.6 and 6 or (area.width > area.height * 0.8 and 4 or 3)
    local rows = math.ceil(#Primitives.names / columns)
    local gap, titleHeight = Primitives.gap, Primitives.titleHeight
    local width, height = (area.width - gap * (columns + 1)) / columns, (area.height - gap * (rows + 1)) / rows
    for index in ipairs(Primitives.names) do
        local column, row = (index - 1) % columns, (index - 1) // columns
        local left, top = gap + column * (width + gap), gap + row * (height + gap)
        graphics2d.drawShape({left, top, width, height}, {radius = 14, color = '#FF1C2130', borderWidth = 2, borderColor = Test.line, layer = -1})
        graphics2d.drawRichText(self.titles[index], left, top + 14, {size = 26, color = Test.ink, maxWidth = width, align = 'center'})
        local roomTop, roomBottom = top + titleHeight, top + height - 14
        self:cell(index, left + width / 2, (roomTop + roomBottom) / 2, width / 2 - 18, (roomBottom - roomTop) / 2)
    end
end

return Primitives
