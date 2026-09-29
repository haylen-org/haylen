-- One pointer for every device: the mouse, the first finger on the screen and a cursor that the right stick of a gamepad moves and the right trigger presses.
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local ui = require('haylen.ui')

local Pointer = {}
Pointer.__index = Pointer

local kCursorSpeed = 1100

function Pointer.new()
    local x, y = input.mousePosition()
    return setmetatable({x = x, y = y, worldX = 0, worldY = 0, down = false, pressed = false, released = false, source = nil, finger = nil}, Pointer)
end

-- Presses only start inside the area, and a press keeps going wherever it moves.
function Pointer:update(dt, area, camera)
    local wasDown = self.down
    self:follow(dt)

    if self.source == nil then
        self:start(area)
    elseif not self:held() then
        self.source = nil
        self.finger = nil
    end

    self.down = self.source ~= nil
    self.pressed = self.down and not wasDown
    self.released = wasDown and not self.down
    self.worldX, self.worldY = camera:screenToWorld(self.x, self.y)
end

function Pointer:follow(dt)
    if self.finger ~= nil then
        local touch = input.findTouch(self.finger)
        if touch ~= nil then
            self.x, self.y = touch.x, touch.y
        end
        return
    end

    local first = input.touches()[1]
    if first ~= nil then
        self.x, self.y = first.x, first.y
        return
    end

    local dx, dy = input.mouseDelta()
    if dx ~= 0 or dy ~= 0 or input.mousePressed('left') then
        self.x, self.y = input.mousePosition()
    end

    local cx, cy = input.vector('cursor')
    self.x = self.x + cx * kCursorSpeed * dt
    self.y = self.y + cy * kCursorSpeed * dt
end

-- A finger lands where it touches, while the mouse and the gamepad cursor hover first, so the interface they hover keeps their presses.
function Pointer:start(area)
    local first = input.touches()[1]
    if first ~= nil and first.phase == 'began' then
        if area:contains({first.x, first.y}) then
            self.source, self.finger = 'touch', first.id
        end
    elseif area:contains({self.x, self.y}) and not ui.usingPointer() then
        if input.mousePressed('left') then
            self.source = 'mouse'
        elseif input.pressed('press') then
            self.source = 'gamepad'
        end
    end
end

function Pointer:held()
    if self.source == 'touch' then
        local touch = input.findTouch(self.finger)
        return touch ~= nil and touch.phase ~= 'ended' and touch.phase ~= 'cancelled'
    elseif self.source == 'mouse' then
        return input.mouseDown('left')
    end
    return input.down('press')
end

-- Shows the gamepad cursor, which has no system cursor of its own.
function Pointer:draw()
    if input.lastDevice() == 'gamepad' then
        graphics2d.drawRing(self.x, self.y, 18, 4, self.down and '#FFFFD54F' or '#FFFFFFFF', {layer = 100})
        graphics2d.drawCircle(self.x, self.y, 4, '#FFFFFFFF', {layer = 100})
    end
end

return Pointer
