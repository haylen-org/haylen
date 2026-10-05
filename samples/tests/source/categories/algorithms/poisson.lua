-- Poisson disk sampling whose spacing follows a density: a noise map sampled on a worker thread by `procedural2d.scatterAsync`, or a distance function around a point you pick with `math.poissonDisk`.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local procedural2d = require('haylen.procedural2d')
local profiler = require('haylen.debug')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')

local Poisson = haylen.class('Poisson', AlgorithmTest)

local kArea = {-720, -390, 1440, 780}
local kNearest, kFarthest = 14, 70
local kResampleDelay = 0.25

function Poisson:enter()
    self:frame{
        hint = 'With the focus mode, tap or click to pick where points crowd together. Points are never closer than the spacing wanted where each of them stands. R or the X button takes a new seed.',
        controls = {
            ui.radioGroup{id = 'mode', items = {{id = 'noise', text = 'Density from noise'}, {id = 'focus', text = 'Density around a focus'}}, selected = 'noise', onChange = function(event)
                self.mode = event.value
                self.dirty = true
            end},
            ui.label{text = 'Spread of the focus', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'spread', value = 400, min = 100, max = 900, step = 25, showValue = true, decimals = 0, onChange = function(event)
                self.spread = event.value
                self.dirty = true
            end},
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
    profiler.beginScope('poisson disk')
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
end

function Poisson:sampleNoise()
    local request, started = self.request, haylen.elapsed()
    self:spawn(function()
        local points = procedural2d.scatterAsync({region = kArea, method = 'poisson', spacing = kNearest, maximumSpacing = kFarthest, densityMap = {seed = self.seed, frequency = 0.0035, octaves = 3}, seed = self.seed}):await()
        if points and request == self.request then
            self.points = points
            self.latency = (haylen.elapsed() - started) * 1000
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
    local timing = self.latency and string.format('worker thread %.0f ms', self.latency) or string.format('function "poissonDisk" %.1f ms', self:timing('poisson disk'))
    self:status(string.format('Points %d, spacing %d to %d, %s', #self.points, kNearest, kFarthest, timing))
end

function Poisson:draw(area)
    graphics2d.drawRect(kArea, '#FF141920')
    for _, point in ipairs(self.points) do
        graphics2d.drawCircle(point.x, point.y, 4, '#FF80CBC4')
    end
    if self.mode == 'focus' then
        graphics2d.drawRing(self.focusPoint[1], self.focusPoint[2], self.spread, 2, '#66FFD54F', {layer = 1})
        graphics2d.drawCircle(self.focusPoint[1], self.focusPoint[2], 8, '#FFFFD54F', {layer = 1})
    end
end

return Poisson
