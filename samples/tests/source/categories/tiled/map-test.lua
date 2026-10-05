-- The base of the Tiled tests: a view of the map that the play area fits, zoomed further by the zoom scale of the test, a pointer for every device, the actions every test reads, and a panel line of details.
local haylen = require('haylen')
local profiler = require('haylen.debug')

local Pointer = require('harness.pointer')
local Test = require('harness.test')

local MapTest = haylen.class('MapTest', Test)

MapTest.defaultView = {1600, 860}
MapTest.gamepadHint = 'A gamepad aims with the right stick and presses with the right trigger.'
MapTest.actions = {
    {name = 'reset', type = 'button', bindings = {'key:r', 'button:west'}},
    {name = 'previous', type = 'button', bindings = {'key:q', 'button:leftShoulder'}},
    {name = 'next', type = 'button', bindings = {'key:e', 'button:rightShoulder'}},
    {name = 'move', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'virtualStick:move'}},
}

function MapTest:init(entry)
    MapTest.super.init(self, entry)
    self.pointer = Pointer()
    self.zoomScale = 1
    self.timings = {}
end

-- Mounts the frame of the harness around the map and loads the actions of every Tiled test and the pointer.
function MapTest:frame(options)
    options.view = options.view or MapTest.defaultView
    options.hint = options.hint .. ' ' .. MapTest.gamepadHint
    MapTest.super.frame(self, options)

    local actions = {}
    for _, list in ipairs({MapTest.actions, Pointer.actions}) do
        for _, action in ipairs(list) do
            actions[#actions + 1] = action
        end
    end
    self:loadActions({actions = actions})
end

-- The frame fits the view into the play area every frame once it has a size, and the zoom scale of the test multiplies that fit.
function MapTest:update(dt)
    MapTest.super.update(self, dt)
    if self.stage then
        local zoom = self.camera.zoom
        self.camera.zoom = {zoom.x * self.zoomScale, zoom.y * self.zoomScale}
    end
    self.pointer:update(dt, self)
end

function MapTest:renderUi()
    self.pointer:draw()
end

-- Shows lines that change only now and then, such as the object under the pointer, in the `details` label of the panel.
function MapTest:details(text)
    if text ~= self.detailText then
        self.detailText = text
        self:set('details', {text = text})
    end
end

-- Returns the milliseconds of the last frame that ran the profiler scope `name`, and keeps it until the scope runs again, because the profiler only holds the last frame.
function MapTest:timing(name)
    for _, scope in ipairs(profiler.frame().scopes) do
        if scope.name == name and scope.milliseconds > 0 then
            self.timings[name] = scope.milliseconds
        end
    end
    return self.timings[name] or 0
end

return MapTest
