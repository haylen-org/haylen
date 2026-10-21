-- Drags the dynamic body under the pointer with a grabber of the world, whose pull scales with the mass of everything joined to the held body, so a coin and a ragdoll follow the pointer alike with the mouse, a finger or a gamepad.
local haylen = require('haylen')
local physics2d = require('haylen.physics2d')

local Look = require('categories.physics.look')

local Grab = haylen.class('Grab')

-- A press takes the nearest body within this many screen units, so a finger finds thin and small bodies.
Grab.reach = 28

-- The table `options` takes the `strength` of the pull in multiples of the weight of the held bodies, and the `category` and `mask` that leave out bodies that should not be dragged.
function Grab:init(world, options)
    options = options or {}
    self.grabber = physics2d.newGrabber(world, {strength = options.strength, category = options.category, mask = options.mask})
end

function Grab:update(pointer, camera)
    if pointer.pressed then
        self.grabber.pickRadius = Grab.reach / camera.zoom.x
        self.grabber:grab(pointer.worldX, pointer.worldY)
    elseif pointer.released then
        self.grabber:release()
    end
    self.grabber:moveTo(pointer.worldX, pointer.worldY)
end

function Grab:release()
    self.grabber:release()
end

function Grab:holding()
    return self.grabber.holding
end

function Grab:draw()
    if not self.grabber.holding then
        return
    end
    local handle, target = self.grabber.handle, self.grabber.target
    local unit = Look.pixel
    Look.line(handle.x, handle.y, target.x, target.y, 4 * unit, '#FFFFE6A0', 50)
    Look.pin(handle.x, handle.y, 6 * unit, 50, '#FFFFC93C')
    Look.pin(target.x, target.y, 9 * unit, 50, '#FFFFFFFF')
end

return Grab
