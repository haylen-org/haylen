-- Measures the bridge to the native part of the plugin `native-demo`: the round trip of one call awaited after the other, the calls per second when many wait at once, the work that a frame spends on N calls sent in it, and the work of the frames that receive N batched events. The numbers show as bars and go to the log with the code of the test, so a run on a device, a simulator or a browser records them.
local async = require('async')
local datetime = require('datetime')
local debug = require('haylen.debug')
local haylen = require('haylen')
local ui = require('haylen.ui')

local DemoTest = require('categories.plugins.demo-test')
local demo = require('native-demo')

local Benchmark = haylen.class('Benchmark', DemoTest)

Benchmark.roundTrips = 200
Benchmark.parallel = 2000
Benchmark.perFrame = {10, 100, 1000}
Benchmark.frames = 60
Benchmark.events = {10, 100, 1000}
Benchmark.steps = {'the round trip', 'the calls per second', 'the frames with calls', 'the frames with events'}

function Benchmark:enter()
    self.connections = {
        demo.onBurst(function(list) self.received = (self.received or 0) + #list end),
    }
    self:frame{
        hint = 'Measure again on a quiet device for steadier numbers.',
        focus = 'run',
        controls = {
            ui.button{id = 'run', text = 'Measure again', variant = 'primary', onClick = function() self:run() end},
            ui.label{text = 'A round trip waits for the answer before the next call, so it holds the wait for the next frame of the engine. Calls per second send every call at once and wait for all of them. The work of a frame comes from "debug.frame", averaged over 60 frames.', color = 'textMuted', font = 'caption'},
        },
        showcase = {
            ui.row{gap = 16, align = 'stretch',
                Benchmark.figure('roundTrip', 'Round trip of one call'),
                Benchmark.figure('throughput', 'Calls per second'),
            },
            Benchmark.chart({'None', '10', '100', '1000'}),
            ui.progress{id = 'step', value = 0, text = 'Waiting to measure'},
        },
        code = "local started = datetime.now():millis()\nfor i = 1, 200 do demo.echo(i):await() end\nprint((datetime.now():millis() - started) / 200)",
    }
    self:run()
end

-- A card with one big number under its name.
function Benchmark.figure(id, name)
    return ui.card{grow = 1, align = 'stretch', gap = 4,
        ui.label{text = name, color = 'textMuted'},
        ui.label{id = id, text = '–', font = 'title'},
    }
end

-- A card with a bar for the calls and a bar for the events of each count, which grow with the milliseconds of their frames.
function Benchmark.chart(counts)
    local rows = {
        ui.label{text = 'Mean work of a frame', font = 'heading'},
        ui.row{gap = 12, ui.spacer{width = 110}, ui.label{text = 'With N calls', color = 'textMuted', grow = 1}, ui.label{text = 'With N events', color = 'textMuted', grow = 1}},
    }
    for index, count in ipairs(counts) do
        rows[#rows + 1] = ui.row{gap = 12,
            ui.label{text = count, width = 110, textAlign = 'end', color = 'textMuted'},
            ui.progress{id = 'calls' .. index, value = 0, text = '–', grow = 1},
            ui.progress{id = 'events' .. index, value = 0, text = '–', tone = 'information', grow = 1},
        }
    end
    return ui.card{grow = 1, align = 'stretch', gap = 12, children = rows}
end

-- Sets the bars of a chart, the longest for the most work.
function Benchmark:showChart(id, values)
    local most = 0
    for _, value in ipairs(values) do
        most = math.max(most, value)
    end
    for index, value in ipairs(values) do
        self:set(id .. index, {value = most > 0 and value / most or 0, text = string.format('%.2f ms', value)})
    end
end

function Benchmark:exit()
    for _, connection in ipairs(self.connections) do
        connection:disconnect()
    end
    Benchmark.super.exit(self)
end

function Benchmark.now()
    return datetime.now():millis()
end

-- Waits inside a task until the engine runs the next frame.
function Benchmark.nextFrame()
    local frame = haylen.frameIndex()
    repeat
        async.sleep(1):await()
    until haylen.frameIndex() ~= frame
end

-- The mean work of the next frames, while `each` runs once in every one of them.
function Benchmark.frameWork(frames, each)
    local total = 0
    for _ = 1, frames do
        if each then
            each()
        end
        Benchmark.nextFrame()
        total = total + debug.frame().milliseconds
    end
    return total / frames
end

-- Shows which step runs, and keeps the button off until every step ends.
function Benchmark:step(index)
    local done = index > #Benchmark.steps
    self:set('step', {value = (index - 1) / #Benchmark.steps, text = done and 'Every measure is done' or string.format('Measuring %s, step %d of %d', Benchmark.steps[index], index, #Benchmark.steps)})
    self:set('run', {text = done and 'Measure again' or 'Measuring…', enabled = done})
end

function Benchmark:run()
    self:act(function()
        self.results:clear()
        self:step(1)
        if not self:roundTrip() then
            return self:step(#Benchmark.steps + 1)
        end
        self:step(2)
        self:throughput()
        self:step(3)
        self:callsPerFrame()
        self:step(4)
        self:eventsPerFrame()
        self:step(#Benchmark.steps + 1)
    end)
end

function Benchmark:roundTrip()
    local name = 'Round trip of one call'
    self.results:set('roundTrip', 'waiting', name, 'Measuring.')
    local started = Benchmark.now()
    for index = 1, Benchmark.roundTrips do
        local _, err = demo.echo(index):await()
        if err then
            self.results:failure('roundTrip', name, err)
            return false
        end
    end
    local each = (Benchmark.now() - started) / Benchmark.roundTrips
    self:set('roundTrip', {text = string.format('%.2f ms', each)})
    self.results:set('roundTrip', 'pass', name, string.format('%.2f ms per call over %d calls awaited one after the other, at %.0f frames per second.', each, Benchmark.roundTrips, debug.frame().fps))
    return true
end

function Benchmark:throughput()
    local name = 'Calls per second'
    self.results:set('throughput', 'waiting', name, 'Measuring.')
    local started = Benchmark.now()
    local calls = {}
    for index = 1, Benchmark.parallel do
        calls[index] = demo.echo(index)
    end
    local _, err = async.all(calls):await()
    if err then
        self.results:set('throughput', 'fail', name, tostring(err.message or err))
        return
    end
    local seconds = math.max(Benchmark.now() - started, 1) / 1000
    self:set('throughput', {text = string.format('%.0f', Benchmark.parallel / seconds)})
    self.results:set('throughput', 'pass', name, string.format('%.0f calls per second, %d calls sent at once in %.0f ms.', Benchmark.parallel / seconds, Benchmark.parallel, seconds * 1000))
end

function Benchmark:callsPerFrame()
    local name = 'Frame work with N calls'
    self.results:set('calls', 'waiting', name, 'Measuring.')
    local base = Benchmark.frameWork(Benchmark.frames)
    local values = {base}
    local parts = {string.format('no calls %.2f ms', base)}
    for _, count in ipairs(Benchmark.perFrame) do
        local pending = {}
        local work = Benchmark.frameWork(Benchmark.frames, function()
            for index = 1, count do
                pending[#pending + 1] = demo.echo(index)
            end
        end)
        async.all(pending):await()
        values[#values + 1] = work
        parts[#parts + 1] = string.format('%d calls %.2f ms', count, work)
        self:showChart('calls', values)
    end
    self.results:set('calls', 'pass', name, 'The mean work of a frame with ' .. table.concat(parts, ', ') .. '.')
end

-- The native part sends the batched events 30 times per second, so the frames that receive a list alternate with frames that do not, and the mean holds both.
function Benchmark:eventsPerFrame()
    local name = 'Frame work with N events'
    self.results:set('events', 'waiting', name, 'Measuring.')
    local base = Benchmark.frameWork(Benchmark.frames)
    local values = {base}
    local parts = {string.format('no events %.2f ms', base)}
    for _, count in ipairs(Benchmark.events) do
        self.received = 0
        local started, err = demo.burst(count, 60):await()
        if err then
            self.results:failure('events', name, err)
            return
        end
        local work = Benchmark.frameWork(Benchmark.frames)
        values[#values + 1] = work
        parts[#parts + 1] = string.format('%d events 30 times per second %.2f ms with %d received', started.count, work, self.received)
        self:showChart('events', values)
        async.sleep(2000):await()
    end
    self.results:set('events', 'pass', name, 'The mean work of a frame with ' .. table.concat(parts, ', ') .. '.')
end

return Benchmark
