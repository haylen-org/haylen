-- The base of the physics tests: a world that the play area fits, a pointer for every device, the actions every test reads, and the time of the physics steps.
local haylen = require('haylen')
local profiler = require('haylen.debug')

local Pointer = require('harness.pointer')
local Test = require('harness.test')

local PhysicsTest = haylen.class('PhysicsTest', Test)

PhysicsTest.defaultView = {1600, 860}
PhysicsTest.scope = 'physics step'
PhysicsTest.gamepadHint = 'A gamepad aims with the right stick and presses with the right trigger.'
PhysicsTest.actions = {
    {name = 'reset', type = 'button', bindings = {'key:r', 'button:west'}},
    {name = 'previous', type = 'button', bindings = {'key:q', 'button:leftShoulder'}},
    {name = 'next', type = 'button', bindings = {'key:e', 'button:rightShoulder'}},
}

function PhysicsTest:init(entry)
    PhysicsTest.super.init(self, entry)
    self.pointer = Pointer()
    self.timings = {}
end

-- Mounts the frame of the harness around the world of the test and loads the actions of every physics test, the pointer and the `actions` of the options. Tests that drive a character instead of the pointer pass `pointer = false`.
function PhysicsTest:frame(options)
    options.view = options.view or PhysicsTest.defaultView
    if options.pointer ~= false then
        options.hint = options.hint .. ' ' .. PhysicsTest.gamepadHint
    end
    PhysicsTest.super.frame(self, options)

    local actions = {}
    for _, list in ipairs({PhysicsTest.actions, Pointer.actions, options.actions or {}}) do
        for _, action in ipairs(list) do
            actions[#actions + 1] = action
        end
    end
    self:loadActions({actions = actions})
end

function PhysicsTest:update(dt)
    PhysicsTest.super.update(self, dt)
    self.pointer:update(dt, self)
end

function PhysicsTest:renderUi()
    self.pointer:draw()
end

-- Steps a world inside the profiler scope that `stepTime` reads.
function PhysicsTest:simulate(world, step)
    profiler.beginScope(PhysicsTest.scope)
    world:step(step)
    profiler.endScope()
end

-- Returns the milliseconds of the last frame that ran the profiler scope `name`, and keeps it until the scope runs again, because the profiler only holds the last frame.
function PhysicsTest:timing(name)
    for _, scope in ipairs(profiler.frame().scopes) do
        if scope.name == name and scope.milliseconds > 0 then
            self.timings[name] = scope.milliseconds
        end
    end
    return self.timings[name] or 0
end

-- Returns the milliseconds of the last physics step.
function PhysicsTest:stepTime()
    return self:timing(PhysicsTest.scope)
end

return PhysicsTest
