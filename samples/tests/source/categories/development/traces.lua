-- Profiler traces: a trace of a number of frames with the scopes of the frame thread, jobs of the workers and the time of the GPU, written to user storage off the frame thread and read back to count its spans by lane, which bars compare.
local async = require('async')
local debugging = require('haylen.debug')
local haylen = require('haylen')
local json = require('json')
local net = require('haylen.net')
local storage = require('haylen.storage')
local ui = require('haylen.ui')

local Test = require('harness.test')
local LogCard = require('categories.development.log-card')

local Traces = haylen.class('Traces', Test)

Traces.path = 'traces/test-project.json'
Traces.code = [[
debugging.startTrace({frames = 60})
scene.spawn(self, function()
    while debugging.tracing() do async.sleep(50):await() end
    local trace = debugging.stopTraceAsync('traces/test-project.json'):await()  -- Written on the I/O pool.
    print(trace.path, trace.spans, trace.dropped, debugging.frame().gpuMilliseconds)
end)]]
-- The lanes of the trace that the bars compare, by the names of their spans.
Traces.kinds = {
    {name = 'frame', text = 'Frames'},
    {name = 'update', text = 'Updates'},
    {name = 'job', text = 'Worker jobs'},
    {name = 'gpuFrame', text = 'GPU frames'},
}

function Traces:enter()
    self.files = LogCard(self, 'log', 'Trace files', 'Each trace written shows here.')
    local bars = {}
    for _, kind in ipairs(Traces.kinds) do
        bars[#bars + 1] = ui.row{gap = 12, alignItems = 'center',
            ui.label{text = kind.text, width = 170},
            ui.progress{id = 'bar-' .. kind.name, value = 0, grow = 1},
            ui.label{id = 'count-' .. kind.name, text = '0', width = 56, textAlign = 'end'},
        }
    end
    self:frame{
        hint = 'Record a trace of 60 frames while workers decode messages, then open the file in the profiler of a browser.',
        code = Traces.code,
        content = {ui.row{wrap = true, grow = 1, gap = 24, alignItems = 'stretch',
            ui.panel{width = 560, gap = 12,
                ui.row{gap = 12, alignItems = 'center',
                    ui.label{text = 'Spans by lane', font = 'heading', grow = 1},
                    ui.badge{id = 'state', text = 'Waiting', tone = 'neutral'},
                },
                ui.column{gap = 10, align = 'stretch', children = bars},
                ui.label{id = 'gpu', text = 'The GPU time of a frame is not measured yet.', color = 'textMuted'},
                ui.button{id = 'record', text = 'Record 60 frames', align = 'stretch', variant = 'primary', onClick = function() self:record() end},
            },
            self.files:node(),
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

function Traces:record()
    if debugging.tracing() then
        self.files:add('A trace is already recording', 'It writes its file once its 60 frames end.')
        return
    end
    debugging.startTrace({frames = 60})
    self:set('state', {text = 'Recording 60 frames', tone = 'warning'})
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
    local most = 1
    for _, kind in ipairs(Traces.kinds) do
        most = math.max(most, counts[kind.name] or 0)
    end
    for _, kind in ipairs(Traces.kinds) do
        local count = counts[kind.name] or 0
        self:set('bar-' .. kind.name, {value = count / most})
        self:set('count-' .. kind.name, {text = tostring(count)})
    end
    self:set('state', {text = 'Written', tone = 'success'})
    self.files:add(string.format('Wrote "%s"', trace.path), string.format('%d spans, with %d dropped.', trace.spans, trace.dropped))
    local gpu = debugging.frame().gpuMilliseconds
    self:set('gpu', {text = gpu and string.format('The GPU took %.3f ms for a frame.', gpu) or 'This graphics API does not measure the GPU time of a frame.'})
end

return Traces
