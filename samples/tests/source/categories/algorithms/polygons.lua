-- Polygon booleans and offsets: a ring you drag over a star, combined by union, difference, intersection or exclusion, grown or shrunk with a corner style and split into convex pieces.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local profiler = require('haylen.debug')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')

local Polygons = haylen.class('Polygons', AlgorithmTest)

Polygons.fill = '#FF2F86F6'

Polygons.code = [[
local ring = {outside, inside}  -- An outline and a hole wound the other way.
local result = m.polygon.subtract(star, ring)  -- Or unite, intersect and exclude.
result = m.polygon.offset(result, -12, {join = 'miter'})  -- Shrinking can split it apart.
local pieces = m.polygon.decompose(result)  -- Convex pieces of at most 8 points.
local area = m.polygon.area(result)
graphics2d.drawPolygon(result, fill)  -- The holes stay open, and only the outlines fade.]]

local kOperations = {{id = 'unite', text = 'Union'}, {id = 'subtract', text = 'Difference'}, {id = 'intersect', text = 'Intersection'}, {id = 'exclude', text = 'Exclusion'}}
local kJoins = {{id = 'round', text = 'Round corners'}, {id = 'miter', text = 'Miter corners'}, {id = 'square', text = 'Square corners'}, {id = 'bevel', text = 'Bevel corners'}}

local function star(x, y, points, outer, inner)
    local outline = {}
    for index = 0, points * 2 - 1 do
        local radius = index % 2 == 0 and outer or inner
        local angle = index * math.pi / points - math.pi / 2
        outline[#outline + 1] = m.vec2(x + math.cos(angle) * radius, y + math.sin(angle) * radius)
    end
    return outline
end

-- A ring is an outline with a hole wound the other way.
local function ring(x, y, outer, inner)
    local outside, inside = {}, {}
    for index = 0, 47 do
        local angle = index / 48 * math.pi * 2
        outside[#outside + 1] = m.vec2(x + math.cos(angle) * outer, y + math.sin(angle) * outer)
        inside[#inside + 1] = m.vec2(x + math.cos(-angle) * inner, y + math.sin(-angle) * inner)
    end
    return {outside, inside}
end

function Polygons:enter()
    self:frame{
        hint = 'Drag with the pointer to move the ring over the star, whose outlines show dashed. The result fills blue, with white outlines around it and yellow ones around its holes. The offset grows it or shrinks it, and shrinking can split it apart.',
        code = Polygons.code,
        controls = {
            ui.formField{label = 'Operation', ui.radioGroup{id = 'operation', items = kOperations, selected = 'subtract', onChange = function(event) self:change('operation', event.value) end}},
            ui.formField{label = 'Offset', ui.slider{id = 'offset', value = 0, min = -40, max = 40, step = 2, showValue = true, decimals = 0, onChange = function(event) self:change('offset', event.value) end}},
            ui.formField{label = 'Corners of the offset', ui.combo{id = 'join', items = kJoins, selected = 'round', onChange = function(event) self:change('join', event.value) end}},
            ui.checkbox{id = 'pieces', text = 'Convex pieces', onChange = function(event) self:change('pieces', event.checked) end},
        },
        focus = 'operation',
    }
    self.options = {operation = 'subtract', offset = 0, join = 'round', pieces = false}
    self.star = star(-120, 0, 7, 320, 150)
    self.starStroke = Polygons.flat(self.star)
    self.center = {180, 40}
    self:combine()
end

function Polygons:change(name, value)
    self.options[name] = value
    self:combine()
end

function Polygons:combine()
    local options = self.options
    self.ring = ring(self.center[1], self.center[2], 220, 110)
    self.ringStrokes = {Polygons.flat(self.ring[1]), Polygons.flat(self.ring[2])}
    profiler.beginScope('polygonBooleans')
    local result = m.polygon[options.operation](self.star, self.ring)
    if options.offset ~= 0 then
        result = m.polygon.offset(result, options.offset, {join = options.join})
    end
    self.result = result
    self.pieces = #result > 0 and m.polygon.decompose(result) or {}
    profiler.endScope()
    self.pieceColors = {}
    for index in ipairs(self.pieces) do
        self.pieceColors[index] = m.fromHsv((index * 0.618) % 1, 0.45, 0.9)
    end
    self.outlines = {}
    for index, outline in ipairs(self.result) do
        self.outlines[index] = {points = Polygons.flat(outline), hole = m.polygonSignedArea(outline) < 0}
    end
end

-- Turns a list of points into the flat list of coordinates that strokes take.
function Polygons.flat(points)
    local flat = {}
    for _, point in ipairs(points) do
        flat[#flat + 1] = point.x
        flat[#flat + 1] = point.y
    end
    return flat
end

function Polygons:update(dt)
    Polygons.super.update(self, dt)
    if self.pointer.down then
        self.center = {self.pointer.worldX, self.pointer.worldY}
        self:combine()
    end
    self:status(string.format('Outlines %d, area %.0f square units, convex pieces %d, combined in %.3f ms', #self.result, m.polygon.area(self.result), #self.pieces, self:timing('polygonBooleans')))
end

function Polygons:draw(area)
    local dashed = {width = 3, color = '#99FFFFFF', closed = true, join = 'round', cap = 'round', dashes = {14, 10}, layer = 4}
    graphics2d.drawStroke(self.starStroke, dashed)
    for _, outline in ipairs(self.ringStrokes) do
        graphics2d.drawStroke(outline, dashed)
    end
    -- The result fills as one shape with its holes open, or as its convex pieces in colors of their own.
    if self.options.pieces then
        for index, piece in ipairs(self.pieces) do
            graphics2d.drawPolygon(piece, self.pieceColors[index], {layer = 1})
            graphics2d.drawStroke(Polygons.flat(piece), {width = 2, color = '#CC15172A', closed = true, join = 'round', layer = 2})
        end
    else
        graphics2d.drawPolygon(self.result, Polygons.fill, {layer = 1})
    end
    for _, outline in ipairs(self.outlines) do
        graphics2d.drawStroke(outline.points, {width = 4, color = outline.hole and '#FFFFC93C' or '#FFF2F3FF', closed = true, join = 'round', layer = 3})
    end
end

return Polygons
