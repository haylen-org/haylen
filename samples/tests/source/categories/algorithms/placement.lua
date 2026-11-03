-- A set count of houses, trees and rocks placed on a worker thread over a meadow, each kept apart by the ground it really occupies, a rectangle, the circle of a trunk or the outline of a rock, and away from a lake and a road, with how many of each kind found no room.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local procedural2d = require('haylen.procedural2d')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Art = require('categories.algorithms.art')

local Placement = haylen.class('Placement', AlgorithmTest)

Placement.code = [[
local map = procedural2d.placeAsync({area = {-760, -320, 1520, 720}, order = 'listed', seed = seed,
    kinds = {{count = 5, shape = {kind = 'rectangle', width = 120, height = 70}, spacing = 40, margin = 30},
        {count = trees, shape = {kind = 'circle', radius = 10, center = {0, -4}}, spacing = 8, margin = 20},
        {count = rocks, shape = {kind = 'polygon', points = rock}, rotate = true, spacing = 4}},
    forbidden = {lake, road}}):await()
print(#map.entities, map.missing[2])  -- What found no room, for each kind.]]

local kMeadow = {-760, -400, 1520, 800}
-- The trees stand on their trunks, so the area of the placement leaves room above it for their crowns.
local kArea = {-760, -320, 1520, 720}
local kLake = {kind = 'circle', center = {-260, 20}, radius = 150}
local kRoad = {kind = 'rectangle', center = {0, 330}, width = 1520, height = 70}
local kRock = {{-20, -6}, {-6, -16}, {16, -12}, {22, 4}, {8, 14}, {-16, 10}}
local kNames = {'houses', 'trees', 'rocks'}
local kFootprint = '#FFFF5A5A'
local kFootprintLayer = 2000

function Placement:enter()
    self:frame{
        hint = 'Every change places again on a worker thread. Ask for more trees and rocks than fit, and the counts left out show in the status line. R or the X button takes a new seed.',
        code = Placement.code,
        controls = {
            ui.formField{label = 'Trees', ui.slider{id = 'trees', value = 60, min = 0, max = 400, step = 10, showValue = true, decimals = 0, onChange = function(event) self:change('trees', event.value) end}},
            ui.formField{label = 'Rocks', ui.slider{id = 'rocks', value = 60, min = 0, max = 400, step = 10, showValue = true, decimals = 0, onChange = function(event) self:change('rocks', event.value) end}},
            ui.formField{label = 'Order', ui.radioGroup{id = 'order', items = {{id = 'listed', text = 'Houses, then trees, then rocks'}, {id = 'shuffled', text = 'Every kind mixed'}}, selected = 'listed', onChange = function(event) self:change('order', event.value) end}},
            ui.checkbox{id = 'footprints', text = 'Show the footprints', checked = true, onChange = function(event) self.footprints = event.checked end},
            ui.button{id = 'seed', text = 'New seed', onClick = function() self:change('seed', self.options.seed + 1) end},
        },
        focus = 'trees',
    }
    self.options = {trees = 60, rocks = 60, order = 'listed', seed = 1}
    self.footprints = true
    self.map = {entities = {}, missing = {0, 0, 0}}
    self.art = Art()
    self.tree = assets.texture('sprites/images/tree.png', {filter = 'linear', mipmaps = true})
    self.places = assets.texture('algorithms/places.png', {filter = 'linear', mipmaps = true})
    self.treeOptions = {width = 54, height = 81, pivotY = 1, layer = 0}
    self.houseOptions = {source = {256, 0, 256, 256}, width = 150, height = 150, pivotY = 0.82, layer = 0}
    self.outline = {}
    self:place()
end

function Placement:change(name, value)
    self.options[name] = value
    self:place()
end

function Placement:describe()
    local options = self.options
    return {
        area = kArea,
        order = options.order,
        seed = options.seed,
        kinds = {
            {count = 5, shape = {kind = 'rectangle', width = 120, height = 70}, spacing = 40, margin = 30},
            {count = options.trees, shape = {kind = 'circle', radius = 10, center = {0, -4}}, spacing = 8, margin = 20},
            {count = options.rocks, shape = {kind = 'polygon', points = kRock}, rotate = true, spacing = 4},
        },
        forbidden = {kLake, kRoad},
    }
end

-- Only the latest request lands, so dragging a slider never shows an older map over a newer one.
function Placement:place()
    self.request = (self.request or 0) + 1
    local request, started = self.request, haylen.elapsed()
    self:spawn(function()
        local map = procedural2d.placeAsync(self:describe()):await()
        if map and request == self.request then
            self.map = map
            self.latency = (haylen.elapsed() - started) * 1000
        end
    end)
end

function Placement:update(dt)
    Placement.super.update(self, dt)
    if input.pressed('reset') then
        self:change('seed', self.options.seed + 1)
    end
    local counts = {0, 0, 0}
    for _, entity in ipairs(self.map.entities) do
        counts[entity.kind] = counts[entity.kind] + 1
    end
    local missing = self.map.missing
    self:status(string.format('Placed %d %s, %d %s and %d %s, and left out %d, %d and %d, in %.0f ms.', counts[1], kNames[1], counts[2], kNames[2], counts[3], kNames[3], missing[1], missing[2], missing[3], self.latency or 0))
end

-- Draws the outline of a rock turned by the rotation of its entity, over the rock.
function Placement:drawRock(entity, depth)
    local outline, cosine, sine = self.outline, math.cos(entity.rotation), math.sin(entity.rotation)
    for index, point in ipairs(kRock) do
        outline[index * 2 - 1] = entity.x + point[1] * cosine - point[2] * sine
        outline[index * 2] = entity.y + point[1] * sine + point[2] * cosine
    end
    self.art:prop('rock', entity.x, entity.y + 8, 40, depth)
    if self.footprints then
        graphics2d.drawStroke(outline, {width = 3, color = kFootprint, closed = true, join = 'round', layer = kFootprintLayer})
    end
end

-- The meadow holds the lake and the road, and every object stands in rows from the back to the front, with its footprint over it.
function Placement:draw(area)
    graphics2d.drawShape(kMeadow, {radius = 18, color = '#FF6DB04A', layer = -3})
    local lake = kLake
    graphics2d.drawShape({lake.center[1] - lake.radius, lake.center[2] - lake.radius, lake.radius * 2, lake.radius * 2}, {kind = 'ellipse', color = '#FF2FB3C4', borderWidth = 10, borderColor = '#FFF3D58D', layer = -2})
    graphics2d.drawRect({kMeadow[1], kRoad.center[2] - kRoad.height / 2, kRoad.width, kRoad.height}, '#FFC9A26B', {layer = -2})

    for _, entity in ipairs(self.map.entities) do
        local depth = Placement.depth(entity.y)
        if entity.kind == 1 then
            self.houseOptions.layer = depth
            graphics2d.draw(self.places, entity.x, entity.y + 35, self.houseOptions)
            if self.footprints then
                graphics2d.drawRectOutline({entity.x - 60, entity.y - 35, 120, 70}, 3, kFootprint, {layer = kFootprintLayer})
            end
        elseif entity.kind == 2 then
            self.treeOptions.layer = depth
            graphics2d.draw(self.tree, entity.x, entity.y + 4, self.treeOptions)
            if self.footprints then
                graphics2d.drawRing(entity.x, entity.y - 4, 10, 3, kFootprint, {layer = kFootprintLayer})
            end
        else
            self:drawRock(entity, depth)
        end
    end
end

-- Objects lower on the meadow draw in front of the ones behind them.
function Placement.depth(y)
    return math.floor(y + 500)
end

return Placement
