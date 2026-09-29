-- Library handlers: the test library receives HaylenNativeApi from its init function, registers bridge handlers with it and sends events from a thread of its own, and the calls it answers fail with typed errors, time out and get cancelled. On the web the page answers the same methods.
local haylen = require('haylen')
local native = require('haylen.native')
local platform = require('haylen.platform')
local ui = require('haylen.ui')

local CheckList = require('check-list')
local sample = require('sample')

local LibraryHandlers = haylen.class('LibraryHandlers', sample.Test)

local expect = CheckList.expect

function LibraryHandlers:enter()
    self.cancelled = {}
    self.connections = {
        platform.on('native_test.ready', function(payload) self.ready = payload end),
        platform.on('native_test.cancelled', function(payload) self.cancelled[payload.call] = true end),
    }
    self:frame({
        hint = 'Run the checks again to see that they hold.',
        focus = 'run',
        controls = {
            ui.button{id = 'run', text = 'Run the checks again', variant = 'primary', onClick = function() self:run() end},
            ui.label{text = 'native.load calls native_test_haylen_init with the interface of the engine. The library registers native_test.echo, native_test.fail and native_test.wait with it, answers from its own threads, sends native_test.ready and hears when the app gives up a call.', color = 'textMuted', font = 'caption'},
            ui.label{font = 'monospace', text = "native.load('native_test', {init = 'native_test_haylen_init'})\nlocal call = platform.call('native_test.wait', nil, {timeout = 0.2})\nlocal _, err = call:await()\nprint(err.code)"},
        },
    })
    self:run()
end

function LibraryHandlers:exit()
    for _, connection in ipairs(self.connections) do
        connection:disconnect()
    end
end

function LibraryHandlers:run()
    self.ready = nil
    self.checks = CheckList(self.info.id)
    self:spawn(function()
        local checks = self.checks
        if native.available() then
            checks:run('Init function', function()
                native.load('native_test', {init = 'native_test_haylen_init'})
                return 'native_test_haylen_init registered the handlers of the library'
            end)
        else
            checks:run('Init function', function()
                sample.call('native_test.init')
                return 'the page stands in for the library'
            end)
        end

        checks:run('Event from a library thread', function()
            if not sample.waitFor(function() return self.ready ~= nil end, 2) then
                error('native_test.ready did not arrive within two seconds', 0)
            end
            return 'native_test.ready ' .. sample.json(self.ready)
        end)
        checks:run('Answer from a library thread', function()
            local echo = sample.call('native_test.echo', {word = 'hello'})
            expect(echo.thread, true, 'the thread flag')
            return 'native_test.echo answered ' .. expect(echo.echo.word, 'hello', 'the echo')
        end)
        checks:run('Typed error', function()
            local ok, err = pcall(sample.call, 'native_test.fail')
            expect(ok, false, 'the success of native_test.fail')
            expect(err.data.reason, 'requested', 'the reason')
            return string.format('failed with the code %s: %s', expect(err.code, 'native_test_failure', 'the code'), err.message)
        end)
        checks:run('Timeout', function()
            local call = platform.call('native_test.wait', nil, {timeout = 0.2})
            local _, err = call:await()
            expect(err and err.code, 'timeout', 'the code')
            if not sample.waitFor(function() return self.cancelled[call.id] end, 2) then
                error('the library heard nothing about the timeout', 0)
            end
            return 'the call timed out after 0.2 seconds and the library heard of it'
        end)
        checks:run('Cancel', function()
            local call = platform.call('native_test.wait')
            sample.waitFor(function() return false end, 0.2)
            expect(call:cancel(), true, 'the first cancel')
            local _, err = call:await()
            expect(err and err.code, 'cancelled', 'the code')
            if not sample.waitFor(function() return self.cancelled[call.id] end, 2) then
                error('the library heard nothing about the cancel', 0)
            end
            return 'call:cancel() failed the call with ' .. tostring(err) .. ' and the library heard of it'
        end)
    end)
end

function LibraryHandlers:update(dt)
    LibraryHandlers.super.update(self, dt)
    self:status(self.checks:summary() .. '   pending calls ' .. platform.pendingCalls())
end

function LibraryHandlers:draw(area)
    self.checks:draw(area)
end

return LibraryHandlers
