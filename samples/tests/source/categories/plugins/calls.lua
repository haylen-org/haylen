-- The call `echo` answers on the main thread, `compute` counts primes on a background thread, `fail` answers with a typed failure, and `wait` never answers by itself, so its timeout and a cancel end it and the native part reports that it heard them.
local haylen = require('haylen')
local ui = require('haylen.ui')

local DemoTest = require('categories.plugins.demo-test')
local demo = require('native-demo')

local Calls = haylen.class('Calls', DemoTest)

Calls.limit = 200000
Calls.primes = 17984
Calls.timeout = 0.5

function Calls:enter()
    self.cancelled = {}
    self.token = 0
    self.connection = demo.onWaitCancelled(function(payload) self.cancelled[payload.token] = payload.language end)
    self:frame{
        hint = 'Run the calls again to see that they hold.',
        focus = 'run',
        controls = {
            ui.button{id = 'run', text = 'Run the calls again', variant = 'primary', onClick = function() self:run() end},
            ui.label{text = 'Every call crosses the bridge to the native part of the plugin and back, and a coroutine awaits its answer. Swift answers on Apple platforms, Kotlin on Android, JavaScript on the web and C on the desktops, where the handlers of native libraries run on the frame thread.', color = 'textMuted', font = 'caption'},
        },
        code = "local demo = require('native-demo')\nlocal answer, err = demo.compute(200000):await()\nlocal call = demo.wait(1, {timeout = 0.5})\nlocal _, failure = call:await()\nprint(failure.code)",
    }
    self:run()
end

function Calls:exit()
    self.connection:disconnect()
    Calls.super.exit(self)
end

function Calls:run()
    self:act(function()
        self.results:clear()
        self:echo()
        self:compute()
        self:fail()
        self:waitWithTimeout()
        self:waitAndCancel()
    end)
end

function Calls:echo()
    local name = 'The call "echo" answers on the main thread'
    self.results:set('echo', 'waiting', name, 'Waiting for the answer.')
    local echoed, err = demo.echo({text = 'hello', number = 42, list = {1, 2, 3}}):await()
    if err then
        self.results:failure('echo', name, err)
        return
    end
    local value = echoed.echo
    local same = value.text == 'hello' and value.number == 42 and #value.list == 3
    local thread = echoed.thread == 'main' or (echoed.language == 'C' and echoed.thread == 'frame')
    self.results:set('echo', (same and thread) and 'pass' or 'fail', name, string.format('%s answered %s on the thread "%s".', echoed.language, DemoTest.json(value), echoed.thread))
end

function Calls:compute()
    local name = 'The call "compute" runs in the background'
    self.results:set('compute', 'waiting', name, 'Counting the primes below ' .. Calls.limit .. '.')
    local started = haylen.frameIndex()
    local computed, err = demo.compute(Calls.limit):await()
    if err then
        self.results:failure('compute', name, err)
        return
    end
    -- The web runtime has one thread, so its handler counts in slices that yield to the page instead.
    local thread = computed.thread == 'background' or computed.language == 'JavaScript'
    local detail = string.format('%s counted %d primes below %d on the thread "%s", %s, in %d frames while the app kept drawing.', computed.language, computed.primes, Calls.limit, computed.thread, computed.detail, haylen.frameIndex() - started)
    self.results:set('compute', (computed.primes == Calls.primes and thread) and 'pass' or 'fail', name, detail)
end

function Calls:fail()
    local name = 'The call "fail" answers with a typed failure'
    self.results:set('fail', 'waiting', name, 'Waiting for the failure.')
    local answer, failure = demo.fail():await()
    if answer ~= nil or failure == nil then
        self.results:set('fail', 'fail', name, 'The call "fail" answered ' .. DemoTest.json(answer) .. ' instead of failing.')
        return
    end
    local typed = failure.code == 'demoFailure' and failure.data and failure.data.reason == 'requested'
    self.results:set('fail', typed and 'pass' or 'fail', name, string.format('It failed with the code "%s" and the data %s: %s', tostring(failure.code), DemoTest.json(failure.data), failure.message))
end

function Calls:waitWithTimeout()
    local name = 'The call "wait" ends with its timeout'
    self.token = self.token + 1
    local token = self.token
    self.results:set('timeout', 'waiting', name, string.format('Waiting %.1f seconds.', Calls.timeout))
    local _, failure = demo.wait(token, {timeout = Calls.timeout}):await()
    if not failure or failure.code ~= 'timeout' then
        self.results:set('timeout', 'fail', name, 'The call "wait" ended with "' .. tostring(failure and failure.code) .. '" instead of "timeout".')
        return
    end
    self:reportHeard('timeout', name, token, string.format('The call failed with the code "timeout" after %.1f seconds', Calls.timeout))
end

function Calls:waitAndCancel()
    local name = 'The call "wait" ends with a cancel'
    self.token = self.token + 1
    local token = self.token
    self.results:set('cancel', 'waiting', name, 'Cancelling the call in 0.3 seconds.')
    local call = demo.wait(token)
    DemoTest.waitFor(function() return false end, 0.3)
    call:cancel()
    local _, failure = call:await()
    if not failure or failure.code ~= 'cancelled' then
        self.results:set('cancel', 'fail', name, 'The call "wait" ended with "' .. tostring(failure and failure.code) .. '" instead of "cancelled".')
        return
    end
    self:reportHeard('cancel', name, token, 'The method "call:cancel()" failed the call with the code "cancelled"')
end

-- The native part hears that the app gave the call up and sends `waitCancelled` with the token of the call.
function Calls:reportHeard(key, name, token, what)
    if not DemoTest.waitFor(function() return self.cancelled[token] ~= nil end, 2) then
        self.results:set(key, 'fail', name, what .. ', but the native part sent no "waitCancelled" within two seconds.')
        return
    end
    self.results:set(key, 'pass', name, string.format('%s, and %s sent "waitCancelled" for the token %d.', what, self.cancelled[token], token))
end

return Calls
