-- Objects scattered on a worker thread inside an island polygon: random by area and density, on a jittered grid or with Poisson spacing, kept out of lakes and a village, and typed by weights and noise biomes.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local procedural2d = require('haylen.procedural2d')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Art = require('categories.algorithms.art')

local Scatter = haylen.class('Scatter', AlgorithmTest)

Scatter.code = [[
local points = procedural2d.scatterAsync({  -- Points of x, y and a type from 1.
    region = {polygon = island}, method = 'poisson', spacing = 22, maximumSpacing = 90,
    densityMap = {seed = 1, frequency = 0.004}, weights = {3, 2, 1, 2},
    exclude = {{center = {-300, -60}, radius = 110}, {60, -200, 220, 150}},
    biome = {seed = 8, frequency = 0.003},
    layers = {{minimum = -1, maximum = 0, weights = {5, 3, 0, 0}}, {minimum = 0, maximum = 1, weights = {0, 1, 2, 4}}},
}):await()]]

local kIsland = {{-700, -120}, {-560, -340}, {-200, -390}, {160, -330}, {520, -380}, {720, -160}, {640, 160}, {700, 330}, {300, 390}, {-120, 330}, {-480, 380}, {-720, 200}}
local kLakes = {{center = {-300, -60}, radius = 110}, {center = {330, 150}, radius = 80}}
local kVillage = {60, -200, 220, 150}
-- The kinds of object, each drawn as a painted prop standing on its point, and the flowers as small blossoms.
local kTypes = {
    {name = 'trees', size = 46},
    {name = 'bushes', prop = 'bush', size = 34},
    {name = 'rocks', prop = 'rock', size = 34},
    {name = 'flowers', size = 7},
}

