-- What runs the new code after a reload: a module function handed to a timer is replaced wherever the engine holds it, a call through the module table always finds the new function, and a closure made at run time keeps the body it had, which a scene rebuilds in its "reloaded" hook when it matters.
local haylen = require('haylen')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local DevelopmentTest = require('categories.development.development-test')
local Test = require('harness.test')
local ticks = require('categories.development.edit.ticks')

local Callbacks = haylen.class('Callbacks', DevelopmentTest)

Callbacks.file = 'source/categories/development/edit/ticks.lua'
Callbacks.steps = {'Change "first" to "second" in the three functions.', 'Save the file.', 'The module function and the call through the table show "second" within half a second, and the closure keeps "first" until "Make the closure again" makes a new one.'}
Callbacks.interval = 0.5

function Callbacks:init(entry)
    Callbacks.super.init(self, entry)
    self.moduleFunction, self.throughTable, self.closure = 'waiting', 'waiting', 'waiting'
    self.firstControl = 'remake'
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

function Callbacks:draw(area)
    local rows = {{'Module function handed to a timer', self.moduleFunction}, {'Call through the module table', self.throughTable}, {'Closure made at run time', self.closure}}
    for index, row in ipairs(rows) do
        local y = 60 + (index - 1) * 90
        Test.caption(row[1], 60, y, {size = 28, color = Test.ink})
        Test.caption(row[2], 60, y + 38, {size = 28, color = row[2] == 'first' and Test.warm or Test.green})
    end
end

return Callbacks
