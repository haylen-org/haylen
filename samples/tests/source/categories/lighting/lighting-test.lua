-- The base of the lighting tests: a lit canvas over the play area that fits a world of 1920 by 1080 around the origin, and for the tests that ask for it a cursor that starts in the middle and follows the mouse, a finger, the arrows, WASD, the directional pad and the sticks inside the play area. A click, a tap, E, Enter, Space, the south or west button or the right trigger place what a test places.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')

local Pointer = require('harness.pointer')
local Test = require('harness.test')

local LightingTest = haylen.class('LightingTest', Test)

LightingTest.view = {1920, 1080}
LightingTest.actions = {actions = {
    {name = 'cursor', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'stick:right'}},
    {name = 'press', type = 'button', bindings = {'axis:rightTrigger+'}},
    {name = 'place', type = 'button', bindings = {'key:e', 'key:enter', 'key:space', 'button:south', 'button:west'}},
}}

function LightingTest:init(entry)
    LightingTest.super.init(self, entry)
    self.pointer = Pointer({confined = true, shown = true})
    self.cursorX, self.cursorY = 0, 0
    self.ambientLight = '#FF141820'
end

-- Mounts the frame of `Test:frame` with the world of the category. With `cursor = true` the test reads the cursor, which takes the action map and the first focus.
function LightingTest:frame(options)
    self.cursor = options.cursor
    options.cursor = nil
    if self.cursor then
        self:loadActions(LightingTest.actions)
        options.play = true
    end
    options.view = options.view or LightingTest.view
    LightingTest.super.frame(self, options)
end

-- Follows the play area, then moves the cursor, which stays inside the play area, in screen and world coordinates.
function LightingTest:update(dt)
    LightingTest.super.update(self, dt)
    if not self.stage or not self.cursor then
        return
    end
    self.pointer:update(dt, self)
    self.cursorX, self.cursorY = self.pointer.worldX, self.pointer.worldY
end

-- Tells whether the cursor pressed this frame, to place something.
function LightingTest:pressed()
    return self.pointer.pressed or input.pressed('place')
end

-- The options of the lit canvas: the ambient light of the test, which some tests change while they run.
function LightingTest:canvas()
    return {ambientLight = self.ambientLight}
end

function LightingTest:render()
    if self.area then
        graphics2d.beginWorld(self.camera, self:canvas())
        self:draw(self.area)
    end
end

function LightingTest:renderUi()
    if self.stage and self.cursor then
        self.pointer:draw()
    end
end

return LightingTest
