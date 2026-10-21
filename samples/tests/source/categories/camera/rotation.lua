-- Rotation: the view turns around its center, rotation smoothing eases the drawn angle the short way around, and `ignoreRotation` keeps the view upright whatever the rotation and the shake.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local CameraTest = require('categories.camera.camera-test')
local Walker = require('categories.camera.walker')
local World = require('categories.camera.world')
local actions = require('categories.camera.actions')

local Rotation = haylen.class('Rotation', CameraTest)

Rotation.code = [[
camera.rotationSmoothing = true
camera.rotationSmoothingSpeed = 4
camera.rotation = camera.rotation + math.pi / 2  -- The drawn angle eases there the short way around.
camera.ignoreRotation = true  -- Keeps the view upright.
camera:update(dt)]]

-- Until a player turns the view, it turns a quarter at a time every few seconds.
Rotation.beat = 3

function Rotation:init(entry)
    Rotation.super.init(self, entry)
    self.world = World()
    self.walker = Walker(World.cottage.x + 60, World.cottage.y + 190, {action = 'move'})
    self.camera = graphics2d.newCamera()
    self.camera.rotationSmoothing = true
    self.camera.rotationSmoothingSpeed = 4
    self.camera.maxShakeAngle = 0.15
    self.camera.rotation = 0.35
    self.camera:snapTo(self.walker.x, self.walker.y - 100)
    self.time = 0
    self.turning = true
end

function Rotation:enter()
    local camera = self.camera
    self:loadActions(actions)
    self:frame{
        hint = 'Turn with Q and E or the right stick, or a quarter turn at a time with the button. Walk with WASD, the arrows, the left stick or by holding the mouse button or a finger.',
        code = Rotation.code,
        play = true,
        controls = {
            ui.row{gap = 8,
                ui.button{id = 'quarter', text = 'Quarter turn', grow = 1, onClick = function() self:turn() end},
                ui.button{id = 'shake', text = 'Shake', grow = 1, onClick = function() camera:addTrauma(0.8) end},
            },
            ui.toggle{id = 'smoothing', text = 'Ease the turns', checked = true, onChange = function(event)
                camera.rotationSmoothing = event.checked
            end},
            ui.toggle{id = 'ignore', text = 'Keep the view upright', onChange = function(event)
                camera.ignoreRotation = event.checked
            end},
        },
    }
end

function Rotation:turn()
    self.turning = false
    self.camera.rotation = self.camera.rotation + math.pi / 2
end

function Rotation:update(dt)
    Rotation.super.update(self, dt)
    local camera = self.camera
    local turn = input.value('rotate')
    if turn ~= 0 then
        self.turning = false
        camera.rotation = camera.rotation + turn * 1.5 * dt
    end
    if self.turning then
        self.time = self.time + dt
        if self.time >= Rotation.beat then
            self.time = 0
            camera.rotation = camera.rotation + math.pi / 2
        end
    end
    self.walker:update(dt, self.world, camera)
    camera:follow(self.walker.x, self.walker.y - 100, dt)
    camera:update(dt)
    if camera.ignoreRotation then
        self:status('The view stays upright whatever its rotation.')
    else
        self:status(string.format('The view is turned by %d degrees.', math.floor(math.deg(camera:renderRotation()) % 360 + 0.5) % 360))
    end
end

function Rotation:draw(area)
    self.world:draw()
    self.walker:draw()
end

return Rotation
