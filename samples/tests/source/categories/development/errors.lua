-- Errors of every kind of code that runs for the app reach the error screen with the message, the file, the line and the stack, and "Back to the app" returns to the test list with the app running on. Each button raises one kind, and the rows keep what the error screen showed, so they fill in each time the test opens again after a return.
local assets = require('haylen.assets')
local async = require('async')
local events = require('haylen.events')
local haylen = require('haylen')
local net = require('haylen.net')
local platform = require('haylen.platform')
local scene = require('haylen.scene')
local signal = require('haylen.signal')
local timer = require('haylen.timer')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Test = require('harness.test')

local Errors = haylen.class('Errors', Test)

Errors.file = 'source/categories/development/errors.lua'
Errors.rowHeight = 36
Errors.messageLength = 90
-- What the error screen showed for each kind, kept for the whole run of the project, because going back to the app reopens the test list.
Errors.reports = {}
Errors.kinds = {
    {id = 'update', text = 'Scene update', message = 'The update of the test failed.'},
    {id = 'enter', text = 'Scene enter', message = 'The enter hook of a scene failed.'},
    {id = 'task', text = 'Task of the scene', message = 'A task of the scene failed after it waited.'},
    {id = 'async', text = 'Task of "async.spawn"', message = 'A task of "async.spawn" failed after it waited.'},
    {id = 'timer', text = 'Timer', message = 'A timer failed.'},
    {id = 'tween', text = 'Tween', message = 'The callback of a tween failed.'},
    {id = 'signal', text = 'Signal', message = 'A handler of a signal failed.'},
    {id = 'event', text = 'Queued event', message = 'A listener of a queued event failed.'},
    {id = 'native', text = 'Native event', message = 'A listener of a native event failed.'},
    {id = 'answer', text = 'Platform answer', message = 'The task that awaited a platform answer failed with 7.'},
    {id = 'asset', text = 'Asset load', message = 'development/missing.png'},
    {id = 'socket', text = 'Socket listener', message = 'A listener of a socket failed.'},
    {id = 'button', text = 'Button handler', message = 'The handler of a button failed.'},
}

-- The code that raises each kind, which runs when its button is pressed.
Errors.raise = {}

function Errors.raise.update(self)
    self.failing = true
end

function Errors.raise.enter()
    scene.push({enter = function()
        error('The enter hook of a scene failed.')
    end})
end

function Errors.raise.task(self)
    self:spawn(function()
        async.sleep(10):await()
        error('A task of the scene failed after it waited.')
    end)
end

function Errors.raise.async()
    async.spawn(function()
        async.sleep(10):await()
        error('A task of "async.spawn" failed after it waited.')
    end)
end

function Errors.raise.timer(self)
    timer.after(0.05, function()
        error('A timer failed.')
    end, {owner = self})
end

-- A tween holds its target weakly, so the test keeps the target.
function Errors.raise.tween(self)
    self.target = {x = 0}
    tween.to(self.target, 0.05, {x = 1}, {owner = self, onComplete = function()
        error('The callback of a tween failed.')
    end})
end

function Errors.raise.signal(self)
    local changed = signal.new('errors.changed')
    changed:connect(function()
        error('A handler of a signal failed.')
    end, {owner = self})
    timer.after(0.05, function() changed:emit() end, {owner = self})
end

function Errors.raise.event(self)
    events.on('errors.happened', function()
        error('A listener of a queued event failed.')
    end, {owner = self})
    events.post('errors.happened')
end

-- The page, the player and the apps of the templates deliver native events the same way, so an event that Lua sends stands in for one of native code.
function Errors.raise.native(self)
    self.native = platform.on('errors.native', function()
        error('A listener of a native event failed.')
    end)
    platform.emit('errors.native', {})
end

function Errors.raise.answer()
    platform.registerHandler('errors.answer', function()
        return {value = 7}
    end)
    async.spawn(function()
        local answer = platform.call('errors.answer'):await()
        error(string.format('The task that awaited a platform answer failed with %d.', answer.value))
    end)
end

-- A failed load rejects its promise, and the task that awaited it raises the failure again to stop the app.
function Errors.raise.asset()
    async.spawn(function()
        local _, failure = assets.loadAsync('development/missing.png', 'texture'):await()
        error(failure, 0)
    end)
end

-- Nothing listens on port 1, so the socket reports "error" at once.
function Errors.raise.socket(self)
    local opened = net.connectWebSocket('ws://127.0.0.1:1/', {owner = self})
    opened:on('error', function()
        error('A listener of a socket failed.')
    end, {owner = self})
end

function Errors.raise.button()
    error('The handler of a button failed.')
end

function Errors:enter()
    local controls = {ui.sectionTitle{text = 'Raise an error from'}}
    for _, kind in ipairs(Errors.kinds) do
        controls[#controls + 1] = ui.button{id = kind.id, text = kind.text, onClick = function()
            self.pending = kind.id
            Errors.raise[kind.id](self)
        end}
    end
    self:frame{hint = 'Each button raises an error from one kind of code. The error screen shows it with its stack, and "Back to the app" returns to the test list, where this test opens again with the row filled in.', controls = controls, focus = Errors.kinds[1].id}

    events.on('appError', function(error)
        if self.pending then
            Errors.reports[self.pending] = {message = error.message, file = error.file, line = error.line, frames = #error.frames}
            self:log('The error screen showed "%s" at "%s" line %d for "%s".', error.message, error.file, error.line, self.pending)
            self.pending = nil
        end
    end, {owner = self})
end

function Errors:exit()
    if self.native then
        self.native:disconnect()
    end
    Errors.super.exit(self)
end

-- Whether the error screen showed the message of the kind at a line of this file, with a stack.
function Errors.passed(kind, report)
    return report.message:find(kind.message, 1, true) ~= nil and report.file == Errors.file and report.line > 0 and report.frames > 0
end

-- Keeps the first line of a message and cuts it to one row, since a message may hold a long path.
function Errors.shorten(message)
    local first = message:match('^[^\n]*')
    if #first <= Errors.messageLength then
        return first
    end
    return first:sub(1, Errors.messageLength - 3) .. '...'
end

function Errors:update(dt)
    Errors.super.update(self, dt)
    if self.failing then
        error('The update of the test failed.')
    end

    local raised, passed = 0, 0
    for _, kind in ipairs(Errors.kinds) do
        local report = Errors.reports[kind.id]
        if report then
            raised = raised + 1
            passed = passed + (Errors.passed(kind, report) and 1 or 0)
        end
    end
    self:status(string.format('Raised %d of %d kinds, and %d reached the error screen with their message, line and stack.', raised, #Errors.kinds, passed))
end

function Errors:draw(area)
    local y = 24
    for _, kind in ipairs(Errors.kinds) do
        local report = Errors.reports[kind.id]
        local passed = report and Errors.passed(kind, report)
        Test.caption(not report and 'Not raised' or passed and 'Pass' or 'Fail', 24, y, {size = 20, color = not report and Test.muted or passed and Test.green or Test.red})
        Test.caption(kind.text, 160, y, {size = 20, color = Test.ink})
        if report then
            Test.caption(string.format('Line %d, %d frames: %s', report.line, report.frames, Errors.shorten(report.message)), 420, y, {size = 18})
        end
        y = y + Errors.rowHeight
    end
end

return Errors
