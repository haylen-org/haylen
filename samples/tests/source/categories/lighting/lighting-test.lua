-- The base of the lighting tests: a lit canvas over the play area that fits a world of 1920 by 1080 around the origin, and for the tests that ask for it a cursor that starts in the middle and follows the mouse, a finger, the arrows, WASD, the directional pad and the sticks inside the play area. A click, a tap, E, Enter, Space, the south or west button or the right trigger place what a test places.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')

local Pointer = require('harness.pointer')
local Test = require('harness.test')

local LightingTest = haylen.class('LightingTest', Test)

LightingTest.view = {1920, 1080}
LightingTest.tagSizes = {26, 20}
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

-- Mounts the frame of `Test:frame` with the world of the category. With `cursor = true` the test reads the cursor, which takes the action map and the first focus, and with `cursor = 'lamp'` a lamp of the test marks the cursor in place of the ring of the pointer.
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

-- Opens the screen canvas that the labels of a test draw on, and draws the ring of the pointer unless a lamp marks the cursor.
function LightingTest:renderUi()
    if not self.stage then
        return
    end
    graphics2d.beginScreen()
    if self.cursor == true then
        self.pointer:draw()
    end
end

-- Draws a label for people on a dark plate in screen coordinates: a title and an optional line under it, centered on `x` with its top at `y`, or placed by `anchor`, the shares of its width and height at `x`, `y`.
function LightingTest.tag(title, detail, x, y, anchor)
    anchor = anchor or {0.5, 0}
    local titleWidth, titleHeight = graphics2d.measureText(nil, title, {size = LightingTest.tagSizes[1]})
    local detailWidth, detailHeight = 0, 0
    if detail then
        detailWidth, detailHeight = graphics2d.measureText(nil, detail, {size = LightingTest.tagSizes[2]})
    end
    local padX, padY = 18, 8
    local width, height = math.max(titleWidth, detailWidth) + padX * 2, titleHeight + detailHeight + padY * 2
    local left, top = x - width * anchor[1], y - height * anchor[2]
    graphics2d.drawShape({left, top, width, height}, {radius = detail and 18 or height / 2, color = '#E6151A2A', borderWidth = 2, borderColor = Test.line, layer = 10})
    graphics2d.drawText(nil, title, left + width / 2, top + padY, {size = LightingTest.tagSizes[1], color = Test.ink, anchor = {0.5, 0}, layer = 11})
    if detail then
        graphics2d.drawText(nil, detail, left + width / 2, top + padY + titleHeight, {size = LightingTest.tagSizes[2], color = Test.muted, anchor = {0.5, 0}, layer = 11})
    end
end

return LightingTest
