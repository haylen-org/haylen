-- Deadlines and cancellation with Varn's "async" module: "async.timeout" gives up waiting while the work goes on, "task.cancel()" stops a task for good and closes its to-be-closed variables, and a task of "scene.spawn" ends with its owner.
local async = require('async')
local datetime = require('datetime')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')

local Test = require('harness.test')
local VarnTest = require('categories.varn.varn-test')

local Cancellation = haylen.class('Cancellation', VarnTest)

Cancellation.hint = 'Each bar is the planned work, filled while it runs, green when it ends and red where it stopped, and the white mark is the deadline or the moment of the cancel. Run again repeats the lesson.'
Cancellation.span = 1000

Cancellation.excerpts = {
    {'Deadlines', [[
local value, failure = async.timeout(
    self:job(800, 'done'), 300):await()]]},
    {'Cancel a task', [[
local task = async.spawn(function()
    local guard <close> = setmetatable({}, {
        __close = function() closed = true end,
    })
    async.sleep(400):await()
    finished = true
end)
async.sleep(100):await()
task.cancel()]]},
    {'A task with an owner', [[
local owner = {}
scene.spawn(owner, function()
    async.sleep(300):await()
    resumed = true
end)
owner = nil
collectgarbage()]]},
}

function Cancellation:run()
    self.lanes = {}
    self:deadlineMet()
    self:deadlineMissed()
    self:cancelTask()
    self:owner()
end

-- Returns a promise for `value` that takes `ms` milliseconds, and ends the lane that draws it.
function Cancellation:job(ms, value, lane)
    return async.promise(function()
        async.sleep(ms):await()
        lane.finish = datetime.now():millis()
        return value
    end)
end

-- Adds a bar for work of `length` milliseconds with a mark at `limit`, the deadline or the moment of a cancel.
function Cancellation:lane(label, length, limit)
    local lane = {label = label, length = length, limit = limit, start = datetime.now():millis()}
    self.lanes[#self.lanes + 1] = lane
    return lane
end

function Cancellation:deadlineMet()
    local started = datetime.now():millis()
    local lane = self:lane('A job of 100 ms with a deadline of 500 ms', 100, 500)
    local value = async.timeout(self:job(100, 'in time', lane), 500):await()
    self:check('met', 'A deadline met', value == 'in time', string.format('Resolved with "%s" after %d ms, before the deadline of 500 ms.', tostring(value), datetime.now():millis() - started))
end

-- The deadline rejects the wait, while the promise it gave up on still settles later.
function Cancellation:deadlineMissed()
    self.results:set('missed', 'waiting', 'A deadline missed', 'Waiting 300 ms for a job of 800 ms.')
    local started = datetime.now():millis()
    local slow = self:job(800, 'done', self:lane('A job of 800 ms with a deadline of 300 ms', 800, 300))
    local value, failure = async.timeout(slow, 300):await()
    self:check('missed', 'A deadline missed', value == nil and failure ~= nil, string.format('Rejected after %d ms: %s', datetime.now():millis() - started, tostring(failure)))

    self.results:set('later', 'waiting', 'The work goes on', 'Waiting for the slow job.')
    local result = slow:await()
    self:check('later', 'The work goes on', result == 'done', string.format('The job the deadline gave up on still resolved with "%s" after %d ms.', tostring(result), datetime.now():millis() - started))
end

function Cancellation:cancelTask()
    local closed, finished = false, false
    local lane = self:lane('A task of 400 ms, cancelled after 100 ms', 400, 100)
    local task = async.spawn(function()
        local guard <close> = setmetatable({}, {
            __close = function() closed = true end,
        })
        async.sleep(400):await()
        finished = true
    end)
    async.sleep(100):await()
    task.cancel()
    lane.stopped = datetime.now():millis()
    local closedAtOnce = closed

    self.results:set('cancel', 'waiting', 'Cancel a task', 'Waiting past the end of its sleep.')
    async.sleep(450):await()
    self:check('cancel', 'Cancel a task', closedAtOnce and not finished, string.format('Closed at once: %s. Resumed after its sleep: %s. A cancelled task never runs again.', tostring(closedAtOnce), tostring(finished)))
end

-- A plain table owner ends when the garbage collector frees it, and the task it held never resumes.
function Cancellation:owner()
    local resumed = false
    local lane = self:lane('A task of 300 ms whose owner went away at once', 300, 0)
    local owner = {}
    scene.spawn(owner, function()
        async.sleep(300):await()
        resumed = true
    end)
    owner = nil
    collectgarbage()
    lane.stopped = datetime.now():millis()

    self.results:set('owner', 'waiting', 'A task with an owner', 'Waiting past the end of its sleep.')
    async.sleep(350):await()
    self:check('owner', 'A task with an owner', not resumed, string.format('Resumed after its owner was gone: %s. The tasks of a test end the same way when the test exits.', tostring(resumed)))
end

function Cancellation:draw(area)
    local y = self.results:draw(area) + 8
    local left, right = 560, area.width - 24
    local scale = (right - left) / Cancellation.span
    local now = datetime.now():millis()
    for _, lane in ipairs(self.lanes or {}) do
        local ended = lane.stopped or lane.finish
        local reached = math.min(lane.length, (ended or now) - lane.start)
        local color = lane.stopped and Test.red or (lane.finish and Test.green or Test.warm)
        Test.caption(lane.label, 24, y)
        graphics2d.drawRect({left, y, lane.length * scale, 22}, Test.surface)
        graphics2d.drawRect({left, y, math.max(2, reached * scale), 22}, color, {layer = 1})
        graphics2d.drawLine(left + lane.limit * scale, y - 6, left + lane.limit * scale, y + 28, 3, Test.ink, {layer = 2})
        y = y + 44
    end
end

return Cancellation
