-- Gamepads: a live card for each of the four gamepads with its sticks, triggers, shoulders, d-pad and face buttons, the connect and disconnect events, and the dead zone that every stick and axis shares.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')
local window = require('haylen.window')

local Journal = require('journal')
local sample = require('sample')

local Gamepads = haylen.class('Gamepads', sample.Test)

local kButtons = {'south', 'east', 'west', 'north', 'leftShoulder', 'rightShoulder', 'leftStick', 'rightStick', 'back', 'start', 'guide', 'dpadUp', 'dpadDown', 'dpadLeft', 'dpadRight'}
local kFace = {north = {0, -1}, south = {0, 1}, west = {-1, 0}, east = {1, 0}}
local kPad = {dpadUp = {0, -1}, dpadDown = {0, 1}, dpadLeft = {-1, 0}, dpadRight = {1, 0}}
local kFlash = 0.3
local kHoldToLeave = 1
local kIdle = '#FF3A4258'

function Gamepads:enter()
    self.defaultDeadzone = input.gamepadDeadzone()
    self.flashes = {}
    for index = 1, 4 do
        self.flashes[index] = {}
    end
    self.hold = 0
    self.journal = Journal(24)
    for index = 1, 4 do
        if input.gamepadConnected(index) then
            self.journal:add('already connected ' .. index .. ': ' .. input.gamepadName(index), sample.muted)
        end
    end
    self:listen('gamepadConnected', function(pad) self.journal:add('gamepadConnected ' .. pad.gamepad .. ': ' .. pad.name, sample.green) end)
    self:listen('gamepadDisconnected', function(pad) self.journal:add('gamepadDisconnected ' .. pad.gamepad .. ': ' .. pad.name, sample.red) end)
    self:frame({
        hint = 'Plug in up to four gamepads. A browser shows a gamepad once one of its buttons is pressed.',
        navigation = not window.hasPointerDevice(),
        back = 'Escape or Back returns to the menu, and so does holding the east button for a second, so a tap on it shows on the card.',
        focus = 'deadzone',
        controls = {
            ui.formField{label = 'Dead zone of every stick and axis', ui.slider{id = 'deadzone', min = 0, max = 0.9, step = 0.05, value = self.defaultDeadzone, showValue = true, onChange = function(event)
                input.setGamepadDeadzone(event.value)
            end}},
            ui.label{text = 'Movement inside the dead zone reads 0 and does not make the gamepad the last device. The rest is rescaled to the full range, and sticks apply it to their radius, which keeps diagonals smooth.', color = 'textMuted', font = 'caption'},
            ui.sectionTitle{text = 'Reading gamepads'},
            ui.label{font = 'monospace', text = "input.gamepadConnected(2)\ninput.gamepadName(2)\ninput.gamepadDown('south', 2)\ninput.gamepadAxis('leftTrigger', 2)\ninput.gamepadStick('left', 2)\ninput.setGamepadDeadzone(0.2)"},
        },
    })
end

function Gamepads:exit()
    input.setGamepadDeadzone(self.defaultDeadzone)
end

-- The east button is the cancel of the UI, so a tap on it shows on the card and holding it for a second goes back.
function Gamepads:eastDown()
    for index = 1, 4 do
        if input.gamepadConnected(index) and input.gamepadDown('east', index) then
            return true
        end
    end
    return false
end

function Gamepads:cancel()
    if not self:eastDown() then
        sample.back()
    end
end

function Gamepads:update(dt)
    Gamepads.super.update(self, dt)
    local connected = 0
    for index = 1, 4 do
        if input.gamepadConnected(index) then
            connected = connected + 1
            local flashes = self.flashes[index]
            for _, button in ipairs(kButtons) do
                flashes[button] = input.gamepadPressed(button, index) and kFlash or math.max(0, (flashes[button] or 0) - dt)
            end
        end
    end

    self.hold = self:eastDown() and self.hold + dt or 0
    if self.hold >= kHoldToLeave then
        self.hold = 0
        sample.back()
    end
    self:status(string.format('connected %d   dead zone %.2f   last device %s', connected, input.gamepadDeadzone(), input.lastDevice()))
end

