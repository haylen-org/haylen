-- Measures the bridge to the native part of the plugin `native-demo`: the round trip of one call awaited after the other, the calls per second when many wait at once, the work that a frame spends on N calls sent in it, and the work of the frames that receive N batched events. The numbers go to the log with the code of the test, so a run on a device, a simulator or a browser records them.
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

function Benchmark:enter()
    self.connections = {
        demo.onBurst(function(list) self.received = (self.received or 0) + #list end),
    }
    self:frame{
        hint = 'Run the measurements again on a quiet device for steadier numbers.',
        focus = 'run',
        controls = {
            ui.button{id = 'run', text = 'Measure again', variant = 'primary', onClick = function() self:run() end},
            ui.label{text = 'A round trip waits for the answer before the next call, so it holds the wait for the next frame of the engine. Calls per second send every call at once and wait for all of them. The frame work comes from "debug.frame", averaged over 60 frames, next to the work of frames without calls.', color = 'textMuted', font = 'caption'},
        },
        code = "local started = datetime.now():millis()\nfor i = 1, 200 do demo.echo(i):await() end\nprint((datetime.now():millis() - started) / 200)",
    }
    self:run()
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

function Benchmark:run()
    self:act(function()
        self.results:clear()
        self:roundTrip()
        self:throughput()
        self:callsPerFrame()
        self:eventsPerFrame()
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
            return
        end
    end
    local each = (Benchmark.now() - started) / Benchmark.roundTrips
    self.results:set('roundTrip', 'pass', name, string.format('%.2f ms per call over %d calls awaited one after the other, at %.0f frames per second.', each, Benchmark.roundTrips, debug.frame().fps))
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
    self.results:set('throughput', 'pass', name, string.format('%.0f calls per second, %d calls sent at once in %.0f ms.', Benchmark.parallel / seconds, Benchmark.parallel, seconds * 1000))
end

function Benchmark:callsPerFrame()
    local name = 'Frame work with N calls'
    self.results:set('calls', 'waiting', name, 'Measuring.')
    local base = Benchmark.frameWork(Benchmark.frames)
    local parts = {string.format('no calls %.2f ms', base)}
    for _, count in ipairs(Benchmark.perFrame) do
        local pending = {}
        local work = Benchmark.frameWork(Benchmark.frames, function()
            for index = 1, count do
                pending[#pending + 1] = demo.echo(index)
            end
        end)
        async.all(pending):await()
        parts[#parts + 1] = string.format('%d calls %.2f ms', count, work)
    end
    self.results:set('calls', 'pass', name, 'Mean work of a frame: ' .. table.concat(parts, ', ') .. '.')
end

-- The native part sends the batched events 30 times per second, so the frames that receive a list alternate with frames that do not, and the mean holds both.
function Benchmark:eventsPerFrame()
    local name = 'Frame work with N events'
    self.results:set('events', 'waiting', name, 'Measuring.')
    local base = Benchmark.frameWork(Benchmark.frames)
    local parts = {string.format('no events %.2f ms', base)}
    for _, count in ipairs(Benchmark.events) do
        self.received = 0
        local started, err = demo.burst(count, 60):await()
        if err then
            self.results:failure('events', name, err)
            return
        end
        local work = Benchmark.frameWork(Benchmark.frames)
        parts[#parts + 1] = string.format('%d events 30 times per second %.2f ms with %d received', started.count, work, self.received)
        async.sleep(2000):await()
    end
    self.results:set('events', 'pass', name, 'Mean work of a frame: ' .. table.concat(parts, ', ') .. '.')
end

return Benchmark
