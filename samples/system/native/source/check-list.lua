-- A list of named checks that pass, fail or do not apply to the platform, each with a detail, drawn one row per check and printed to the log.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local sample = require('sample')

local CheckList = haylen.class('CheckList')

local kColors = {running = sample.warm, pass = sample.green, fail = sample.red, skip = sample.muted}
local kMarks = {running = '...', pass = 'PASS', fail = 'FAIL', skip = 'N/A'}

function CheckList:init(test)
    self.test = test
    self.checks = {}
end

-- Runs `body` inside the task of the caller, so it may wait on promises. The check passes with the text `body` returns and fails with the error it raises.
function CheckList:run(name, body)
    local check = {name = name, state = 'running', detail = ''}
    self.checks[#self.checks + 1] = check
    local ok, result = pcall(body)
    check.state = ok and 'pass' or 'fail'
    check.detail = tostring(result or '')
    print(string.format('[native] %s %s %s: %s', self.test, kMarks[check.state], name, check.detail))
end

function CheckList:skip(name, reason)
    self.checks[#self.checks + 1] = {name = name, state = 'skip', detail = reason}
    print(string.format('[native] %s N/A %s: %s', self.test, name, reason))
end

function CheckList:summary()
    local counts = {running = 0, pass = 0, fail = 0, skip = 0}
    for _, check in ipairs(self.checks) do
        counts[check.state] = counts[check.state] + 1
    end
    return string.format('%d passed   %d failed   %d not applicable   %d running', counts.pass, counts.fail, counts.skip, counts.running)
end

function CheckList:draw(area)
    local width = area.width - 140
    local y = 24
    for _, check in ipairs(self.checks) do
        sample.caption(kMarks[check.state], 24, y, {size = 24, color = kColors[check.state]})
        sample.caption(check.name, 110, y, {size = 24, color = sample.ink})
        sample.caption(check.detail, 110, y + 32, {size = 19, maxWidth = width})
        local _, height = graphics2d.measureText(nil, check.detail, {size = 19, maxWidth = width})
        y = y + 32 + height + 28
    end
end

-- Raises an error that names what was expected when a value differs.
function CheckList.expect(value, expected, what)
    if value ~= expected then
        error(string.format('The value of %s is "%s" instead of "%s".', what, tostring(value), tostring(expected)), 2)
    end
    return value
end

return CheckList
