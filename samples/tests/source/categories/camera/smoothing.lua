-- Smoothing and look-ahead: position smoothing eases the view toward its goal, and the look-ahead moves the goal ahead of the walker by its velocity.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local Walker = require('categories.camera.walker')
local World = require('categories.camera.world')
local actions = require('categories.camera.actions')

local Smoothing = haylen.class('Smoothing', Test)

function Smoothing:init(entry)
    Smoothing.super.init(self, entry)
    self.world = World()
    self.walker = Walker(0, 0, '#FFFFD040', 'move')
    self.camera = graphics2d.newCamera()
    self.camera.positionSmoothing = true
    self.camera.positionSmoothingSpeed = 4
    self.camera.lookAheadTime = 0.4
    self.camera.maxLookAhead = 360
end

function Smoothing:enter()
    local camera = self.camera
    self:loadActions(actions)
    self:frame{
        hint = 'Walk with WASD, the arrows, the left stick or by holding the mouse button or a finger, then stop suddenly: the view catches up smoothly and leads where you walk.',
        play = true,
        controls = {
            ui.toggle{id = 'smoothing', text = 'Position smoothing', checked = true, onChange = function(event)
                camera.positionSmoothing = event.checked
            end},
            ui.formField{label = 'Smoothing speed', ui.slider{id = 'speed', min = 0.5, max = 15, value = camera.positionSmoothingSpeed, showValue = true, onChange = function(event)
                camera.positionSmoothingSpeed = event.value
            end}},
            ui.formField{label = 'Look-ahead time in seconds', ui.slider{id = 'lookAhead', min = 0, max = 1, value = camera.lookAheadTime, showValue = true, onChange = function(event)
                camera.lookAheadTime = event.value
            end}},
        },
    }
end

function Smoothing:update(dt)
    Smoothing.super.update(self, dt)
    local camera = self.camera
    self.walker:update(dt, camera)
    camera:follow(self.walker.x, self.walker.y, dt)
    camera:update(dt)
    self:status(string.format('Lead %.0f, %.0f   Smoothing "%s" at %.1f', camera.x - self.walker.x, camera.y - self.walker.y, camera.positionSmoothing, camera.positionSmoothingSpeed))
end

function Smoothing:draw(area)
    self.world:draw()
    self.walker:draw()
    self.camera:drawDebug({layer = 10})
end

return Smoothing
