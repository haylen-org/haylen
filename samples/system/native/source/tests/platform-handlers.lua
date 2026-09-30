-- Platform handlers: methods that the local plugin native-sample answers, suspending Kotlin and a throwing Java handler on Android, async Swift on Apple platforms and JavaScript on the web, with typed errors, exceptions that fail the call and cancellation that reaches the handler.
local haylen = require('haylen')
local ui = require('haylen.ui')

local CheckList = require('check-list')
local nativeSample = require('native-sample')
local sample = require('sample')

local PlatformHandlers = haylen.class('PlatformHandlers', sample.Test)

local expect = CheckList.expect

function PlatformHandlers:enter()
    self.stopped = {}
    self.connection = nativeSample.onCancelled(function(payload) self.stopped[#self.stopped + 1] = payload.language end)
    self:frame({
        hint = 'Run the checks again to see that they hold.',
        focus = 'run',
        controls = {
            ui.button{id = 'run', text = 'Run the checks again', variant = 'primary', onClick = function() self:run() end},
            ui.label{text = 'The handlers live in the local plugin "native-sample": its Android module registers them with "registerSuspend", its Apple class with "register" of its context and its web module with "context.register". The desktop player runs no native part of the plugin.', color = 'textMuted', font = 'caption'},
            ui.label{font = 'monospace', text = "local call = nativeSample.slow()\ncall:cancel()"},
        },
    })
    self:run()
end

function PlatformHandlers:exit()
    self.connection:disconnect()
end

function PlatformHandlers:run()
    self.checks = CheckList(self.info.id)
    self:spawn(function()
        local checks = self.checks
        local greeting, err = nativeSample.greet('Lua'):await()
        if err and err.code == 'noHandler' then
            checks:skip('Async handler', 'No native part of the plugin runs here: ' .. err.message)
            return
        end

        checks:run('Async handler', function()
            if err then
                error(err, 0)
            end
            return 'The ' .. greeting.language .. ' handler answered "' .. expect(greeting.greeting, 'Hello, Lua', 'the greeting') .. '"'
        end)
        checks:run('Typed error', function()
            local ok, failure = pcall(sample.await, nativeSample.refuse())
            expect(ok, false, 'the success of "native-sample.refuse"')
            expect(failure.data.reason, 'requested', 'the reason')
            return string.format('Failed with the code "%s": %s', expect(failure.code, 'refused', 'the code'), failure.message)
        end)
        checks:run('Handler that throws', function()
            local ok, failure = pcall(sample.await, nativeSample.explode())
            expect(ok, false, 'the success of "native-sample.explode"')
            return string.format('Failed with the code "%s" instead of crashing: %s', expect(failure.code, 'exception', 'the code'), failure.message)
        end)
        checks:run('Cancel reaches the handler', function()
            local before = #self.stopped
            local call = nativeSample.slow()
            sample.waitFor(function() return false end, 0.2)
            call:cancel()
            if not sample.waitFor(function() return #self.stopped > before end, 2) then
                error('The handler did not stop within two seconds.', 0)
            end
            return 'The ' .. self.stopped[#self.stopped] .. ' handler stopped'
        end)
        checks:run('Timeout reaches the handler', function()
            local before = #self.stopped
            local _, failure = nativeSample.slow({timeout = 0.3}):await()
            expect(failure and failure.code, 'timeout', 'the code')
            if not sample.waitFor(function() return #self.stopped > before end, 2) then
                error('The handler did not stop within two seconds.', 0)
            end
            return 'The ' .. self.stopped[#self.stopped] .. ' handler stopped after the timeout'
        end)
    end)
end

function PlatformHandlers:update(dt)
    PlatformHandlers.super.update(self, dt)
    self:status(self.checks:summary() .. '   platform "' .. haylen.platform .. '"')
end

function PlatformHandlers:draw(area)
    self.checks:draw(area)
end

return PlatformHandlers
