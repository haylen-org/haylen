-- Drags the dynamic body under the pointer with a mouse joint whose target follows the pointer.
local graphics2d = require('haylen.graphics2d')

local Grab = {}
Grab.__index = Grab

-- The optional filter of world:pick leaves out bodies that should not be dragged.
function Grab.new(world, filter)
    return setmetatable({world = world, filter = filter, anchor = world:createBody({type = 'static'}), joint = nil, body = nil}, Grab)
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
            self.body = body
            self.joint = self.world:createJoint('mouse', self.anchor, body, {bx = pointer.worldX, by = pointer.worldY})
            body.awake = true
            return
        end
    end
end

function Grab:release()
    if self.joint ~= nil then
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
