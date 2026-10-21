-- The native part sends 100 events of the name `burst` 30 times per second, each marked batched, and the bridge hands the events of one frame to the listener as one list in order, which suits sensors, locations and progress. Swift, Kotlin and JavaScript send them from the main thread and C from a thread of its library. The chart shows the size of every list as it arrives.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local DemoTest = require('categories.plugins.demo-test')
local Test = require('harness.test')
local demo = require('native-demo')

local Batches = haylen.class('Batches', DemoTest)

Batches.count = 100
Batches.ticks = 30
Batches.name = 'Batched events arrive as one list per frame'
Batches.chart = '@native-batches'
Batches.chartSize = {1000, 360}
Batches.bars = 30

function Batches:enter()
    self.sizes = {}
    self.chartTarget = graphics.newRenderTarget(Batches.chartSize[1], Batches.chartSize[2], {filter = 'linear'})
    self.chartCamera = graphics2d.newCamera()
    self.chartCamera.anchor = 'topLeft'
    ui.setImage(Batches.chart, self.chartTarget)
    self.connections = {
        demo.onBurst(function(list) self:receive(list) end),
        demo.onBurstDone(function(done) self:finish(done) end),
    }
    self:frame{
        hint = 'Send the bursts again to see that they hold.',
        focus = 'send',
        controls = {
            ui.button{id = 'send', text = 'Send the bursts', variant = 'primary', onClick = function() self:send() end},
            ui.label{text = 'Each native part marks the events batched, with "{batched = true}" on the web, "batched" set to "true" in Swift and Kotlin and a flag of the emit in C. The listener runs once per frame with a list instead of once per event.', color = 'textMuted', font = 'caption'},
        },
        showcase = {
            ui.card{grow = 1, align = 'stretch', gap = 12,
                ui.row{gap = 12, ui.label{text = 'Events in each list', font = 'heading', grow = 1}, ui.badge{id = 'lists', text = 'No lists yet', tone = 'neutral'}},
                ui.image{image = Batches.chart, fit = 'fill', grow = 1, radius = 12, align = 'stretch'},
                ui.label{text = 'Each bar is one call of the listener, with the events of one frame. The line marks the 100 events of one tick.', font = 'caption', color = 'textMuted'},
            },
        },
        code = "demo.onBurst(function(list)\n  print(#list, list[1].index)\nend)\ndemo.burst(100, 30)",
    }
    self:send()
end

function Batches:exit()
    for _, connection in ipairs(self.connections) do
        connection:disconnect()
    end
    ui.removeImage(Batches.chart)
    Batches.super.exit(self)
end

function Batches:send()
    self:act(function()
        self.results:clear()
        self.sizes = {}
        self.received = {lists = 0, events = 0, largest = 0, ordered = true}
        local started, err = demo.burst(Batches.count, Batches.ticks):await()
        if err then
            self.received = nil
            self.results:failure('burst', Batches.name, err)
            return
        end
        self.results:set('burst', 'waiting', Batches.name, string.format('%s sends %d batched events 30 times per second for %d ticks.', started.language, started.count, started.ticks))
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
        local position = event.tick * Batches.count + event.index
        received.ordered = received.ordered and position == (received.last or -1) + 1
        received.last = position
    end
    self.sizes[#self.sizes + 1] = #list
    self:set('lists', {text = string.format('%d events in %d lists', received.events, received.lists), tone = 'information'})
    self.results:set('burst', 'waiting', Batches.name, string.format('%d events in %d lists so far, at most %d in one list.', received.events, received.lists, received.largest))
end

-- The event `burstDone` comes after the last event of the last list, so every event arrived by then.
function Batches:finish(done)
    local received = self.received
    if not received then
        return
    end
    local batched = received.events == done.events and received.ordered and received.lists < received.events
    self:set('lists', {text = string.format('%d events in %d lists', received.events, received.lists), tone = batched and 'success' or 'danger'})
    self.results:set('burst', batched and 'pass' or 'fail', Batches.name, string.format('%d of the %d events that %s sent arrived %s in %d lists, where one call per event would have called the listener %d times.', received.events, done.events, done.language, received.ordered and 'in order' or 'out of order', received.lists, done.events))
    self.received = nil
end

-- Paints the newest lists as bars that grow from the floor of the chart, against the line of one tick.
function Batches:render()
    Batches.super.render(self)
    local width, height = Batches.chartSize[1], Batches.chartSize[2]
    graphics2d.beginTarget(self.chartTarget, self.chartCamera, {clear = ui.themeColor('window')})
    local largest = Batches.count
    for _, size in ipairs(self.sizes) do
        largest = math.max(largest, size)
    end
    local floor, top = height - 24, 24
    local scale = (floor - top) / (largest * 1.25)
    local tick = floor - Batches.count * scale
    graphics2d.drawShape({24, tick - 1.5, width - 48, 3}, {radius = 1.5, color = Test.warm})
    local first = math.max(1, #self.sizes - Batches.bars + 1)
    local slot = (width - 48) / Batches.bars
    for index = first, #self.sizes do
        local size = self.sizes[index]
        local barHeight = math.max(slot * 0.6, size * scale)
        local x = 24 + (index - first) * slot + slot * 0.2
        graphics2d.drawShape({x, floor - barHeight, slot * 0.6, barHeight}, {radius = slot * 0.3, color = size > Batches.count and Test.violet or Test.accent})
    end
end

return Batches
