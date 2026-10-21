-- Poisson disk sampling whose spacing follows a density: a noise map sampled on a worker thread by `procedural2d.scatterAsync`, or a distance function around a point you pick with `math.poissonDisk`.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local procedural2d = require('haylen.procedural2d')
local profiler = require('haylen.debug')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local picture = require('categories.algorithms.picture')

local Poisson = haylen.class('Poisson', AlgorithmTest)

Poisson.code = [[
local density = {seed = 1, frequency = 0.0035, octaves = 3}
local options = {region = area, method = 'poisson', spacing = 14, maximumSpacing = 70, densityMap = density}
local points = procedural2d.scatterAsync(options):await()  -- On a worker thread.
local spacing = function(point) return m.lerp(14, 70, m.smoothstep(0, 400, point:distance({0, 0}))) end
local crowded = m.poissonDisk({area = area, minimumDistance = 14, maximumDistance = 70, seed = 1, distance = spacing})]]

local kArea = {-720, -390, 1440, 780}
local kNearest, kFarthest = 14, 70
local kResampleDelay = 0.25

function Poisson:enter()
    self:frame{
        hint = 'With the focus mode, tap or click to pick where points crowd together. Points are never closer than the spacing wanted where each of them stands. R or the X button takes a new seed.',
        code = Poisson.code,
        controls = {
            ui.formField{label = 'Density', ui.radioGroup{id = 'mode', items = {{id = 'noise', text = 'Density from noise'}, {id = 'focus', text = 'Density around a focus'}}, selected = 'noise', onChange = function(event)
                self.mode = event.value
                self.dirty = true
            end}},
            ui.formField{label = 'Spread of the focus', ui.slider{id = 'spread', value = 400, min = 100, max = 900, step = 25, showValue = true, decimals = 0, onChange = function(event)
                self.spread = event.value
                self.dirty = true
            end}},
            ui.label{text = 'Each point shows the room it keeps around it, warm where points crowd and cool where they spread out.', color = 'textMuted', font = 'caption'},
            ui.button{id = 'seed', text = 'New seed', onClick = function()
                self.seed = self.seed + 1
                self.dirty = true
            end},
        },
        focus = 'mode',
    }
    self.mode, self.spread, self.seed = 'noise', 400, 1
    self.focusPoint = {0, 0}
    self.points = {}
    self.rooms, self.tints = {}, {}
    self.dot, self.dotShape = {0, 0, 10, 10}, {kind = 'ellipse', color = '#FFE8EAF2', borderWidth = 2, borderColor = '#FF15172A', layer = 2}
    self.wait = 0
    self:sample()
end

-- Every sample takes a new request number, so a worker result that lands after a newer sample is dropped.
function Poisson:sample()
    self.request = (self.request or 0) + 1
    if self.mode == 'focus' then
        self:sampleAroundFocus()
    else
        self:sampleNoise()
    end
end

-- Spacing grows from the nearest spacing at the focus to the farthest one a spread away.
function Poisson:sampleAroundFocus()
    local fx, fy, spread = self.focusPoint[1], self.focusPoint[2], self.spread
    profiler.beginScope('poissonDisk')
    self.points = m.poissonDisk({
        area = kArea,
        minimumDistance = kNearest,
        maximumDistance = kFarthest,
        seed = self.seed,
        distance = function(point)
            local distance = math.sqrt((point.x - fx) ^ 2 + (point.y - fy) ^ 2)
            return m.lerp(kNearest, kFarthest, m.smoothstep(0, spread, distance))
        end,
    })
    profiler.endScope()
    self.latency = nil
    self:measure()
end