function Scatter:enter()
    self:frame{
        hint = 'Every change scatters again on a worker thread. Biomes give forests of trees and bushes and meadows of rocks and flowers, and density follows noise. R or the X button takes a new seed.',
        code = Scatter.code,
        controls = {
            ui.formField{label = 'Spread', ui.radioGroup{id = 'method', items = {{id = 'random', text = 'Random by density'}, {id = 'grid', text = 'Jittered grid'}, {id = 'poisson', text = 'Poisson spacing'}}, selected = 'poisson', onChange = function(event) self:change('method', event.value) end}},
            ui.formField{label = 'Density', ui.slider{id = 'density', value = 1, min = 0.2, max = 3, step = 0.1, showValue = true, onChange = function(event) self:change('density', event.value) end}},
            ui.checkbox{id = 'exclude', text = 'Exclude lakes and village', checked = true, onChange = function(event) self:change('exclude', event.checked) end},
            ui.checkbox{id = 'biomes', text = 'Biomes by noise', checked = true, onChange = function(event) self:change('biomes', event.checked) end},
            ui.button{id = 'seed', text = 'New seed', onClick = function() self:change('seed', self.options.seed + 1) end},
        },
        focus = 'method',
    }
    self.options = {method = 'poisson', density = 1, exclude = true, biomes = true, seed = 1}
    self.points = {}
    self.art = Art()
    self.tree = assets.texture('sprites/images/tree.png', {filter = 'linear', mipmaps = true})
    self.places = assets.texture('algorithms/places.png', {filter = 'linear', mipmaps = true})
    self.treeOptions = {width = 0, height = 0, pivotY = 1, layer = 0}
    self.islandStroke = {}
    for _, point in ipairs(kIsland) do
        self.islandStroke[#self.islandStroke + 1] = point[1]
        self.islandStroke[#self.islandStroke + 1] = point[2]
    end
    self:scatter()
end

function Scatter:change(name, value)
    self.options[name] = value
    self:scatter()
end

function Scatter:describe()
    local options = self.options
    local description = {region = {polygon = kIsland}, method = options.method, seed = options.seed, weights = {3, 2, 1, 2}}
    if options.method == 'random' then
        description.density = 0.0008 * options.density
        description.densityMap = {seed = options.seed, frequency = 0.004}
    elseif options.method == 'grid' then
        description.spacing = 36 / math.sqrt(options.density)
        description.jitter = 0.8
    else
        description.spacing = 22 / math.sqrt(options.density)
        description.maximumSpacing = 90 / math.sqrt(options.density)
        description.densityMap = {seed = options.seed, frequency = 0.004}
    end
    if options.exclude then
        description.exclude = {kLakes[1], kLakes[2], kVillage}
    end
    if options.biomes then
        description.biome = {seed = options.seed + 7, frequency = 0.003, octaves = 3}
        description.layers = {{minimum = -1, maximum = 0, weights = {5, 3, 0, 0}}, {minimum = 0, maximum = 1, weights = {0, 1, 2, 4}}}
    end
    return description
end

-- Only the latest request lands, so dragging a slider never shows an older scatter over a newer one.
function Scatter:scatter()
    self.request = (self.request or 0) + 1
    local request, started = self.request, haylen.elapsed()
    self:spawn(function()
        local points = procedural2d.scatterAsync(self:describe()):await()
        if points and request == self.request then
            self.points = points
            self.latency = (haylen.elapsed() - started) * 1000
        end
    end)
end

function Scatter:update(dt)
    Scatter.super.update(self, dt)
    if input.pressed('reset') then
        self:change('seed', self.options.seed + 1)
    end
    local counts = {0, 0, 0, 0}
    for _, point in ipairs(self.points) do
        counts[point.type] = counts[point.type] + 1
    end
    self:status(string.format('%d points, %d trees, %d bushes, %d rocks and %d flowers, scattered in %.0f ms', #self.points, counts[1], counts[2], counts[3], counts[4], self.latency or 0))
end

-- The island lies in the sea with a beach around it, its lakes have a sandy shore, the village is a cottage, and the objects stand in rows from the back to the front.
function Scatter:draw(area)
    graphics2d.drawShape({-790, -440, 1580, 880}, {radius = 18, color = '#FF2A8FB0', layer = -3})
    graphics2d.drawPolygon(kIsland, '#FF6DB04A', {layer = -2})
    graphics2d.drawStroke(self.islandStroke, {width = 22, color = '#FFF3D58D', closed = true, join = 'round', layer = -2})
    if self.options.exclude then
        for _, lake in ipairs(kLakes) do
            local radius = lake.radius
            graphics2d.drawShape({lake.center[1] - radius, lake.center[2] - radius, radius * 2, radius * 2}, {kind = 'ellipse', color = '#FF2FB3C4', borderWidth = 10, borderColor = '#FFF3D58D', layer = -1})
        end
        local village = kVillage
        graphics2d.draw(self.places, village[1] + village[3] / 2, village[2] + village[4], {source = {256, 0, 256, 256}, width = 170, height = 170, pivotY = 0.97, layer = Scatter.depth(village[2] + village[4])})
    end
    local options = self.treeOptions
    for _, point in ipairs(self.points) do
        local kind = kTypes[point.type]
        local depth = Scatter.depth(point.y)
        if kind.prop then
            self.art:prop(kind.prop, point.x, point.y + kind.size * 0.15, kind.size, depth)
        elseif kind.name == 'trees' then
            options.width, options.height, options.layer = kind.size, kind.size * 1.5, depth
            graphics2d.draw(self.tree, point.x, point.y + 4, options)
        else
            graphics2d.drawCircle(point.x, point.y, kind.size, (point.x + point.y) % 2 < 1 and '#FFFFFFFF' or '#FFF48FB1', {layer = depth})
            graphics2d.drawCircle(point.x, point.y, kind.size * 0.4, '#FFFFC93C', {layer = depth})
        end
    end
end

-- Objects lower on the island draw in front of the ones behind them.
function Scatter.depth(y)
    return math.floor(y + 500)
end

return Scatter
