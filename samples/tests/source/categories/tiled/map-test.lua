-- The base of the Tiled tests: a view of the map that the play area fits, zoomed further by the zoom scale of the test, a pointer for every device, the actions every test reads, and a panel line of details.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local profiler = require('haylen.debug')
local tiled = require('haylen.tiled')

local Pointer = require('harness.pointer')
local Test = require('harness.test')

local MapTest = haylen.class('MapTest', Test)

MapTest.defaultView = {1600, 860}
MapTest.ink = '#FF1E1B33'
-- The room around a map when the view fits it, in the units of the map.
MapTest.margin = 32
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

-- Loads a map of the Tiled tests with the `linear` filter, because their tiles are painted in high definition and drawn at the size of the grid.
function MapTest.loadMap(path)
    return tiled.newMapRenderer(assets.load(path, nil, {filter = 'linear'}))
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

-- Draws a frame around a map, a soft shadow under it and a dark rounded border over its edge, so the map reads as a framed board of the play area.
function MapTest.frameMap(bounds, options)
    options = options or {}
    local unit = graphics2d.canvasUnitSize()
    local x, y, width, height = bounds.x, bounds.y, bounds.width, bounds.height
    graphics2d.drawShape({x - 4 * unit, y + 6 * unit, width + 8 * unit, height + 8 * unit}, {radius = 14 * unit, color = '#80000000', softness = 24 * unit, layer = options.under or -10})
    graphics2d.drawShape({x - 3 * unit, y - 3 * unit, width + 6 * unit, height + 6 * unit}, {radius = 10 * unit, color = '#00000000', borderWidth = 5 * unit, borderColor = MapTest.ink, layer = options.over or 8})
end

-- Draws a label in a dark pill with a light text, centered on `x` and with its bottom at `y` unless the options anchor it otherwise, the way the Tiled tests name things on their maps.
function MapTest.pill(text, x, y, options)
    options = options or {}
    local unit = graphics2d.canvasUnitSize()
    local size = (options.size or 20) * unit
    local width, height = graphics2d.measureText(nil, text, {size = size})
    local anchor = options.anchor or {0.5, 1}
    local padX, padY = 12 * unit, 5 * unit
    local left, top = x - (width + padX * 2) * anchor[1], y - (height + padY * 2) * anchor[2]
    local layer = options.layer or 9
    graphics2d.drawShape({left, top, width + padX * 2, height + padY * 2}, {radius = (height + padY * 2) / 2, color = options.fill or '#E61E1B33', borderWidth = 2 * unit, borderColor = options.border or '#FF3A4066', layer = layer})
    graphics2d.drawText(nil, text, left + padX, top + padY, {size = size, color = options.color or '#FFF2F3FF', layer = layer + 1})
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