function Gamepads:drawStick(index, side, x, y, radius)
    local sx, sy = input.gamepadStick(side, index)
    local clicked = input.gamepadDown(side .. 'Stick', index)
    graphics2d.drawCircle(x, y, radius, sample.surface, {layer = 1})
    graphics2d.drawRing(x, y, radius * input.gamepadDeadzone(), 2, sample.muted, {layer = 2})
    graphics2d.drawRing(x, y, radius, 3, clicked and sample.warm or sample.line, {layer = 2})
    graphics2d.drawLine(x, y, x + sx * radius, y + sy * radius, 3, sample.accent, {layer = 3})
    graphics2d.drawCircle(x + sx * radius, y + sy * radius, radius * 0.28, clicked and sample.warm or sample.accent, {layer = 3})
    sample.caption(string.format('%s %+.2f %+.2f', side, sx, sy), x, y + radius + 8, {anchor = {0.5, 0}, size = 16})
end

function Gamepads:drawButton(index, button, x, y, radius)
    local color = input.gamepadDown(button, index) and sample.accent or kIdle
    graphics2d.drawCircle(x, y, radius, color, {layer = 2})
    if (self.flashes[index][button] or 0) > 0 then
        graphics2d.drawRing(x, y, radius + 4, 3, sample.warm, {layer = 3})
    end
end

function Gamepads:drawTrigger(index, axis, x, top, height)
    local value = input.gamepadAxis(axis, index)
    graphics2d.drawRect({x, top, 20, height}, kIdle, {layer = 1})
    graphics2d.drawRect({x, top + height * (1 - value), 20, height * value}, sample.green, {layer = 2})
end

function Gamepads:drawPad(index, rect)
    local x, y, w, h = rect[1], rect[2], rect[3], rect[4]
    local connected = input.gamepadConnected(index)
    graphics2d.drawRect(rect, connected and '#FF1F2433' or '#FF1A1E28')
    graphics2d.drawRectOutline(rect, 2, connected and sample.line or '#FF22273A', {layer = 1})
    sample.caption(index .. '  ' .. (connected and input.gamepadName(index) or 'Not connected'), x + 16, y + 12, {color = connected and sample.ink or sample.muted})
    if not connected then
        return
    end

    local r = math.min(w, h) * 0.12
    self:drawTrigger(index, 'leftTrigger', x + 16, y + h * 0.22, h * 0.66)
    self:drawTrigger(index, 'rightTrigger', x + w - 36, y + h * 0.22, h * 0.66)
    for side, left in pairs({leftShoulder = x + w * 0.1, rightShoulder = x + w * 0.66}) do
        graphics2d.drawRect({left, y + h * 0.14, w * 0.24, 14}, input.gamepadDown(side, index) and sample.accent or kIdle, {layer = 2})
    end
    self:drawStick(index, 'left', x + w * 0.22, y + h * 0.45, r)
    self:drawStick(index, 'right', x + w * 0.64, y + h * 0.76, r)
    for button, offset in pairs(kPad) do
        self:drawButton(index, button, x + w * 0.36 + offset[1] * r * 0.62, y + h * 0.76 + offset[2] * r * 0.62, r * 0.3)
    end
    for button, offset in pairs(kFace) do
        self:drawButton(index, button, x + w * 0.8 + offset[1] * r * 0.8, y + h * 0.45 + offset[2] * r * 0.8, r * 0.34)
    end
    self:drawButton(index, 'back', x + w * 0.42, y + h * 0.42, r * 0.2)
    self:drawButton(index, 'guide', x + w * 0.5, y + h * 0.32, r * 0.24)
    self:drawButton(index, 'start', x + w * 0.58, y + h * 0.42, r * 0.2)
end

function Gamepads:draw(area)
    local width = area.width * 0.72
    local w, h = (width - 36) / 2, (area.height - 36) / 2
    for index = 1, 4 do
        local column, row = (index - 1) % 2, (index - 1) // 2
        self:drawPad(index, {12 + column * (w + 12), 12 + row * (h + 12), w, h})
    end

    local logLeft = width + 20
    sample.caption('Connections', logLeft, 20)
    self.journal:draw(logLeft, 56, area.height - 140, 17)
    if self.hold > 0 then
        local x, y = logLeft + 60, area.height - 60
        graphics2d.drawArc(x, y, 34, 8, -math.pi / 2, -math.pi / 2 + math.pi * 2 * self.hold / kHoldToLeave, sample.warm, {layer = 3})
        sample.caption('Leaving', x + 50, y, {anchor = {0, 0.5}, color = sample.warm})
    end
end

return Gamepads
