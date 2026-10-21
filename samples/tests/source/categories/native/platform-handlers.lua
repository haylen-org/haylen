-- Methods that the plugin `native-sample` answers, suspending Kotlin and a throwing Java handler on Android, async Swift on Apple platforms and JavaScript on the web, with typed errors, exceptions that fail the call and cancellation that reaches the handler.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local Checks = require('categories.native.checks')
local nativeSample = require('native-sample')

local PlatformHandlers = haylen.class('PlatformHandlers', Test)

local expect = Checks.expect

-- How the native part of the plugin `native-sample` registers its handlers on each platform.
PlatformHandlers.registrations = {
    android = 'The Android module of the plugin registers its handlers with "registerSuspend" and a handler that throws.',
    ios = 'The Apple class of the plugin registers its async Swift handlers with "register" of its context.',
    tvos = 'The Apple class of the plugin registers its async Swift handlers with "register" of its context.',
    macos = 'The Apple class of the plugin registers its async Swift handlers with "register" of its context.',
    web = 'The web module of the plugin registers its handlers with "context.register".',
    desktop = 'The desktop player runs no native part of the plugin, so its handlers answer only in the apps of the other platforms.',
}

function PlatformHandlers:enter()
    self.checks = Checks(self.entry.code)
    self.stopped = {}
    self.connection = nativeSample.onCancelled(function(payload) self.stopped[#self.stopped + 1] = payload.language end)
    self:frame{
        hint = 'Run the checks again to see that they hold.',
        focus = 'run',
        content = {self.checks:view()},
        controls = {
            ui.button{id = 'run', text = 'Run the checks again', variant = 'primary', onClick = function() self:run() end},
            ui.label{text = PlatformHandlers.registrations[haylen.platform] or PlatformHandlers.registrations.desktop, color = 'textMuted', font = 'caption'},
        },
        code = "local greeting = nativeSample.greet('Lua'):await()\nlocal _, failure = nativeSample.refuse():await()\nprint(failure.code, failure.data.reason)\nlocal call = nativeSample.slow()\ncall:cancel()  -- The handler hears it and stops.",
    }
    self.checks:show(self.gui)
    self:run()
end

function PlatformHandlers:exit()
    self.connection:disconnect()
    PlatformHandlers.super.exit(self)
end

function PlatformHandlers:run()
    self.checks:clear()
    self:spawn(function()
        local checks = self.checks
        local greeting, err = nativeSample.greet('Lua'):await()
        if err and err.code == 'noHandler' then
            checks:skip('An async handler', 'No native part of the plugin runs here: ' .. err.message)
            return
        end

        checks:run('An async handler', function()
            if err then
                error(err, 0)
            end
            return 'The ' .. greeting.language .. ' handler answered "' .. expect(greeting.greeting, 'Hello, Lua', 'the greeting') .. '".'
        end)
        checks:run('A typed error', function()
            local ok, failure = pcall(Checks.await, nativeSample.refuse())
            expect(ok, false, 'the success of "native-sample.refuse"')
            expect(failure.data.reason, 'requested', 'the reason')
            return string.format('The call failed with the code "%s" and the reason "%s".', expect(failure.code, 'refused', 'the code'), failure.data.reason)
        end)
        checks:run('A handler that throws', function()
            local ok, failure = pcall(Checks.await, nativeSample.explode())
            expect(ok, false, 'the success of "native-sample.explode"')
            return string.format('The call failed with the code "%s" instead of crashing: %s', expect(failure.code, 'exception', 'the code'), failure.message)
        end)
        checks:run('A cancel reaches the handler', function()
            local before = #self.stopped
            local call = nativeSample.slow()
            Checks.waitFor(function() return false end, 0.2)
            call:cancel()
            if not Checks.waitFor(function() return #self.stopped > before end, 2) then
                error('The handler did not stop within two seconds.', 0)
            end
            return 'The ' .. self.stopped[#self.stopped] .. ' handler stopped.'
        end)
        checks:run('A timeout reaches the handler', function()
            local before = #self.stopped
            local _, failure = nativeSample.slow({timeout = 0.3}):await()
            expect(failure and failure.code, 'timeout', 'the code')
            if not Checks.waitFor(function() return #self.stopped > before end, 2) then
                error('The handler did not stop within two seconds.', 0)
            end
            return 'The ' .. self.stopped[#self.stopped] .. ' handler stopped after the timeout.'
        end)
    end)
end

function PlatformHandlers:update(dt)
    PlatformHandlers.super.update(self, dt)
    self:status(self.checks:summary())
end

return PlatformHandlers
