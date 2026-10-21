-- Gamepads: a live card for each of the eight gamepads with its sticks, triggers, shoulders, d-pad and face buttons labeled as the gamepad labels them, the connect and disconnect events, rumbles of both motors, and the dead zone that every stick and axis shares.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local Journal = require('harness.journal')
local Test = require('harness.test')
local prompts = require('categories.input.prompts')

local Gamepads = haylen.class('Gamepads', Test)

Gamepads.code = [[
local name, labels = input.gamepadName(2), input.gamepadLabels(2)
local x, y = input.gamepadStick('left', 2)
local jump = input.gamepadPressed('south', 2)
local brake = input.gamepadAxis('leftTrigger', 2)
input.rumble(2, {low = 0.7, high = 0.7, seconds = 0.4})
input.setGamepadDeadzone(0.2)]]

local kButtons = {'south', 'east', 'west', 'north', 'leftShoulder', 'rightShoulder', 'leftStick', 'rightStick', 'back', 'start', 'guide', 'dpadUp', 'dpadDown', 'dpadLeft', 'dpadRight'}
local kFace = {north = {0, -1}, south = {0, 1}, west = {-1, 0}, east = {1, 0}}
local kPad = {dpadUp = {0, -1}, dpadDown = {0, 1}, dpadLeft = {-1, 0}, dpadRight = {1, 0}}
local kFlash = 0.3
local kHoldToLeave = 1
local kIdle = '#FF3A4258'
local kCard = '#FF1F2433'
local kGroove = '#FF15172A'
local kPads = 8
local kLabels = {
    letters = {south = 'A', east = 'B', west = 'X', north = 'Y'},
    swappedLetters = {south = 'B', east = 'A', west = 'Y', north = 'X'},
    shapes = {south = 'Cross', east = 'Circle', west = 'Square', north = 'Triangle'},
}
local kRumbles = {{id = 'low', text = 'Low motor', low = 1, high = 0}, {id = 'high', text = 'High motor', low = 0, high = 1}, {id = 'both', text = 'Both motors', low = 0.7, high = 0.7}}

function Gamepads:enter()
    self.defaultDeadzone = input.gamepadDeadzone()
    self.flashes = {}
    for index = 1, kPads do
        self.flashes[index] = {}
    end
    self.hold = 0
    self.journal = Journal(24)
    for index = 1, kPads do
        if input.gamepadConnected(index) then
            self.journal:add('Gamepad ' .. index .. ' was connected already: ' .. input.gamepadName(index) .. '.', Test.muted)
        end
    end
    self.silhouette = assets.texture('input/gamepad.png', {filter = 'linear'})
    self:listen('gamepadConnected', function(pad) self.journal:add('Gamepad ' .. pad.gamepad .. ' connects: ' .. pad.name .. '.', Test.green) end)
    self:listen('gamepadDisconnected', function(pad) self.journal:add('Gamepad ' .. pad.gamepad .. ' disconnects: ' .. pad.name .. '.', Test.red) end)
    self:frame({
        code = Gamepads.code,
        hint = 'Plug in up to eight gamepads. A browser shows a gamepad once one of its buttons is pressed. A tap on the B button shows on the card, and holding it for a second goes back.',
        play = true,
        controls = {
            ui.formField{label = 'Dead zone of every stick and axis', ui.slider{id = 'deadzone', min = 0, max = 0.9, step = 0.05, value = self.defaultDeadzone, showValue = true, onChange = function(event)
                input.setGamepadDeadzone(event.value)
            end}},
            ui.label{text = 'Movement inside the dead zone reads 0 and does not make the gamepad the last device. The rest is rescaled to the full range, and sticks apply it to their radius, which keeps diagonals smooth.', color = 'textMuted', font = 'caption'},
            ui.label{text = 'Rumble every connected gamepad', font = 'caption', color = 'textMuted'},
            ui.column{gap = 8, alignItems = 'stretch', children = self:rumbleButtons()},
        },
    })
end

