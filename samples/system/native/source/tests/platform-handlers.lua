-- Platform handlers: methods that the platform code of the app answers, suspending Kotlin and a throwing Java handler on Android, async Swift on Apple platforms and JavaScript on the web, with typed errors, exceptions that fail the call and cancellation that reaches the handler.
local haylen = require('haylen')
local platform = require('haylen.platform')
local ui = require('haylen.ui')

local CheckList = require('check-list')
local sample = require('sample')

local PlatformHandlers = haylen.class('PlatformHandlers', sample.Test)

local expect = CheckList.expect

function PlatformHandlers:enter()
    self.stopped = {}
    self.connection = platform.on('native_sample.cancelled', function(payload) self.stopped[#self.stopped + 1] = payload.language end)
    self:frame({
        hint = 'Run the checks again to see that they hold.',
        focus = 'run',
        controls = {
            ui.button{id = 'run', text = 'Run the checks again', variant = 'primary', onClick = function() self:run() end},
            ui.label{text = 'The handlers live in platform/android with HaylenCoroutines.register, platform/apple with HaylenBridge.register and platform/web with Module.haylen.register. The desktop player runs no platform code of the app.', color = 'textMuted', font = 'caption'},
            ui.label{font = 'monospace', text = "local call = platform.call('native_sample.slow')\ncall:cancel()"},
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
        local greeting, err = platform.call('native_sample.greet', {name = 'Lua'}):await()
        if err and err.code == 'noHandler' then
            checks:skip('Async handler', 'No platform code of this app runs here: ' .. err.message)
            return
        end

        checks:run('Async handler', function()
            if err then
                error(err, 0)
            end
            return greeting.language .. ' answered ' .. expect(greeting.greeting, 'Hello, Lua', 'the greeting')
        end)
        checks:run('Typed error', function()
            local ok, failure = pcall(sample.call, 'native_sample.refuse')
            expect(ok, false, 'the success of native_sample.refuse')
            expect(failure.data.reason, 'requested', 'the reason')
            return string.format('failed with the code %s: %s', expect(failure.code, 'refused', 'the code'), failure.message)
        end)
        checks:run('Handler that throws', function()
            local ok, failure = pcall(sample.call, 'native_sample.explode')
            expect(ok, false, 'the success of native_sample.explode')
            return string.format('failed with the code %s instead of crashing: %s', expect(failure.code, 'exception', 'the code'), failure.message)
        end)
        checks:run('Cancel reaches the handler', function()
            local before = #self.stopped
            local call = platform.call('native_sample.slow')
            sample.waitFor(function() return false end, 0.2)
            call:cancel()
            if not sample.waitFor(function() return #self.stopped > before end, 2) then
                error('the handler did not stop within two seconds', 0)
            end
            return 'the ' .. self.stopped[#self.stopped] .. ' handler stopped'
        end)
        checks:run('Timeout reaches the handler', function()
            local before = #self.stopped
            local _, failure = platform.call('native_sample.slow', nil, {timeout = 0.3}):await()
            expect(failure and failure.code, 'timeout', 'the code')
            if not sample.waitFor(function() return #self.stopped > before end, 2) then
                error('the handler did not stop within two seconds', 0)
            end
            return 'the ' .. self.stopped[#self.stopped] .. ' handler stopped after the timeout'
        end)
    end)
end

function PlatformHandlers:update(dt)
    PlatformHandlers.super.update(self, dt)
    self:status(self.checks:summary() .. '   platform ' .. haylen.platform)
end

function PlatformHandlers:draw(area)
    self.checks:draw(area)
end

return PlatformHandlers
