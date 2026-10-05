-- The test library receives `HaylenNativeApi` from its init function, registers bridge handlers with it and sends events from a thread of its own, and the calls it answers fail with typed errors, time out and get cancelled. On the web the plugin `native-test` answers the same handlers.
local haylen = require('haylen')
local native = require('haylen.native')
local platform = require('haylen.platform')
local ui = require('haylen.ui')

local Test = require('harness.test')
local Checks = require('categories.native.checks')
local nativeTest = require('native-test')

local LibraryHandlers = haylen.class('LibraryHandlers', Test)

local expect = Checks.expect

function LibraryHandlers:enter()
    self.cancelled = {}
    self.connections = {
        platform.on(nativeTest.name('ready'), function(payload) self.ready = payload end),
        platform.on(nativeTest.name('cancelled'), function(payload) self.cancelled[payload.call] = true end),
    }
    self:frame{
        hint = 'Run the checks again to see that they hold.',
        focus = 'run',
        controls = {
            ui.button{id = 'run', text = 'Run the checks again', variant = 'primary', onClick = function() self:run() end},
            ui.label{text = 'The function "native.load" calls "native_test_haylen_init" with the interface of the engine. The library registers "native_test.echo", "native_test.fail" and "native_test.wait" with it, answers from its own threads, sends "native_test.ready" and hears when the app gives up a call.', color = 'textMuted', font = 'caption'},
        },
        code = "native.load('native_test', {init = 'native_test_haylen_init'})\nlocal call = platform.call('native_test.wait', nil, {timeout = 0.2})\nlocal _, err = call:await()\nprint(err.code)",
    }
    self:run()
end

function LibraryHandlers:exit()
    for _, connection in ipairs(self.connections) do
        connection:disconnect()
    end
    LibraryHandlers.super.exit(self)
end

function LibraryHandlers:run()
    self.ready = nil
    self.checks = Checks(self.entry.code)
    self:spawn(function()
        local checks = self.checks
        if native.available() then
            checks:run('The init function', function()
                native.load('native_test', {init = 'native_test_haylen_init'})
                return 'The function "native_test_haylen_init" registered the handlers of the library.'
            end)
        else
            checks:run('The init function', function()
                Checks.await(nativeTest.call('init'))
                return 'The page stands in for the library.'
            end)
        end

        checks:run('An event from a thread of the library', function()
            if not Checks.waitFor(function() return self.ready ~= nil end, 2) then
                error('The event "' .. nativeTest.name('ready') .. '" did not arrive within two seconds.', 0)
            end
            return 'The event "' .. nativeTest.name('ready') .. '" arrived with ' .. Checks.json(self.ready) .. '.'
        end)
        checks:run('An answer from a thread of the library', function()
            local echo = Checks.call(nativeTest.name('echo'), {word = 'hello'})
            expect(echo.thread, true, 'the thread flag')
            return 'The method "' .. nativeTest.name('echo') .. '" answered "' .. expect(echo.echo.word, 'hello', 'the echo') .. '".'
        end)
        checks:run('A typed error', function()
            local ok, err = pcall(Checks.call, nativeTest.name('fail'))
            expect(ok, false, 'the success of "' .. nativeTest.name('fail') .. '"')
            expect(err.data.reason, 'requested', 'the reason')
            return string.format('The call failed with the code "%s": %s', expect(err.code, 'native_test_failure', 'the code'), err.message)
        end)
        checks:run('A timeout', function()
            local call = platform.call(nativeTest.name('wait'), nil, {timeout = 0.2})
            local _, err = call:await()
            expect(err and err.code, 'timeout', 'the code')
            if not Checks.waitFor(function() return self.cancelled[call.id] end, 2) then
                error('The library heard nothing about the timeout.', 0)
            end
            return 'The call timed out after 0.2 seconds, and the library heard of it.'
        end)
        checks:run('A cancel', function()
            local call = platform.call(nativeTest.name('wait'))
            Checks.waitFor(function() return false end, 0.2)
            expect(call:cancel(), true, 'the first cancel')
            local _, err = call:await()
            expect(err and err.code, 'cancelled', 'the code')
            if not Checks.waitFor(function() return self.cancelled[call.id] end, 2) then
                error('The library heard nothing about the cancel.', 0)
            end
            return 'The method "call:cancel()" failed the call with "' .. tostring(err) .. '", and the library heard of it.'
        end)
    end)
end

function LibraryHandlers:update(dt)
    LibraryHandlers.super.update(self, dt)
    self:status(string.format('%s, pending calls %d', self.checks:summary(), platform.pendingCallCount()))
end

function LibraryHandlers:draw(area)
    self.checks:draw(area)
end

return LibraryHandlers
