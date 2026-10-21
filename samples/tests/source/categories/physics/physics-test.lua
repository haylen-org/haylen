-- The base of the physics tests: a world that the play area fits, a pointer for every device, the actions every test reads, the time of the physics steps and the labels that name what a stage shows.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local profiler = require('haylen.debug')

local Look = require('categories.physics.look')
local Pointer = require('harness.pointer')
local Test = require('harness.test')

local PhysicsTest = haylen.class('PhysicsTest', Test)

-- The world the camera fits into the play area, as wide as the play area of a desktop window is against its height, so the stage fills it there.
PhysicsTest.defaultView = {1600, 760}
PhysicsTest.scope = 'physics step'
PhysicsTest.gamepadHint = 'A gamepad aims with the right stick and presses with the right trigger.'
PhysicsTest.actions = {
    {name = 'reset', type = 'button', bindings = {'key:r', 'button:west'}},
    {name = 'previous', type = 'button', bindings = {'key:q', 'button:leftShoulder'}},
    {name = 'next', type = 'button', bindings = {'key:e', 'button:rightShoulder'}},
}

-- Labels are plates of the slate of the interface with white text in the font of the art, this many pixels of the screen tall.
PhysicsTest.labelSize = 22
PhysicsTest.plate = '#EE2F3352'
PhysicsTest.plateEdge = '#FF15172A'
-- The plates over the two sides of a test that shows a problem next to its solution.
PhysicsTest.problemPlate = '#F2C2362F'
PhysicsTest.solutionPlate = '#F2278A4F'

function PhysicsTest:init(entry)
    PhysicsTest.super.init(self, entry)
    self.pointer = Pointer()
    self.timings = {}
    self.labelFont = assets.font('fonts/lilita-one-regular.ttf', {bakeSize = 64, spread = 16})
    self.labelText = {size = 1, color = '#FFFFFFFF', anchor = {0.5, 0.5}, layer = 0}
    self.labelPlate = {kind = 'rectangle', radius = 0, color = PhysicsTest.plate, borderWidth = 0, borderColor = PhysicsTest.plateEdge, layer = 0}
    self.labelRect = {0, 0, 0, 0}
    self.tagText = {size = 1, color = '#FFFFFFFF', outlineWidth = 1, outlineColor = Look.ink, anchor = {0.5, 0.5}, align = 'center', layer = 0}
    self.divider = {0, 0, 0, 0}
    self.dividerStroke = {width = 1, color = '#B3FFFFFF', dashes = {1, 1}, cap = 'round', layer = 35}
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
    Look.pixel = 1 / self.camera.zoom.x
    self.pointer:update(dt, self)
end

function PhysicsTest:renderUi()
    self.pointer:draw()
end

-- Draws a label on a plate at `x`, `y` of the world, which keeps its size on the screen at any zoom. The table `options` takes the `anchor` of the plate, `{0.5, 0.5}` by default, a text `color`, a `plate` color, the `size` in pixels of the screen and the `layer`.
function PhysicsTest:label(text, x, y, options)
    options = options or {}
    local unit = Look.pixel
    local style = self.labelText
    style.size = (options.size or PhysicsTest.labelSize) * unit
    style.color = options.color or '#FFFFFFFF'
    style.layer = (options.layer or 40) + 1
    local width, height = graphics2d.measureText(self.labelFont, text, style)
    local plateWidth, plateHeight = width + style.size * 0.9, height + style.size * 0.35
    local anchor = options.anchor
    local left = x - plateWidth * (anchor and anchor[1] or 0.5)
    local top = y - plateHeight * (anchor and anchor[2] or 0.5)
    local plate, area = self.labelPlate, self.labelRect
    area[1], area[2], area[3], area[4] = left, top, plateWidth, plateHeight
    plate.radius, plate.borderWidth, plate.layer = math.min(plateHeight / 2, 10 * unit), 2 * unit, options.layer or 40
    plate.color = options.plate or PhysicsTest.plate
    graphics2d.drawShape(area, plate)
    graphics2d.drawText(self.labelFont, text, left + plateWidth / 2, top + plateHeight / 2, style)
end

-- Draws a short text with the bold outline of the art at `x`, `y` of the world, such as the value written on a body, which keeps its size on the screen at any zoom. The table `options` takes the `size` in pixels of the screen, a `color` and the `layer`.
function PhysicsTest:tag(text, x, y, options)
    options = options or {}
    local unit = Look.pixel
    local style = self.tagText
    style.size = (options.size or 18) * unit
    style.outlineWidth = 3.5 * unit
    style.color = options.color or '#FFFFFFFF'
    style.layer = options.layer or 40
    graphics2d.drawText(self.labelFont, text, x, y, style)
end

-- Draws the titles of the two halves of a test that shows a problem on the left of `x` and its solution on the right, at `y`, with a dashed line between the halves.
function PhysicsTest:compare(problem, solution, x, y, options)
    local unit = Look.pixel
    local bounds = graphics2d.canvasBounds()
    self.divider[1], self.divider[2], self.divider[3], self.divider[4] = x, bounds.y, x, bounds.y + bounds.height
    self.dividerStroke.width, self.dividerStroke.dashes[1], self.dividerStroke.dashes[2] = 3 * unit, 14 * unit, 10 * unit
    graphics2d.drawStroke(self.divider, self.dividerStroke)
    local offset = options and options.offset or 400
    self:label(problem, x - offset, y, {size = 24, plate = PhysicsTest.problemPlate})
    self:label(solution, x + offset, y, {size = 24, plate = PhysicsTest.solutionPlate})
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

-- Formats an angle in radians as whole degrees, without the sign of a zero.
function PhysicsTest.degrees(radians)
    local value = math.floor(math.deg(radians) + 0.5)
    return value == 0 and 0 or value
end

return PhysicsTest
