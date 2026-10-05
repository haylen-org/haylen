-- Polygon booleans and offsets: a ring you drag over a star, combined by union, difference, intersection or exclusion, grown or shrunk with a corner style and split into convex pieces.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local profiler = require('haylen.debug')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')

local Polygons = haylen.class('Polygons', AlgorithmTest)

local kOperations = {{id = 'unite', text = 'Union'}, {id = 'subtract', text = 'Difference'}, {id = 'intersect', text = 'Intersection'}, {id = 'exclude', text = 'Exclusion'}}
local kJoins = {{id = 'round', text = 'Round corners'}, {id = 'miter', text = 'Miter corners'}, {id = 'square', text = 'Square corners'}, {id = 'bevel', text = 'Bevel corners'}}

local function star(x, y, points, outer, inner)
    local outline = {}
    for index = 0, points * 2 - 1 do
        local radius = index % 2 == 0 and outer or inner
        local angle = index * math.pi / points - math.pi / 2
        outline[#outline + 1] = {x + math.cos(angle) * radius, y + math.sin(angle) * radius}
    end
    return outline
end

-- A ring is an outline with a hole wound the other way.
local function ring(x, y, outer, inner)
    local outside, inside = {}, {}
    for index = 0, 47 do
        local angle = index / 48 * math.pi * 2
        outside[#outside + 1] = {x + math.cos(angle) * outer, y + math.sin(angle) * outer}
        inside[#inside + 1] = {x + math.cos(-angle) * inner, y + math.sin(-angle) * inner}
    end
    return {outside, inside}
end

function Polygons:enter()
    self:frame{
        hint = 'Drag with the pointer to move the ring over the star. The offset grows the result or shrinks it, and shrinking can split it apart.',
        controls = {
            ui.radioGroup{id = 'operation', items = kOperations, selected = 'subtract', onChange = function(event) self:change('operation', event.value) end},
            ui.label{text = 'Offset', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'offset', value = 0, min = -40, max = 40, step = 2, showValue = true, decimals = 0, onChange = function(event) self:change('offset', event.value) end},
            ui.combo{id = 'join', items = kJoins, selected = 'round', onChange = function(event) self:change('join', event.value) end},
            ui.checkbox{id = 'pieces', text = 'Convex pieces', onChange = function(event) self:change('pieces', event.checked) end},
        },
        focus = 'operation',
    }
    self.options = {operation = 'subtract', offset = 0, join = 'round', pieces = false}
    self.star = star(-120, 0, 7, 320, 150)
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
    profiler.beginScope('polygon booleans')
    local result = m.polygon[options.operation](self.star, self.ring)
    if options.offset ~= 0 then
        result = m.polygon.offset(result, options.offset, {join = options.join})
    end
    self.result = result
    self.pieces = #result > 0 and m.polygon.decompose(result) or {}
    profiler.endScope()
end

function Polygons:update(dt)
    Polygons.super.update(self, dt)
    if self.pointer.down then
        self.center = {self.pointer.worldX, self.pointer.worldY}
        self:combine()
    end
    self:status(string.format('Outlines %d, area %.0f, convex pieces %d, booleans %.3f ms', #self.result, m.polygon.area(self.result), #self.pieces, self:timing('polygon booleans')))
end

function Polygons:draw(area)
    graphics2d.drawPolyline(self.star, 2, '#66FFFFFF', true)
    for _, outline in ipairs(self.ring) do
        graphics2d.drawPolyline(outline, 2, '#66FFFFFF', true)
    end
    for index, piece in ipairs(self.pieces) do
        local color = self.options.pieces and m.fromHsv((index * 0.618) % 1, 0.5, 0.85) or '#FF4FC3F7'
        graphics2d.drawPolygon(piece, color, {layer = 1})
        if self.options.pieces then
            graphics2d.drawPolyline(piece, 1.5, '#AA1A2029', true, {layer = 2})
        end
    end
    for _, outline in ipairs(self.result) do
        graphics2d.drawPolyline(outline, 3, m.polygonSignedArea(outline) >= 0 and '#FFFFFFFF' or '#FFFFD54F', true, {layer = 3})
    end
end

return Polygons
