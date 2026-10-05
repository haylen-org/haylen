-- The base of the algorithm tests: a world that the play area fits, a pointer for every device, the reset, previous and next actions, and the time of profiler scopes.
local haylen = require('haylen')
local profiler = require('haylen.debug')

local Pointer = require('harness.pointer')
local Test = require('harness.test')

local AlgorithmTest = haylen.class('AlgorithmTest', Test)

AlgorithmTest.defaultView = {1600, 860}
AlgorithmTest.gamepadHint = 'A gamepad aims with the right stick and presses with the right trigger.'
AlgorithmTest.actions = {
    {name = 'reset', type = 'button', bindings = {'key:r', 'button:west'}},
    {name = 'previous', type = 'button', bindings = {'key:q', 'button:leftShoulder'}},
    {name = 'next', type = 'button', bindings = {'key:e', 'button:rightShoulder'}},
}

function AlgorithmTest:init(entry)
    AlgorithmTest.super.init(self, entry)
    self.pointer = Pointer()
    self.timings = {}
end

-- Mounts the frame of the harness around the world of the test and loads the actions every algorithm test reads.
function AlgorithmTest:frame(options)
    options.view = options.view or AlgorithmTest.defaultView
    options.hint = options.hint .. ' ' .. AlgorithmTest.gamepadHint
    AlgorithmTest.super.frame(self, options)

    local actions = {}
    for _, action in ipairs(AlgorithmTest.actions) do
        actions[#actions + 1] = action
    end
    for _, action in ipairs(Pointer.actions) do
        actions[#actions + 1] = action
    end
    self:loadActions({actions = actions})
end

function AlgorithmTest:update(dt)
    AlgorithmTest.super.update(self, dt)
    self.pointer:update(dt, self)
end

function AlgorithmTest:renderUi()
    self.pointer:draw()
end

-- Returns the milliseconds of the last frame that ran the profiler scope `name`, and keeps it until the scope runs again, because the profiler only holds the last frame.
function AlgorithmTest:timing(name)
    for _, scope in ipairs(profiler.frame().scopes) do
        if scope.name == name and scope.milliseconds > 0 then
            self.timings[name] = scope.milliseconds
        end
    end
    return self.timings[name] or 0
end

return AlgorithmTest
