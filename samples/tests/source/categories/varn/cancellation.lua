-- Deadlines and cancellation with Varn's "async" module: "async.timeout" gives up waiting while the work goes on, "task.cancel()" stops a task for good and closes its to-be-closed variables, and a task of "scene.spawn" ends with its owner.
local async = require('async')
local datetime = require('datetime')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')

local Test = require('harness.test')
local VarnTest = require('categories.varn.varn-test')

local Cancellation = haylen.class('Cancellation', VarnTest)

Cancellation.hint = 'Each bar is the planned work, green when it ends and red where it stopped, and the blue mark is the deadline or the cancel. Run again repeats the lesson.'
Cancellation.span = 1000
Cancellation.stageWidth = 580
Cancellation.bar = 20
-- The height of a lane, its name over its bar and the name of its mark, which the lanes spread out from to share the stage.
Cancellation.laneHeight = 96
Cancellation.margin = 24
Cancellation.legend = {{'Running', Test.warm}, {'Ended', Test.green}, {'Stopped', Test.red}, {'Deadline or cancel', Test.accent}}

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
    self:check('cancel', 'Cancel a task', closedAtOnce and not finished, (closedAtOnce and 'The task closed its variables at once' or 'The task kept its variables open') .. (finished and ', and it still resumed after its sleep.' or ', and it never resumed after its sleep, since a cancelled task never runs again.'))
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
    self:check('owner', 'A task with an owner', not resumed, resumed and 'The task resumed after its owner was gone.' or 'The task never resumed once its owner was gone, the way the tasks of a test end when the test exits.')
end

-- Draws the legend, then every lane as its name over a groove of its planned work, filled with round ends as far as the work went, with the deadline or the cancel marked across it and named, over the lines of every 250 ms.
function Cancellation:draw(area)
    local margin, bar = Cancellation.margin, Cancellation.bar
    local left, right = margin, area.width - margin
    local scale = (right - left) / Cancellation.span
    local x = margin
    for _, entry in ipairs(Cancellation.legend) do
        local style = {size = 18, color = Test.muted, anchor = {0, 0.5}, layer = 2}
        graphics2d.drawCircle(x + 8, margin + 12, 8, entry[2])
        graphics2d.drawText(nil, entry[1], x + 24, margin + 12, style)
        x = x + 48 + graphics2d.measureText(nil, entry[1], style)
    end

    local axis = area.height - margin - 20
    for step = 0, Cancellation.span, 250 do
        local tick = left + step * scale
        graphics2d.drawLine(tick, margin + 44, tick, axis - 8, 1, Test.line)
        graphics2d.drawText(nil, step == Cancellation.span and step .. ' ms' or tostring(step), tick, axis, {size = 18, color = Test.muted, anchor = {step == 0 and 0 or step == Cancellation.span and 1 or 0.5, 0}, layer = 2})
    end

    local now = datetime.now():millis()
    local lanes = self.lanes or {}
    local first = margin + 56
    local step = math.max(Cancellation.laneHeight, (axis - 8 - first) / math.max(1, #lanes))
    local y = first + (step - Cancellation.laneHeight) / 2
    for _, lane in ipairs(lanes) do
        local ended = lane.stopped or lane.finish
        local reached = math.min(lane.length, (ended or now) - lane.start)
        local color = lane.stopped and Test.red or (lane.finish and Test.green or Test.warm)
        graphics2d.drawText(nil, lane.label, left, y, {size = 19, color = Test.ink, maxWidth = right - left, layer = 2})
        local top = y + 30
        graphics2d.drawShape({left, top, lane.length * scale, bar}, {radius = bar / 2, color = Test.line, layer = 1})
        graphics2d.drawShape({left, top, math.max(bar, reached * scale), bar}, {radius = bar / 2, color = color, layer = 2})
        local mark = left + lane.limit * scale
        graphics2d.drawShape({mark - 3, top - 8, 6, bar + 16}, {radius = 3, color = Test.accent, layer = 3})
        graphics2d.drawText(nil, string.format('%d ms', lane.limit), mark + 8, top + bar + 6, {size = 16, color = Test.accent, layer = 3})
        y = y + step
    end
end

return Cancellation
