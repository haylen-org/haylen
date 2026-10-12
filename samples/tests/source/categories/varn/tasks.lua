-- Tasks and promises of Varn's "async" module: a task is a coroutine that waits on promises with ":await()" while the app keeps drawing frames, catches the failures that do not matter and hands the frame back in a long loop.
local async = require('async')
local datetime = require('datetime')
local haylen = require('haylen')
local timer = require('haylen.timer')

local VarnTest = require('categories.varn.varn-test')

local Tasks = haylen.class('Tasks', VarnTest)

Tasks.excerpts = {
    {'Tasks', [[
local task = async.spawn(function()
    steps[#steps + 1] = 'task starts'
    async.sleep(50):await()
    steps[#steps + 1] = 'task resumes'
end)
steps[#steps + 1] = 'spawn returns'

local frame = haylen.frameIndex()
async.sleep(250):await()
print(haylen.frameIndex() - frame)

local entry = async.run(function()
    async.sleep(50):await()
    finished = true
end)]]},
    {'Promises', [[
local ready = async.promise(function()
    async.sleep(100):await()
    return 'ready'
end)
local pending = not ready:isDone()
local value = ready:await()

local value, failure = async.promise(function()
    error('No route to the island.', 0)
end):await()

local ok, failure = pcall(function()
    async.sleep(20):await()
    error('The map is broken.', 0)
end)

local route = async.promise(function()
    error('The bridge is out.', 0)
end):catch(function(failure)
    return 'The ferry, since ' .. failure
end):await()]]},
    {'Handing the frame back', [[
local sum = 0
for index = 1, 2000000 do
    sum = sum + index
    if index % 20000 == 0 then
        async.yield():await()
    end
end]]},
    {'A callback as a promise', [[
local opened, resolve = async.deferred()
timer.after(0.2, resolve, {owner = self})
opened:await()]]},
}

function Tasks:run()
    self:order()
    self:sleep()
    self:promises()
    self:rejection()
    self:errors()
    self:caught()
    self:slices()
    self:deferred()
    self:entryTask()
end

-- A new task runs at once until its first await, and the code after "async.spawn" goes on from there.
function Tasks:order()
    local steps = {}
    local task = async.spawn(function()
        steps[#steps + 1] = 'task starts'
        async.sleep(50):await()
        steps[#steps + 1] = 'task resumes'
    end)
    steps[#steps + 1] = 'spawn returns'
    async.sleep(100):await()

    local order = table.concat(steps, ', ')
    self:check('order', 'Tasks', order == 'task starts, spawn returns, task resumes' and type(task.cancel) == 'function', string.format('The order was "%s", and "async.spawn" returned a handle with "cancel".', order))
end

function Tasks:sleep()
    self.results:set('sleep', 'waiting', 'Sleep without blocking', 'Waiting for "async.sleep(250)".')
    local started, frame = datetime.now():millis(), haylen.frameIndex()
    async.sleep(250):await()

    local waited, frames = datetime.now():millis() - started, haylen.frameIndex() - frame
    self:check('sleep', 'Sleep without blocking', waited >= 240 and frames >= 2, string.format('Waited %d ms for "async.sleep(250)" while the app drew %d frames.', waited, frames))
end

function Tasks:promises()
    local ready = async.promise(function()
        async.sleep(100):await()
        return 'ready'
    end)
    local pending = not ready:isDone()
    local value = ready:await()
    self:check('promise', 'Promises', value == 'ready' and pending and ready:isDone(), string.format('Resolved with "%s", and "isDone" turned from false to true.', tostring(value)))
end

function Tasks:rejection()
    local value, failure = async.promise(function()
        error('No route to the island.', 0)
    end):await()
    self:check('rejection', 'Rejections', value == nil and failure == 'No route to the island.', string.format('The await returned nil and "%s" instead of raising.', tostring(failure)))
end

function Tasks:errors()
    local ok, failure = pcall(function()
        async.sleep(20):await()
        error('The map is broken.', 0)
    end)
    self:check('errors', 'Errors in a task', not ok and failure == 'The map is broken.', string.format('The "pcall" around an await caught "%s", and the task went on.', tostring(failure)))
end

-- A rejection that nothing awaits stops the app, so a failure that the app expects goes to "catch", whose function answers in its place.
function Tasks:caught()
    local route = async.promise(function()
        error('The bridge is out.', 0)
    end):catch(function(failure)
        return 'The ferry, since ' .. failure
    end):await()
    self:check('catch', 'Catching a failure', route == 'The ferry, since The bridge is out.', string.format('The function of "catch" turned the rejection into "%s".', tostring(route)))
end

-- A long loop awaits "async.yield" now and then, so other tasks and timers take their turns, and the work goes on in the next frame once the event loop spent the budget of the frame.
function Tasks:slices()
    self.results:set('yield', 'waiting', 'Handing the frame back', 'Summing 2000000 numbers in slices.')
    local ticks, summing = 0, true
    async.spawn(function()
        while summing do
            ticks = ticks + 1
            async.yield():await()
        end
    end)

    local frame = haylen.frameIndex()
    local sum = 0
    for index = 1, 2000000 do
        sum = sum + index
        if index % 20000 == 0 then
            async.yield():await()
        end
    end
    summing = false
    self:check('yield', 'Handing the frame back', sum == 2000001000000 and ticks > 0, string.format('The sum took %d frames, and another task ran %d times between its slices.', haylen.frameIndex() - frame, ticks))
end

-- A deferred promise turns a callback of the engine into something a task awaits.
function Tasks:deferred()
    local started = datetime.now():millis()
    local opened, resolve = async.deferred()
    timer.after(0.2, resolve, {owner = self})
    local pending = not opened:isDone()
    opened:await()
    self:check('deferred', 'A callback as a promise', pending, string.format('The promise waited %d ms until a timer of "haylen.timer" called "resolve".', datetime.now():millis() - started))
end

function Tasks:entryTask()
    local finished = false
    local entry = async.run(function()
        async.sleep(50):await()
        finished = true
    end)
    async.sleep(100):await()
    self:check('run', 'Entry tasks', finished and type(entry.cancel) == 'function', 'The function of "async.run" ran to its end, and the app went on, because in the engine its end ends the task and not the app.')
end

return Tasks
