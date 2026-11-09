-- Gamepad motion: a ball that rolls in a tray with the tilt of a gamepad through the action map, the raw readings of the accelerometer, the gyroscope and the attitude of every gamepad that has them, and the fingers on its touchpads. The arrow keys, the d-pad and the left stick roll the same ball, so a gamepad without sensors still plays. The motion sensors of the device belong to the plugin "motion".
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local Test = require('harness.test')

local GamepadMotion = haylen.class('GamepadMotion', Test)

GamepadMotion.code = [[
input.loadActions({actions = {{name = 'roll', type = 'vector', bindings = {'stick:left', 'stick:tilt'}}}})
local request = input.startGamepadMotion({sensors = {'accelerometer', 'gyroscope'}, owner = self})
local x, y = input.vector('roll')  -- The tilt reads like a stick.
local ax, ay, az = input.gamepadAcceleration(1)
local fingers = input.gamepadTouches(1)]]

local kSensors = {'accelerometer', 'gyroscope', 'attitude'}
-- The ball accelerates by the tilt times this many points per second squared, which is gravity at the scale of the tray, and loses a share of its speed every second and at every bounce.
local kGravity = 2400
local kFriction = 0.6
local kBounce = 0.55
local kRadius = 34

function GamepadMotion:enter()
    self:loadActions({actions = {
        {name = 'roll', type = 'vector', up = {'key:up', 'button:dpadUp'}, down = {'key:down', 'button:dpadDown'}, left = {'key:left', 'button:dpadLeft'}, right = {'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'stick:tilt'}},
    }})
    self.ball = {x = 0, y = 0, speedX = 0, speedY = 0}
    self.wanted = {accelerometer = true, gyroscope = true, attitude = true}
    local toggles = {}
    for index, sensor in ipairs(kSensors) do
        toggles[index] = ui.toggle{id = sensor, text = sensor:sub(1, 1):upper() .. sensor:sub(2), checked = true, onChange = function(event)
            self.wanted[sensor] = event.checked
            self:startSensors()
        end}
    end
    self:frame({
        code = GamepadMotion.code,
        hint = 'Tilt a gamepad with motion sensors, such as the Siri Remote, a DualSense or a Switch Pro controller, to roll the ball, or use the arrows, the d-pad or the left stick. Touch its touchpad to see the fingers.',
        play = true,
        controls = {
            ui.label{text = 'Sensors of gamepads that run', font = 'caption', color = 'textMuted'},
            ui.column{gap = 8, children = toggles},
            ui.label{id = 'sensors', text = 'Starting the sensors.'},
        },
    })
    self:startSensors()
end

-- Starts the sensors that the toggles want and stops what the previous start ran, so the gamepads run only what the test reads.
function GamepadMotion:startSensors()
    if self.request then
        self.request:stop()
        self.request = nil
    end

    local sensors = {}
    for _, sensor in ipairs(kSensors) do
        if self.wanted[sensor] then
            sensors[#sensors + 1] = sensor
        end
    end
    if #sensors == 0 then
        self:set('sensors', {text = 'No sensor runs.'})
        self:log('Every sensor of the gamepads stopped.')
        return
    end
    self.request = input.startGamepadMotion({sensors = sensors, owner = self})
    self:set('sensors', {text = 'Running ' .. table.concat(self.request.sensors, ', ') .. '.'})
    self:log('The gamepads run %s.', table.concat(self.request.sensors, ', '))
end

-- The ball rolls by the action, which the tilt and the sticks both feed, and bounces off the walls of the tray.
function GamepadMotion:update(dt)
    GamepadMotion.super.update(self, dt)
    local tray = self.tray
    if not tray then
        return
    end
    local x, y = input.vector('roll')
    local ball = self.ball
    local keep = math.max(0, 1 - kFriction * dt)
    ball.speedX = (ball.speedX + x * kGravity * dt) * keep
    ball.speedY = (ball.speedY + y * kGravity * dt) * keep
    ball.x = ball.x + ball.speedX * dt
    ball.y = ball.y + ball.speedY * dt
    local halfWidth, halfHeight = tray[3] / 2 - kRadius, tray[4] / 2 - kRadius
    if math.abs(ball.x) > halfWidth then
        ball.x = halfWidth * (ball.x > 0 and 1 or -1)
        ball.speedX = -ball.speedX * kBounce
    end
    if math.abs(ball.y) > halfHeight then
        ball.y = halfHeight * (ball.y > 0 and 1 or -1)
        ball.speedY = -ball.speedY * kBounce
    end

    local gamepad = self:findGamepad()
    if gamepad then
        local tiltX, tiltY = input.gamepadTilt(gamepad)
        self:status(string.format('Gamepad %d tilts %.2f, %.2f with %s.', gamepad, tiltX, tiltY, table.concat(input.gamepadSensors(gamepad), ' and ')))
        return
    end
    self:status('No gamepad with motion sensors is connected, so the keys and the sticks roll the ball.')
end

-- The first connected gamepad that has motion sensors.
function GamepadMotion:findGamepad()
    for index = 1, 8 do
        if #input.gamepadSensors(index) > 0 then
            return index
        end
    end
    return nil
end

-- Draws one line of readings with its title.
function GamepadMotion:drawReading(title, text, x, y, color)
    Test.caption(title, x, y, {size = 20})
    graphics2d.drawText(nil, text, x, y + 26, {size = 24, color = color or Test.ink, layer = 2})
end

function GamepadMotion:drawReadings(x, y)
    local gamepad = self:findGamepad()
    if not gamepad then
        self:drawReading('Gamepads', 'No gamepad with motion sensors is connected.', x, y, Test.muted)
        return
    end

    local ax, ay, az = input.gamepadAcceleration(gamepad)
    local rx, ry, rz = input.gamepadRotationRate(gamepad)
    local qx, qy, qz, qw = input.gamepadAttitude(gamepad)
    local tiltX, tiltY = input.gamepadTilt(gamepad)
    self:drawReading(string.format('Gamepad %d, %s', gamepad, input.gamepadName(gamepad)), table.concat(input.gamepadSensors(gamepad), ', '), x, y, Test.warm)
    self:drawReading('Acceleration with gravity, meters per second squared', string.format('%6.2f  %6.2f  %6.2f', ax, ay, az), x, y + 70)
    self:drawReading('Rotation rate, radians per second', string.format('%6.2f  %6.2f  %6.2f', rx, ry, rz), x, y + 140)
    self:drawReading('Attitude quaternion', string.format('%5.2f  %5.2f  %5.2f  %5.2f', qx, qy, qz, qw), x, y + 210)
    self:drawReading('Tilt', string.format('%5.2f  %5.2f', tiltX, tiltY), x, y + 280, Test.green)

    local touched = {}
    for _, finger in ipairs(input.gamepadTouches(gamepad)) do
        touched[#touched + 1] = string.format('%.2f, %.2f', finger.x, finger.y)
    end
    self:drawReading('Fingers on its touchpads', #touched > 0 and table.concat(touched, '   ') or 'None', x, y + 350, Test.warm)
end

function GamepadMotion:draw(area)
    local margin = 24
    local panel = math.min(460, area.width * 0.42)
    local tray = {margin, margin, area.width - panel - margin * 3, area.height - margin * 2}
    self.tray = tray
    graphics2d.drawShape(tray, {radius = 28, color = Test.surface, borderWidth = 3, borderColor = Test.line})
    local centerX, centerY = tray[1] + tray[3] / 2, tray[2] + tray[4] / 2
    graphics2d.drawLine(centerX - 40, centerY, centerX + 40, centerY, 2, Test.line)
    graphics2d.drawLine(centerX, centerY - 40, centerX, centerY + 40, 2, Test.line)

    local tiltX, tiltY = input.vector('roll')
    graphics2d.drawLine(centerX, centerY, centerX + tiltX * 120, centerY + tiltY * 120, 4, Test.accent, {layer = 2})
    local ball = self.ball
    graphics2d.drawCircle(centerX + ball.x + 4, centerY + ball.y + 8, kRadius, '#55000000', {layer = 3})
    graphics2d.drawCircle(centerX + ball.x, centerY + ball.y, kRadius, Test.warm, {layer = 4})
    graphics2d.drawCircle(centerX + ball.x - 10, centerY + ball.y - 10, kRadius * 0.3, '#66FFFFFF', {layer = 5})

    self:drawReadings(tray[1] + tray[3] + margin, margin)
end

return GamepadMotion