function Gamepads:rumbleButtons()
    local buttons = {}
    for _, rumble in ipairs(kRumbles) do
        buttons[#buttons + 1] = ui.button{id = rumble.id, text = rumble.text, onClick = function()
            for index = 1, kPads do
                if input.gamepadConnected(index) then
                    input.rumble(index, {low = rumble.low, high = rumble.high, seconds = 0.4})
                end
            end
            self.journal:add('The ' .. rumble.text:lower() .. ' rumbles for 0.4 seconds.', Test.accent)
        end}
    end
    return buttons
end

function Gamepads:exit()
    Gamepads.super.exit(self)
    input.setGamepadDeadzone(self.defaultDeadzone)
end

-- The east button is the cancel of the UI, so a tap on it shows on the card and holding it for a second goes back.
function Gamepads:eastDown()
    for index = 1, kPads do
        if input.gamepadConnected(index) and input.gamepadDown('east', index) then
            return true
        end
    end
    return false
end

function Gamepads:cancel()
    if not self:eastDown() then
        Gamepads.super.cancel(self)
    end
end

function Gamepads:update(dt)
    Gamepads.super.update(self, dt)
    local connected = 0
    for index = 1, kPads do
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
        Gamepads.super.cancel(self)
    end
    self:status(string.format('%d of 8 gamepads connected, a dead zone of %.2f, and the last device is "%s".', connected, input.gamepadDeadzone(), input.lastDevice()))
end

function Gamepads:drawStick(index, side, x, y, radius)
    local sx, sy = input.gamepadStick(side, index)
    local clicked = input.gamepadDown(side .. 'Stick', index)
    graphics2d.drawShape({x - radius, y - radius, radius * 2, radius * 2}, {kind = 'ellipse', color = kGroove, borderWidth = 3, borderColor = clicked and Test.warm or Test.line, layer = 1})
    local zone = radius * input.gamepadDeadzone()
    if zone > 1 then
        graphics2d.drawShape({x - zone, y - zone, zone * 2, zone * 2}, {kind = 'ellipse', color = '#00000000', borderWidth = 2, borderColor = Test.muted, layer = 2})
    end
    local knob = radius * 0.42
    graphics2d.drawShape({x + sx * radius * 0.6 - knob, y + sy * radius * 0.6 - knob, knob * 2, knob * 2}, {kind = 'ellipse', color = clicked and Test.warm or Test.accent, borderWidth = 2, borderColor = prompts.ink, layer = 3})
end

function Gamepads:drawButton(index, button, x, y, radius)
    local face = prompts.faceColors[button]
    local color = input.gamepadDown(button, index) and (face or Test.accent) or kIdle
    graphics2d.drawShape({x - radius, y - radius, radius * 2, radius * 2}, {kind = 'ellipse', color = color, borderWidth = 2, borderColor = face or prompts.ink, layer = 2})
    local labels = prompts.faces[input.gamepadLabels(index)]
    if labels and labels[button] and radius >= 10 then
        graphics2d.drawText(prompts.font(), labels[button]:sub(1, 1), x, y, {size = radius * 1.1, anchor = {0.5, 0.5}, color = '#FFFFFFFF', outlineWidth = 2, outlineColor = prompts.ink, layer = 3})
    end
    if (self.flashes[index][button] or 0) > 0 then
        graphics2d.drawShape({x - radius - 5, y - radius - 5, radius * 2 + 10, radius * 2 + 10}, {kind = 'ellipse', color = '#00000000', borderWidth = 3, borderColor = Test.warm, layer = 3})
    end
end

-- A trigger is a groove with round ends that fills from the bottom as it is pulled.
function Gamepads:drawTrigger(index, axis, x, top, height)
    local value = input.gamepadAxis(axis, index)
    local width = 18
    graphics2d.drawShape({x, top, width, height}, {radius = width / 2, color = kGroove, layer = 1})
    if value > 0.01 then
        local fill = math.max(width - 6, (height - 6) * value)
        graphics2d.drawShape({x + 3, top + height - 3 - fill, width - 6, fill}, {radius = (width - 6) / 2, color = Test.green, layer = 2})
    end
end

