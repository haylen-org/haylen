-- Smoothing and look-ahead: position smoothing eases the view toward its goal, and the look-ahead moves the goal ahead of the walker by its velocity.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local CameraTest = require('categories.camera.camera-test')
local Test = require('harness.test')
local Walker = require('categories.camera.walker')
local World = require('categories.camera.world')
local actions = require('categories.camera.actions')

local Smoothing = haylen.class('Smoothing', CameraTest)

Smoothing.code = [[
camera.positionSmoothing = true
camera.positionSmoothingSpeed = 4
camera.lookAheadTime = 0.4  -- Leads the walker by 0.4 seconds of its velocity.
camera.maxLookAhead = 360
camera:follow(walker.x, walker.y, dt)]]

-- Until a player steers, the walker runs along the road and stops at its ends, so the view shows its lead and its catching up from the start.
Smoothing.route = {{-350, 230, 1.2}, {-1050, 170}, {-1800, 330, 1.2}, {-1050, 170}}

function Smoothing:init(entry)
    Smoothing.super.init(self, entry)
    self.world = World()
    self.walker = Walker(-1800, 330, {action = 'move', route = Smoothing.route})
    self.camera = graphics2d.newCamera()
    self.camera.positionSmoothing = true
    self.camera.positionSmoothingSpeed = 4
    self.camera.lookAheadTime = 0.4
    self.camera.maxLookAhead = 360
    self.camera:snapTo(self.walker.x, self.walker.y)
end

function Smoothing:enter()
    local camera = self.camera
    self:loadActions(actions)
    self:frame{
        hint = 'Walk with WASD, the arrows, the left stick or by holding the mouse button or a finger, then stop suddenly: the view catches up smoothly and leads where you walk.',
        code = Smoothing.code,
        play = true,
        controls = {
            ui.toggle{id = 'smoothing', text = 'Position smoothing', checked = true, onChange = function(event)
                camera.positionSmoothing = event.checked
            end},
            ui.formField{label = 'Smoothing speed', ui.slider{id = 'speed', min = 0.5, max = 15, value = camera.positionSmoothingSpeed, showValue = true, decimals = 1, onChange = function(event)
                camera.positionSmoothingSpeed = event.value
            end}},
            ui.formField{label = 'Look-ahead in seconds', ui.slider{id = 'lookAhead', min = 0, max = 1, value = camera.lookAheadTime, showValue = true, decimals = 2, onChange = function(event)
                camera.lookAheadTime = event.value
            end}},
        },
    }
end

function Smoothing:update(dt)
    Smoothing.super.update(self, dt)
    local camera, walker = self.camera, self.walker
    walker:update(dt, self.world, camera)
    camera:follow(walker.x, walker.y, dt)
    camera:update(dt)
    local lead = math.sqrt((camera.x - walker.x) ^ 2 + (camera.y - walker.y) ^ 2)
    self:status(string.format('The view center is %d units from the walker.', math.floor(lead + 0.5)))
end

function Smoothing:draw(area)
    self.world:draw()
    self.walker:draw()
    -- A trail of dots from the walker to the center of the view shows the lead.
    local camera, walker = self.camera, self.walker
    local dx, dy = camera.x - walker.x, camera.y - walker.y
    local length = math.sqrt(dx * dx + dy * dy)
    for step = 30, length - 20, 26 do
        graphics2d.drawCircle(walker.x + dx * step / length, walker.y - 60 + (dy + 60) * step / length, 5, '#C0FFE08A', {layer = 10})
    end
    graphics2d.drawCircle(camera.x, camera.y, 12, '#FFFFE08A', {layer = 10})
    graphics2d.drawRing(camera.x, camera.y, 24, 4, '#C0FFE08A', {layer = 10})
    CameraTest.tag('View center', camera.x, camera.y + 84, {color = Test.warm})
end

return Smoothing
