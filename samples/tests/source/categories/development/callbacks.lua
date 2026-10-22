-- What runs the new code after a reload: a module function handed to a timer is replaced wherever the engine holds it, a call through the module table always finds the new function, and a closure made at run time keeps the body it had, which a scene rebuilds in its "reloaded" hook when it matters.
local haylen = require('haylen')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local DevelopmentTest = require('categories.development.development-test')
local ticks = require('categories.development.edit.ticks')

local Callbacks = haylen.class('Callbacks', DevelopmentTest)

Callbacks.code = [[
timer.every(0.5, ticks.record, {owner = self})  -- The module function takes the new code.
timer.every(0.5, function()
    self.throughTable = ticks.label()  -- A call through the table takes it too.
end, {owner = self})
self.closureTimer = timer.every(0.5, ticks.makeClosure(self), {owner = self})  -- A closure keeps its code.]]

Callbacks.files = {'source/categories/development/edit/ticks.lua'}
Callbacks.steps = {'Change "first" to "second" in the three functions.', 'Save the file.', 'The module function and the call through the table show "second" within half a second, and the closure keeps "first" until "Make the closure again" makes a new one.'}
Callbacks.interval = 0.5
-- Each card keeps the versions of its last calls, oldest first.
Callbacks.historyLength = 8
Callbacks.kinds = {
    {id = 'moduleFunction', title = 'Module function', note = 'Handed to a timer as it is, and replaced wherever the engine holds it.'},
    {id = 'throughTable', title = 'Call through the table', note = 'Finds the new function on every call.'},
    {id = 'closure', title = 'Closure made at run time', note = 'Keeps the body it had until the button makes it again.'},
}

function Callbacks:init(entry)
    Callbacks.super.init(self, entry)
    self.firstControl = 'remake'
    self.history = {moduleFunction = {}, throughTable = {}, closure = {}}
end

function Callbacks:controls()
    return {ui.button{id = 'remake', text = 'Make the closure again', variant = 'primary', onClick = function()
        self:makeClosure()
    end}}
end

function Callbacks:enter()
    Callbacks.super.enter(self)
    ticks.target = self
    timer.every(Callbacks.interval, ticks.record, {owner = self})
    timer.every(Callbacks.interval, function() self.throughTable = ticks.label() end, {owner = self})
    self:makeClosure()
end

function Callbacks:makeClosure()
    if self.closureTimer then
        timer.cancel(self.closureTimer)
    end
    self.closureTimer = timer.every(Callbacks.interval, ticks.makeClosure(self), {owner = self})
end

-- The three ways of calling, each in a card with what it does after a save, the version of the code it ran last and the versions of its last calls.
function Callbacks:content()
    local cards = {}
    for index, kind in ipairs(Callbacks.kinds) do
        cards[index] = ui.card{grow = 1, gap = 16, minWidth = 320, align = 'stretch',
            ui.label{text = kind.title, font = 'heading'},
            ui.label{text = kind.note, color = 'textMuted'},
            ui.divider{align = 'stretch'},
            ui.spacer{grow = 1},
            ui.label{id = kind.id, text = 'Waiting', font = 'title', color = 'textMuted', textAlign = 'center', align = 'stretch'},
            ui.spacer{grow = 1},
            ui.label{text = 'The last calls, oldest first', font = 'caption', color = 'textMuted'},
            ui.grid{id = kind.id .. '-history', columns = 4, gap = 6},
        }
    end
    return {ui.row{wrap = true, grow = 1, gap = 24, alignItems = 'stretch', children = cards}}
end

-- Takes the version of each call since the last frame, which the timers write into the fields of the kinds, and shows it with the last calls.
function Callbacks:update(dt)
    Callbacks.super.update(self, dt)
    for _, kind in ipairs(Callbacks.kinds) do
        local value = self[kind.id]
        if value then
            self[kind.id] = nil
            self:showCall(kind.id, value)
        end
    end
end

function Callbacks:showCall(id, value)
    local history = self.history[id]
    history[#history + 1] = value
    if #history > Callbacks.historyLength then
        table.remove(history, 1)
    end
    local tone = value == 'first' and 'warning' or 'success'
    self:set(id, {text = string.format('Ran "%s"', value), color = tone .. 'Text'})
    local badges = {}
    for index, version in ipairs(history) do
        badges[index] = ui.badge{text = string.format('"%s"', version), tone = version == 'first' and 'warning' or 'success', align = 'stretch'}
    end
    self.gui:replaceChildren(id .. '-history', badges)
end

return Callbacks
