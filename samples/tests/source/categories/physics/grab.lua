-- Drags the dynamic body under the pointer with a mouse joint whose target follows the pointer.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Grab = haylen.class('Grab')

-- The pull accelerates the grabbed body at up to this many meters per second squared, times the `strength` of the options.
Grab.acceleration = 1000

-- The table `options` takes the `filter` of `world:pick` that leaves out bodies that should not be dragged, and a `strength` that multiplies the pull, such as the number of parts of a ragdoll so one limb drags the whole figure.
function Grab:init(world, options)
    options = options or {}
    self.world, self.filter, self.strength = world, options.filter, options.strength or 1
    self.anchor = world:createBody({type = 'static'})
end

function Grab:update(pointer, camera)
    if pointer.pressed then
        self:take(pointer, camera)
    elseif pointer.released or (self.joint ~= nil and not self.joint.valid) then
        self:release()
    elseif self.joint ~= nil then
        self.joint.target = {pointer.worldX, pointer.worldY}
    end
    self.targetX, self.targetY = pointer.worldX, pointer.worldY
end

function Grab:take(pointer, camera)
    for _, shape in ipairs(self.world:pick(camera, pointer.x, pointer.y, self.filter)) do
        local body = shape.body
        if body.type == 'dynamic' and not shape.sensor then
            local force = Grab.acceleration * self.world.pixelsPerMeter * body.mass * self.strength
            self.body = body
            self.joint = self.world:createJoint('mouse', self.anchor, body, {bx = pointer.worldX, by = pointer.worldY, maxMotorForce = force})
            body.awake = true
            return
        end
    end
end

function Grab:release()
    if self.joint ~= nil and self.joint.valid then
        self.joint:destroy()
    end
    self.joint, self.body = nil, nil
end

function Grab:holding()
    return self.joint ~= nil
end

function Grab:draw()
    if self.joint ~= nil and self.body.valid then
        graphics2d.drawLine(self.body.x, self.body.y, self.targetX, self.targetY, 3, '#CCFFFFFF', {layer = 50})
        graphics2d.drawCircle(self.targetX, self.targetY, 6, '#FFFFFFFF', {layer = 50})
    end
end

return Grab
