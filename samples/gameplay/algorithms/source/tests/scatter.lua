-- Objects scattered on a worker thread inside an island polygon: random by area and density, on a jittered grid or with Poisson spacing, kept out of lakes and a village, and typed by weights and noise biomes.
local haylen = require('haylen')
local async = require('async')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local procedural2d = require('haylen.procedural2d')
local ui = require('haylen.ui')

local sample = require('sample')

local Scatter = haylen.class('Scatter', sample.Test)

local kIsland = {{-700, -120}, {-560, -340}, {-200, -390}, {160, -330}, {520, -380}, {720, -160}, {640, 160}, {700, 330}, {300, 390}, {-120, 330}, {-480, 380}, {-720, 200}}
local kLakes = {{center = {-300, -60}, radius = 110}, {center = {330, 150}, radius = 80}}
local kVillage = {60, -200, 220, 150}
local kTypes = {
    {name = 'trees', color = '#FF2E7D32', radius = 9},
    {name = 'bushes', color = '#FF66BB6A', radius = 6},
    {name = 'rocks', color = '#FF90A4AE', radius = 7},
    {name = 'flowers', color = '#FFF48FB1', radius = 4},
}

function Scatter:enter()
    Scatter.super.enter(self, {
        hint = 'Every change scatters again on a worker thread. Biomes give forests of trees and bushes and meadows of rocks and flowers, and density follows noise.',
        controls = {
            ui.radioGroup{id = 'method', items = {{id = 'random', text = 'Random by density'}, {id = 'grid', text = 'Jittered grid'}, {id = 'poisson', text = 'Poisson spacing'}}, selected = 'poisson', onChange = function(event) self:change('method', event.value) end},
            ui.label{text = 'Density', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'density', value = 1, min = 0.2, max = 3, step = 0.1, showValue = true, onChange = function(event) self:change('density', event.value) end},
            ui.checkbox{id = 'exclude', text = 'Exclude lakes and village', checked = true, onChange = function(event) self:change('exclude', event.checked) end},
            ui.checkbox{id = 'biomes', text = 'Biomes by noise', checked = true, onChange = function(event) self:change('biomes', event.checked) end},
            ui.button{id = 'seed', text = 'New seed', onClick = function() self:change('seed', self.options.seed + 1) end},
        },
        stats = true,
        focus = 'method',
    })
    self.options = {method = 'poisson', density = 1, exclude = true, biomes = true, seed = 1}
    self.points = {}
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
    local request, started = self.request, haylen.time()
    async.spawn(function()
        local points = procedural2d.scatterAsync(self:describe()):await()
        if points and self.options and request == self.request then
            self.points = points
            self.latency = (haylen.time() - started) * 1000
        end
    end)
end

function Scatter:exit()
    Scatter.super.exit(self)
    self.points, self.options = nil, nil
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
    self:showStats(string.format('points %d\ntrees %d, bushes %d\nrocks %d, flowers %d\nready in %.0f ms', #self.points, counts[1], counts[2], counts[3], counts[4], self.latency or 0))
end

function Scatter:render()
    self:beginWorld()
    graphics2d.drawPolygon(kIsland, '#FF3E5C3A')
    graphics2d.drawPolyline(kIsland, 4, '#FFD7C28F', true)
    if self.options.exclude then
        for _, lake in ipairs(kLakes) do
            graphics2d.drawCircle(lake.center[1], lake.center[2], lake.radius, '#FF1E4E79', {layer = 1})
        end
        graphics2d.drawRect(kVillage, '#FF8D6E63', {layer = 1})
    end
    for _, point in ipairs(self.points) do
        local kind = kTypes[point.type]
        graphics2d.drawCircle(point.x, point.y, kind.radius, kind.color, {layer = 2}, 8)
    end
end

return Scatter
