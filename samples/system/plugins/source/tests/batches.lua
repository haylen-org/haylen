-- Batched events: the native part sends 100 events of the name burst 30 times per second, each marked batched, and the bridge hands the events of one frame to the listener as one list in order, which suits sensors, locations and progress. Swift, Kotlin and JavaScript send them from the main thread and C from a thread of its library.
local haylen = require('haylen')
local ui = require('haylen.ui')

local demo = require('native-demo')
local sample = require('sample')

local Batches = haylen.class('Batches', sample.Test)

local kCount = 100
local kTicks = 30
local kName = 'batched events arrive as one list per frame'

function Batches:enter()
    self.connections = {
        demo.onBurst(function(list) self:receive(list) end),
        demo.onBurstDone(function(done) self:finish(done) end),
    }
    self:frame({
        hint = 'Send the bursts again to see that they hold.',
        focus = 'send',
        controls = {
            ui.button{id = 'send', text = 'Send the bursts', variant = 'primary', onClick = function() self:send() end},
            ui.label{text = 'Each native part marks the events batched: context.emit with {batched = true} on the web, with batched set to true in Swift and Kotlin, and HAYLEN_NATIVE_EMIT_BATCHED in C. The listener runs once per frame with a list instead of once per event.', color = 'textMuted', font = 'caption'},
            ui.label{font = 'monospace', text = "demo.onBurst(function(list)\n  print(#list, list[1].index)\nend)\ndemo.burst(100, 30)"},
        },
    })
    self:send()
end

function Batches:exit()
    for _, connection in ipairs(self.connections) do
        connection:disconnect()
    end
end

function Batches:send()
    self:act(function()
        self.results:clear()
        self.received = {lists = 0, events = 0, largest = 0, ordered = true}
        local started, err = demo.burst(kCount, kTicks):await()
        if err then
            self.received = nil
            self.results:failure('burst', kName, err)
            return
        end
        self.results:set('burst', 'waiting', kName, string.format('%s sends %d batched events 30 times per second for %d ticks.', started.language, started.count, started.ticks))
    end)
end

-- The events of every list follow each other, tick after tick and index after index.
function Batches:receive(list)
    local received = self.received
    if not received then
        return
    end
    received.lists = received.lists + 1
    received.events = received.events + #list
    received.largest = math.max(received.largest, #list)
    for _, event in ipairs(list) do
        local position = event.tick * kCount + event.index
        received.ordered = received.ordered and position == (received.last or -1) + 1
        received.last = position
    end
    self.results:set('burst', 'waiting', kName, string.format('%d events in %d lists so far, at most %d in one list.', received.events, received.lists, received.largest))
end

-- burstDone comes after the last event of the last list, so every event arrived by then.
function Batches:finish(done)
    local received = self.received
    if not received then
        return
    end
    local batched = received.events == done.events and received.ordered and received.lists < received.events
    self.results:set('burst', batched and 'pass' or 'fail', kName, string.format('%d of the %d events that %s sent arrived %s in %d lists, at most %d in one list, while %d events a tick would call a listener %d times.', received.events, done.events, done.language, received.ordered and 'in order' or 'out of order', received.lists, received.largest, kCount, done.events))
    self.received = nil
end

return Batches
