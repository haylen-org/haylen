-- Profiler traces: a trace of a number of frames with the scopes of the frame thread, jobs of the workers and the time of the GPU, written to user storage off the frame thread and read back to count its spans by lane.
local async = require('async')
local debugging = require('haylen.debug')
local haylen = require('haylen')
local json = require('json')
local net = require('haylen.net')
local storage = require('haylen.storage')
local ui = require('haylen.ui')

local Test = require('harness.test')

local Traces = haylen.class('Traces', Test)

Traces.code = [[
debugging.startTrace({frames = 60})
scene.spawn(self, function()
    while debugging.tracing() do async.sleep(50):await() end
    local trace = debugging.stopTraceAsync('traces/level.json'):await()  -- Written on the I/O pool.
    print(trace.path, trace.spans, trace.dropped, debugging.frame().gpuMilliseconds)
end)]]

Traces.path = 'traces/test-project.json'

function Traces:enter()
    self.lines = {}
    self:frame{
        hint = 'Record a trace of 60 frames while workers decode messages, then open the file in the profiler of a browser.',
        code = Traces.code,
        content = {ui.row{grow = 1, gap = 24,
            ui.panel{width = 520, align = 'stretch', gap = 12,
                ui.button{id = 'record', text = 'Record 60 frames', align = 'stretch', variant = 'primary', onClick = function() self:record() end},
                ui.label{id = 'gpu', text = 'GPU time of a frame: not measured yet.'},
            },
            ui.scroll{grow = 1, ui.label{id = 'log', text = '', grow = 1}},
        }},
    }
    self:record()
end

function Traces:exit()
    if debugging.tracing() then
        debugging.stopTraceAsync(Traces.path)
    end
    Traces.super.exit(self)
end

function Traces:note(line)
    table.insert(self.lines, 1, line)
    self.lines[13] = nil
    self:set('log', {text = table.concat(self.lines, '\n')})
end

function Traces:record()
    if debugging.tracing() then
        self:note('A trace is already recording.')
        return
    end
    debugging.startTrace({frames = 60})
    self:note('Recording 60 frames.')
    local message = net.encodeMessagePack({frames = {1, 2, 3, 4, 5, 6, 7, 8}})
    self:spawn(function()
        for _ = 1, 8 do
            net.decodeMessagePackAsync(message):await()
        end
        while debugging.tracing() do
            async.sleep(50):await()
        end
        local trace = debugging.stopTraceAsync(Traces.path):await()
        self:report(trace)
    end)
end

function Traces:report(trace)
    local counts = {}
    for _, event in ipairs(json.decode(storage.readText(trace.path)).traceEvents) do
        if event.ph == 'X' then
            counts[event.name] = (counts[event.name] or 0) + 1
        end
    end
    self:note(string.format('Wrote %d spans to "%s": %d frames, %d updates, %d jobs and %d GPU frames, with %d dropped.', trace.spans, trace.path, counts.frame or 0, counts.update or 0, counts.job or 0, counts.gpuFrame or 0, trace.dropped))
    local gpu = debugging.frame().gpuMilliseconds
    self:set('gpu', {text = gpu and string.format('GPU time of a frame: %.3f ms.', gpu) or 'GPU time of a frame: this graphics API does not measure it.'})
end

return Traces
