-- One pointer over the play area of a test for every device: the mouse, the first finger on the screen and a cursor that the `cursor` action moves and the `press` action presses, the right stick and the right trigger in `Pointer.actions`. A test adds those actions to its action map, updates the pointer after the frame follows the play area and draws it from `renderUi`. With `confined = true` the pointer starts in the middle of the play area and never leaves it, and with `shown = true` it draws its ring for every device instead of only for gamepads.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local ui = require('haylen.ui')

local Pointer = haylen.class('Pointer')

Pointer.cursorSpeed = 1100
Pointer.actions = {
    {name = 'cursor', type = 'vector', bindings = {'stick:right'}},
    {name = 'press', type = 'button', bindings = {'axis:rightTrigger+'}},
}

function Pointer:init(options)
    options = options or {}
    self.confined = options.confined
    self.shown = options.shown
    self.x, self.y = input.mousePosition()
    self.worldX, self.worldY = 0, 0
    self.down, self.pressed, self.released = false, false, false
end

-- Follows the devices over the play area of `test`. Presses only start inside the play area, and a press keeps going wherever it moves.
function Pointer:update(dt, test)
    local stage = test.stage
    if not stage then
        return
    end
    local wasDown = self.down
    if self.confined and not self.started then
        self.started = true
        self.x, self.y = stage.x + stage.width / 2, stage.y + stage.height / 2
    end
    self:follow(dt)
    if self.confined then
        self.x, self.y = m.clamp(self.x, stage.x, stage:right()), m.clamp(self.y, stage.y, stage:bottom())
    end

    if self.source == nil then
        self:start(test.stage)
    elseif not self:held() then
        self.source, self.finger = nil, nil
    end

    self.down = self.source ~= nil
    self.pressed = self.down and not wasDown
    self.released = wasDown and not self.down
    self.worldX, self.worldY = test.camera:screenToWorld(self.x, self.y)
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
    self.x = self.x + cx * Pointer.cursorSpeed * dt
    self.y = self.y + cy * Pointer.cursorSpeed * dt
end

-- A finger lands where it touches, while the mouse and the gamepad cursor hover first, so the controls they hover keep their presses.
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

-- Shows the gamepad cursor, which has no system cursor of its own, or the pointer of every device when it is shown.
function Pointer:draw()
    if self.shown or input.lastDevice() == 'gamepad' then
        graphics2d.beginScreen()
        graphics2d.drawRing(self.x, self.y, 18, 4, self.down and '#FFFFD54F' or '#FFFFFFFF', {layer = 100})
        graphics2d.drawCircle(self.x, self.y, 4, '#FFFFFFFF', {layer = 100})
    end
end

return Pointer
