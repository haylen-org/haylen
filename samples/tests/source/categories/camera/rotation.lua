-- Rotation: the view turns around its center, rotation smoothing eases the drawn angle the short way around, and `ignoreRotation` keeps the view upright whatever the rotation and the shake.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local Test = require('harness.test')
local Walker = require('categories.camera.walker')
local World = require('categories.camera.world')
local actions = require('categories.camera.actions')

local Rotation = haylen.class('Rotation', Test)

function Rotation:init(entry)
    Rotation.super.init(self, entry)
    self.world = World()
    self.walker = Walker(0, 0, '#FFFFD040', 'move')
    self.camera = graphics2d.newCamera()
    self.camera.rotationSmoothing = true
    self.camera.rotationSmoothingSpeed = 4
    self.camera.maxShakeAngle = 0.15
end

function Rotation:enter()
    local camera = self.camera
    self:loadActions(actions)
    self:frame{
        hint = 'Turn with Q and E or the right stick, or a quarter turn at a time with the button. Walk with WASD, the arrows, the left stick or by holding the mouse button or a finger.',
        play = true,
        controls = {
            ui.row{gap = 8,
                ui.button{id = 'quarter', text = 'Quarter turn', grow = 1, onClick = function() camera.rotation = camera.rotation + math.pi / 2 end},
                ui.button{id = 'shake', text = 'Shake', grow = 1, onClick = function() camera:addTrauma(0.8) end},
            },
            ui.toggle{id = 'smoothing', text = 'Rotation smoothing', checked = true, onChange = function(event)
                camera.rotationSmoothing = event.checked
            end},
            ui.toggle{id = 'ignore', text = 'Property "ignoreRotation"', onChange = function(event)
                camera.ignoreRotation = event.checked
            end},
        },
    }
end

function Rotation:update(dt)
    Rotation.super.update(self, dt)
    local camera = self.camera
    camera.rotation = camera.rotation + input.value('rotate') * 1.5 * dt
    self.walker:update(dt, camera)
    camera:follow(self.walker.x, self.walker.y, dt)
    camera:update(dt)
    self:status(string.format('Rotation %.2f   Drawn with %.2f   Property "ignoreRotation" is "%s"', camera.rotation, camera:renderRotation(), camera.ignoreRotation))
end

function Rotation:draw(area)
    self.world:draw()
    self.walker:draw()
end

return Rotation