-- Finds for every point the distance to its nearest neighbour through a grid of buckets, which the draw turns into the room around it.
function Poisson:measure()
    local cell, buckets = kFarthest, {}
    local function key(x, y)
        return math.floor((x - kArea[1]) / cell) * 4096 + math.floor((y - kArea[2]) / cell)
    end
    for _, point in ipairs(self.points) do
        local bucket = key(point.x, point.y)
        buckets[bucket] = buckets[bucket] or {}
        table.insert(buckets[bucket], point)
    end
    self.rooms, self.tints = {}, {}
    for index, point in ipairs(self.points) do
        local best = kFarthest * kFarthest
        local column, row = math.floor((point.x - kArea[1]) / cell), math.floor((point.y - kArea[2]) / cell)
        for dx = -1, 1 do
            for dy = -1, 1 do
                for _, other in ipairs(buckets[(column + dx) * 4096 + row + dy] or {}) do
                    if other ~= point then
                        best = math.min(best, (other.x - point.x) ^ 2 + (other.y - point.y) ^ 2)
                    end
                end
            end
        end
        self.rooms[index] = math.sqrt(best)
        self.tints[index] = m.color(picture.heat(1 - (self.rooms[index] - kNearest) / (kFarthest - kNearest))):withAlpha(0.28)
    end
end

function Poisson:sampleNoise()
    local request, started = self.request, haylen.elapsed()
    self:spawn(function()
        local points = procedural2d.scatterAsync({region = kArea, method = 'poisson', spacing = kNearest, maximumSpacing = kFarthest, densityMap = {seed = self.seed, frequency = 0.0035, octaves = 3}, seed = self.seed}):await()
        if points and request == self.request then
            self.points = points
            self.latency = (haylen.elapsed() - started) * 1000
            self:measure()
        end
    end)
end

function Poisson:update(dt)
    Poisson.super.update(self, dt)
    if input.pressed('reset') then
        self.seed = self.seed + 1
        self.dirty = true
    end
    if self.mode == 'focus' and self.pointer.pressed then
        self.focusPoint = {self.pointer.worldX, self.pointer.worldY}
        self.dirty = true
    end

    -- Sliders report every step of a drag, so changes sample again at most a few times per second.
    self.wait = math.max(0, self.wait - dt)
    if self.dirty and self.wait == 0 then
        self.dirty, self.wait = false, kResampleDelay
        self:sample()
    end
    local timing = self.latency and string.format('worker thread %.0f ms', self.latency) or string.format('function "poissonDisk" %.1f ms', self:timing('poissonDisk'))
    self:status(string.format('%d points spaced from %d to %d, %s', #self.points, kNearest, kFarthest, timing))
end

function Poisson:draw(area)
    graphics2d.drawShape({kArea[1] - 24, kArea[2] - 24, kArea[3] + 48, kArea[4] + 48}, {radius = 16, color = '#FF141920', borderWidth = 3, borderColor = '#FF4A5578'})
    -- The rooms of the points end at the frame, so none spills over it.
    graphics2d.pushClip({kArea[1] - 21, kArea[2] - 21, kArea[3] + 42, kArea[4] + 42})
    for index, point in ipairs(self.points) do
        if self.rooms[index] then
            graphics2d.drawCircle(point.x, point.y, self.rooms[index] / 2, self.tints[index], {layer = 1})
        end
        local dot = self.dot
        dot[1], dot[2] = point.x - 5, point.y - 5
        graphics2d.drawShape(dot, self.dotShape)
    end
    graphics2d.popClip()
    if self.mode == 'focus' then
        local ring = {}
        for step = 0, 63 do
            ring[#ring + 1] = self.focusPoint[1] + math.cos(step / 64 * math.pi * 2) * self.spread
            ring[#ring + 1] = self.focusPoint[2] + math.sin(step / 64 * math.pi * 2) * self.spread
        end
        graphics2d.drawStroke(ring, {width = 3, color = '#AAFFC93C', closed = true, dashes = {14, 10}, layer = 3})
        graphics2d.drawShape({self.focusPoint[1] - 10, self.focusPoint[2] - 10, 20, 20}, {kind = 'ellipse', color = '#FFFFC93C', borderWidth = 3, borderColor = '#FF15172A', layer = 3})
    end
end

return Poisson
