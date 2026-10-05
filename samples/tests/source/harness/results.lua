-- Rows of checks that a test draws on its stage: a state, a name and a detail. The states are waiting, pass, fail, info and skip, which marks what does not apply to the platform, such as an unsupported call. Every row that settles in a new state goes to the log with the code of the test.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local log = require('haylen.log')

local Test = require('harness.test')

local Results = haylen.class('Results')

Results.colors = {waiting = Test.warm, pass = Test.green, fail = Test.red, info = Test.accent, skip = Test.muted}
Results.marks = {waiting = 'Waiting', pass = 'Pass', fail = 'Fail', info = 'Info', skip = 'N/A'}

function Results:init(code)
    self.code = code
    self.rows = {}
    self.byKey = {}
end

-- Adds the row of `key`, or changes it.
function Results:set(key, state, name, detail)
    local row = self.byKey[key]
    if not row then
        row = {}
        self.byKey[key] = row
        self.rows[#self.rows + 1] = row
    end
    local settled = state ~= 'waiting' and row.state ~= state
    row.state, row.name, row.detail = state, name, tostring(detail or '')
    if settled then
        local write = state == 'fail' and log.warning or log.info
        write(string.format('[%s] %s: %s. %s', self.code, Results.marks[state], name, row.detail))
    end
end

function Results:clear()
    self.rows = {}
    self.byKey = {}
end

-- Sets the row from a failed call: an unsupported call does not apply to the platform, and any other failure fails the row.
function Results:failure(key, name, err)
    if err.code == 'unsupported' then
        self:set(key, 'skip', name, 'Unsupported on this platform: ' .. err.message)
    else
        self:set(key, 'fail', name, string.format('%s (code "%s")', err.message, tostring(err.code)))
    end
end

function Results:count(state)
    local count = 0
    for _, row in ipairs(self.rows) do
        if row.state == state then
            count = count + 1
        end
    end
    return count
end

function Results:summary()
    return string.format('Passed %d, failed %d, not applicable %d, waiting %d', self:count('pass'), self:count('fail'), self:count('skip'), self:count('waiting'))
end

function Results:draw(area, top)
    local width = area.width - 170
    local y = top or 24
    for _, row in ipairs(self.rows) do
        Test.caption(Results.marks[row.state], 24, y, {size = 24, color = Results.colors[row.state]})
        Test.caption(row.name, 140, y, {size = 24, color = Test.ink})
        Test.caption(row.detail, 140, y + 32, {size = 19, maxWidth = width})
        local _, height = graphics2d.measureText(nil, row.detail, {size = 19, maxWidth = width})
        y = y + 32 + height + 24
    end
    return y
end

return Results
