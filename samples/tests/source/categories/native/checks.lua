-- Rows of results that run named checks inside a task of the test: a check passes with the text its body returns and fails with the error its body raises. It also waits on calls of the bridge.
local async = require('async')
local haylen = require('haylen')
local json = require('json')
local platform = require('haylen.platform')

local Results = require('harness.results')

local Checks = haylen.class('Checks', Results)

-- Runs `body` inside the task of the caller, so it may wait on promises.
function Checks:run(name, body)
    self:set(name, 'waiting', name, '')
    local ok, result = pcall(body)
    self:set(name, ok and 'pass' or 'fail', name, tostring(result or ''))
end

function Checks:skip(name, reason)
    self:set(name, 'skip', name, reason)
end

-- Raises an error that names what was expected when a value differs.
function Checks.expect(value, expected, what)
    if value ~= expected then
        error(string.format('The value of %s is "%s" instead of "%s".', what, tostring(value), tostring(expected)), 2)
    end
    return value
end

-- Writes a value from the bridge as JSON, the form it crossed the bridge in.
function Checks.json(value)
    if value == nil then
        return 'null'
    end
    return json.encode(value)
end

-- Waits for a platform call inside a task and returns its result, raising its error when it fails.
function Checks.await(call)
    local result, err = call:await()
    if err then
        error(err, 0)
    end
    return result
end

-- Calls a platform method and waits for its result inside a task, raising its error when it fails.
function Checks.call(method, params, options)
    return Checks.await(platform.call(method, params, options))
end

-- Waits inside a task until `condition` holds, checking every 10 milliseconds for at most `seconds`, and returns whether it held.
function Checks.waitFor(condition, seconds)
    for _ = 1, seconds * 100 do
        if condition() then
            return true
        end
        async.sleep(10):await()
    end
    return condition()
end

return Checks
