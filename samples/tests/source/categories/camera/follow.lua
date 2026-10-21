-- Dead zone and drag margins: the view stays put while the walker moves inside a box around its center, a fixed dead zone in world units or drag margins in fractions of the view.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local CameraTest = require('categories.camera.camera-test')
local Test = require('harness.test')
local Walker = require('categories.camera.walker')
local World = require('categories.camera.world')
local actions = require('categories.camera.actions')

local Follow = haylen.class('Follow', CameraTest)

Follow.code = [[
camera.deadZone = {420, 260}  -- The walker moves inside this box without moving the view.
camera.dragHorizontal = true
camera.dragVertical = true
camera.dragMargins = {0.5, 0.35, 0.5, 0.35}  -- Or inside margins in fractions of half the view.
camera:follow(walker.x, walker.y, dt)]]

Follow.modes = {{id = 'none', text = 'Nothing, locked on'}, {id = 'deadZone', text = 'Dead zone of 420 by 260'}, {id = 'drag', text = 'Drag margins of half the view'}}
Follow.margins = {0.5, 0.35, 0.5, 0.35}
-- Until a player steers, the walker takes small steps inside the box and then long strides that push its edge.
Follow.route = {{-880, 80, 0.8}, {-1110, 50, 0.8}, {-1000, 60, 0.6}, {-560, 120, 1}, {-1000, 60, 0.6}, {-1450, 230, 1}}

function Follow:init(entry)
    Follow.super.init(self, entry)
    self.world = World()
    self.walker = Walker(-1000, 60, {action = 'move', route = Follow.route})
    self.camera = graphics2d.newCamera()
    self.camera:snapTo(self.walker.x, self.walker.y)
    self.mode = 'deadZone'
    self:apply()
end

function Follow:enter()
    self:loadActions(actions)
    self:frame{
        hint = 'Walk with WASD, the arrows, the left stick or by holding the mouse button or a finger. The view moves only once the walker pushes the edge of the box.',
        code = Follow.code,
        play = true,
        controls = {ui.formField{label = 'Follow with', ui.radioGroup{id = 'mode', selected = self.mode, items = Follow.modes, onChange = function(event)
            self.mode = event.value
            self:apply()
        end}}},
    }
end

function Follow:apply()
    local camera = self.camera
    camera.deadZone = self.mode == 'deadZone' and {420, 260} or {0, 0}
    camera.dragHorizontal = self.mode == 'drag'
    camera.dragVertical = self.mode == 'drag'
    camera.dragMargins = Follow.margins
    camera:align()
end

-- Returns the box around the view center where the walker moves without moving the view, or nothing when the view locks on.
function Follow:freeBox()
    local camera = self.camera
    if self.mode == 'deadZone' then
        return {camera.x - 210, camera.y - 130, 420, 260}
    end
    if self.mode == 'drag' then
        local halfWidth, halfHeight = camera.viewSize.x / 2 / camera.zoom.x, camera.viewSize.y / 2 / camera.zoom.y
        local margins = Follow.margins
        return {camera.x - margins[1] * halfWidth, camera.y - margins[2] * halfHeight, (margins[1] + margins[3]) * halfWidth, (margins[2] + margins[4]) * halfHeight}
    end
end

function Follow:update(dt)
    Follow.super.update(self, dt)
    self.walker:update(dt, self.world, self.camera)
    local camera = self.camera
    local x, y = camera.x, camera.y
    camera:follow(self.walker.x, self.walker.y, dt)
    camera:update(dt)
    if self.mode == 'none' then
        self:status('The view stays locked on the walker.')
    elseif math.abs(camera.x - x) + math.abs(camera.y - y) > 0.01 then
        self:status('The walker pushes the edge of the box, so the view follows.')
    else
        self:status('The walker moves inside the box, and the view stays put.')
    end
end

function Follow:draw(area)
    self.world:draw()
    self.walker:draw()
    local box = self:freeBox()
    if box then
        graphics2d.drawShape(box, {radius = 18, color = '#14FFE08A', borderWidth = 4, borderColor = '#D0FFE08A', layer = 10})
        CameraTest.tag(self.mode == 'deadZone' and 'Dead zone' or 'Drag margins', box[1] + box[3] / 2, box[2] - 10, {color = Test.warm})
    end
end

return Follow
