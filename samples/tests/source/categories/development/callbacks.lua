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

Callbacks.file = 'source/categories/development/edit/ticks.lua'
Callbacks.steps = {'Change "first" to "second" in the three functions.', 'Save the file.', 'The module function and the call through the table show "second" within half a second, and the closure keeps "first" until "Make the closure again" makes a new one.'}
Callbacks.interval = 0.5
Callbacks.kinds = {
    {id = 'moduleFunction', title = 'Module function', note = 'Handed to a timer as it is, and replaced wherever the engine holds it.'},
    {id = 'throughTable', title = 'Call through the table', note = 'Finds the new function on every call.'},
    {id = 'closure', title = 'Closure made at run time', note = 'Keeps the body it had until the button makes it again.'},
}

function Callbacks:init(entry)
    Callbacks.super.init(self, entry)
    self.moduleFunction, self.throughTable, self.closure = 'waiting', 'waiting', 'waiting'
    self.firstControl = 'remake'
    self.shown = {}
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

-- The three ways of calling, each in a card with the version of the code it ran last and what it does after a save.
function Callbacks:content()
    local cards = {}
    for index, kind in ipairs(Callbacks.kinds) do
        cards[index] = ui.card{grow = 1, gap = 20, minWidth = 320, align = 'stretch', justify = 'center', alignItems = 'center',
            ui.label{text = kind.title, font = 'heading'},
            ui.label{id = kind.id, text = 'Waiting', font = 'title', color = 'textMuted'},
            ui.label{text = kind.note, color = 'textMuted', textAlign = 'center'},
        }
    end
    return {ui.row{wrap = true, grow = 1, gap = 24, alignItems = 'stretch', children = cards}}
end

-- Shows the version each way ran last, once it changes.
function Callbacks:update(dt)
    Callbacks.super.update(self, dt)
    for _, kind in ipairs(Callbacks.kinds) do
        local value = self[kind.id]
        if value ~= self.shown[kind.id] and value ~= 'waiting' then
            self.shown[kind.id] = value
            self:set(kind.id, {text = string.format('Ran "%s"', value), color = value == 'first' and 'warningText' or 'successText'})
        end
    end
end

return Callbacks