function Gamepads:drawPad(index, rect)
    local x, y, w, h = rect[1], rect[2], rect[3], rect[4]
    local connected = input.gamepadConnected(index)
    graphics2d.drawShape(rect, {radius = 18, color = connected and kCard or '#FF1A1E28', borderWidth = 2, borderColor = connected and Test.line or '#FF22273A'})
    graphics2d.drawText(prompts.font(), 'Gamepad ' .. index, x + 18, y + 24, {size = 24, anchor = {0, 0.5}, color = connected and '#FFFFFFFF' or Test.muted, layer = 2})
    if not connected then
        local size = math.min(w * 0.62, (h - 100) * self.silhouette.width / self.silhouette.height)
        graphics2d.draw(self.silhouette, x + w / 2, y + h * 0.5, {width = size, height = size * self.silhouette.height / self.silhouette.width, color = '#30FFFFFF', layer = 1})
        graphics2d.drawText(nil, 'Not connected', x + w / 2, y + h - 30, {size = 20, color = Test.muted, anchor = {0.5, 0.5}, layer = 2})
        return
    end
    graphics2d.drawText(nil, input.gamepadName(index), x + 18, y + 52, {size = 20, color = Test.muted, anchor = {0, 0.5}, maxWidth = w - 36, wrap = false, minScale = 0.7, layer = 2})

    local r = math.min(w, h) * 0.12
    local top = y + h * 0.3
    self:drawTrigger(index, 'leftTrigger', x + 16, top, h * 0.62)
    self:drawTrigger(index, 'rightTrigger', x + w - 34, top, h * 0.62)
    for side, left in pairs({leftShoulder = x + w * 0.12, rightShoulder = x + w * 0.64}) do
        graphics2d.drawShape({left, top, w * 0.24, 14}, {radius = 7, color = input.gamepadDown(side, index) and Test.accent or kIdle, layer = 2})
    end
    self:drawStick(index, 'left', x + w * 0.26, y + h * 0.55, r)
    self:drawStick(index, 'right', x + w * 0.62, y + h * 0.8, r)
    for button, offset in pairs(kPad) do
        self:drawButton(index, button, x + w * 0.38 + offset[1] * r * 0.62, y + h * 0.8 + offset[2] * r * 0.62, r * 0.3)
    end
    for button, offset in pairs(kFace) do
        self:drawButton(index, button, x + w * 0.76 + offset[1] * r * 0.85, y + h * 0.55 + offset[2] * r * 0.85, r * 0.36)
    end
    self:drawButton(index, 'back', x + w * 0.43, y + h * 0.5, r * 0.2)
    self:drawButton(index, 'guide', x + w * 0.51, y + h * 0.42, r * 0.24)
    self:drawButton(index, 'start', x + w * 0.59, y + h * 0.5, r * 0.2)
end

function Gamepads:draw(area)
    local margin, gap = 24, 16
    local logWidth = math.max(300, area.width * 0.24)
    local width = area.width - margin * 2 - gap - logWidth
    local w, h = (width - gap * 3) / 4, (area.height - margin * 2 - gap) / 2
    for index = 1, kPads do
        local column, row = (index - 1) % 4, (index - 1) // 4
        self:drawPad(index, {margin + column * (w + gap), margin + row * (h + gap), w, h})
    end

    local logLeft = margin + width + gap
    graphics2d.drawShape({logLeft, margin, logWidth, area.height - margin * 2}, {radius = 18, color = Test.surface, borderWidth = 2, borderColor = Test.line})
    graphics2d.drawText(prompts.font(), 'Connections', logLeft + 20, margin + 30, {size = 24, anchor = {0, 0.5}, layer = 2})
    self.journal:draw(logLeft + 4, margin + 60, area.height - margin * 2 - 150, 19, logWidth - 24)
    if self.hold > 0 then
        local cx, cy = logLeft + 54, area.height - margin - 50
        graphics2d.drawArc(cx, cy, 26, 7, -math.pi / 2, -math.pi / 2 + math.pi * 2 * self.hold / kHoldToLeave, Test.warm, {layer = 3})
        graphics2d.drawText(nil, 'Leaving', cx + 44, cy, {size = 22, color = Test.warm, anchor = {0, 0.5}, layer = 3})
    end
end

return Gamepads
