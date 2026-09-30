-- Calls: echo answers on the main thread, compute counts primes on a background thread, fail answers with a typed failure, and wait never answers by itself, so its timeout and a cancel end it and the native part reports that it heard them.
local haylen = require('haylen')
local ui = require('haylen.ui')

local demo = require('native-demo')
local sample = require('sample')

local Calls = haylen.class('Calls', sample.Test)

local kLimit = 200000
local kPrimes = 17984
local kTimeout = 0.5

function Calls:enter()
    self.cancelled = {}
    self.token = 0
    self.connection = demo.onWaitCancelled(function(payload) self.cancelled[payload.token] = payload.language end)
    self:frame({
        hint = 'Run the calls again to see that they hold.',
        focus = 'run',
        controls = {
            ui.button{id = 'run', text = 'Run the calls again', variant = 'primary', onClick = function() self:run() end},
            ui.label{text = 'Every call crosses the bridge to the native part of the plugin and back, and a coroutine awaits its answer. Swift answers on Apple platforms, Kotlin on Android, JavaScript on the web and C on the desktops, where the handlers of native libraries run on the frame thread.', color = 'textMuted', font = 'caption'},
            ui.label{font = 'monospace', text = "local demo = require('native-demo')\nlocal answer, err = demo.compute(200000):await()\nlocal call = demo.wait(1, {timeout = 0.5})\nlocal _, failure = call:await()\nprint(failure.code) -- timeout"},
        },
    })
    self:run()
end

function Calls:exit()
    self.connection:disconnect()
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
    local name = 'echo answers on the main thread'
    self.results:set('echo', 'waiting', name, 'waiting for the answer')
    local echoed, err = demo.echo({text = 'hello', number = 42, list = {1, 2, 3}}):await()
    if err then
        self.results:failure('echo', name, err)
        return
    end
    local value = echoed.echo
    local same = value.text == 'hello' and value.number == 42 and #value.list == 3
    local thread = echoed.thread == 'main' or (echoed.language == 'C' and echoed.thread == 'frame')
    self.results:set('echo', (same and thread) and 'pass' or 'fail', name, string.format('%s answered %s on the %s thread.', echoed.language, sample.json(value), echoed.thread))
end

function Calls:compute()
    local name = 'compute runs in the background'
    self.results:set('compute', 'waiting', name, 'counting the primes below ' .. kLimit)
    local started = haylen.frameIndex()
    local computed, err = demo.compute(kLimit):await()
    if err then
        self.results:failure('compute', name, err)
        return
    end
    -- The web runtime has one thread, so its handler counts in slices that yield to the page instead.
    local thread = computed.thread == 'background' or computed.language == 'JavaScript'
    local detail = string.format('%s counted %d primes below %d on the %s thread, %s, in %d frames while the app kept drawing.', computed.language, computed.primes, kLimit, computed.thread, computed.detail, haylen.frameIndex() - started)
    self.results:set('compute', (computed.primes == kPrimes and thread) and 'pass' or 'fail', name, detail)
end

function Calls:fail()
    local name = 'fail answers with a typed failure'
    self.results:set('fail', 'waiting', name, 'waiting for the failure')
    local answer, failure = demo.fail():await()
    if answer ~= nil or failure == nil then
        self.results:set('fail', 'fail', name, 'fail answered ' .. sample.json(answer) .. ' instead of failing.')
        return
    end
    local typed = failure.code == 'demoFailure' and failure.data and failure.data.reason == 'requested'
    self.results:set('fail', typed and 'pass' or 'fail', name, string.format('Failed with the code %s and the data %s: %s', tostring(failure.code), sample.json(failure.data), failure.message))
end

function Calls:waitWithTimeout()
    local name = 'wait ends with its timeout'
    self.token = self.token + 1
    local token = self.token
    self.results:set('timeout', 'waiting', name, string.format('waiting %.1f seconds', kTimeout))
    local _, failure = demo.wait(token, {timeout = kTimeout}):await()
    if not failure or failure.code ~= 'timeout' then
        self.results:set('timeout', 'fail', name, 'wait ended with ' .. tostring(failure and failure.code) .. ' instead of timeout.')
        return
    end
    self:reportHeard('timeout', name, token, string.format('The call failed with the code timeout after %.1f seconds', kTimeout))
end

function Calls:waitAndCancel()
    local name = 'wait ends with a cancel'
    self.token = self.token + 1
    local token = self.token
    self.results:set('cancel', 'waiting', name, 'cancelling the call in 0.3 seconds')
    local call = demo.wait(token)
    sample.waitFor(function() return false end, 0.3)
    call:cancel()
    local _, failure = call:await()
    if not failure or failure.code ~= 'cancelled' then
        self.results:set('cancel', 'fail', name, 'wait ended with ' .. tostring(failure and failure.code) .. ' instead of cancelled.')
        return
    end
    self:reportHeard('cancel', name, token, 'call:cancel() failed the call with the code cancelled')
end

-- The native part hears that the app gave the call up and sends waitCancelled with the token of the call.
function Calls:reportHeard(key, name, token, what)
    if not sample.waitFor(function() return self.cancelled[token] ~= nil end, 2) then
        self.results:set(key, 'fail', name, what .. ', but the native part sent no waitCancelled within two seconds.')
        return
    end
    self.results:set(key, 'pass', name, string.format('%s, and %s sent waitCancelled for the token %d.', what, self.cancelled[token], token))
end

return Calls
