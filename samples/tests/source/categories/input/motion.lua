-- Motion sensors: a ball that rolls in a tray with the tilt of the device or of a gamepad through the action map, the raw readings of the accelerometer, the gyroscope and the attitude, the touchpad of a gamepad, and the exact reason of the platform for every sensor that does not run. The arrow keys, the d-pad and the left stick roll the same ball, so a device without sensors still plays.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local Test = require('harness.test')

local Motion = haylen.class('Motion', Test)

Motion.code = [[
input.loadActions({actions = {{name = 'roll', type = 'vector', bindings = {'stick:left', 'stick:tilt'}}}})
local motion, failure = input.startMotion({sensors = {'accelerometer'}, gamepads = true, owner = self}):await()
if failure then print(failure.code, failure.message) end  -- 'unsupported', 'denied' or 'failed'.
local x, y = input.vector('roll')  -- The tilt reads like a stick.
local ax, ay, az = input.acceleration()
input.calibrateTilt()  -- The way the player holds the device becomes level.]]

local kSensors = {'accelerometer', 'gyroscope', 'attitude'}
local kRates = {{id = '30', text = '30'}, {id = '60', text = '60'}, {id = '100', text = '100'}}
-- The ball accelerates by the tilt times this many points per second squared, which is gravity at the scale of the tray, and loses a share of its speed every second and at every bounce.
local kGravity = 2400
local kFriction = 0.6
local kBounce = 0.55
local kRadius = 34

function Motion:enter()
    self:loadActions({actions = {
        {name = 'roll', type = 'vector', up = {'key:up', 'button:dpadUp'}, down = {'key:down', 'button:dpadDown'}, left = {'key:left', 'button:dpadLeft'}, right = {'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'stick:tilt'}},
    }})
    self.ball = {x = 0, y = 0, speedX = 0, speedY = 0}
    self.rate = 60
    self.results = {}
    self:frame({
        code = Motion.code,
        hint = 'Tilt the device or a gamepad with sensors to roll the ball, or use the arrows, the d-pad or the left stick. Some browsers ask for the sensors only after a tap on Start the sensors.',
        play = true,
        controls = {
            ui.button{id = 'start', text = 'Start the sensors', onClick = function() self:startSensors() end},
            ui.button{id = 'calibrate', text = 'Take this angle as level', onClick = function()
                self:log('Level %s.', input.calibrateTilt() and 'taken from the accelerometer' or 'unchanged without readings')
            end},
            ui.button{id = 'level', text = 'Level is face up', onClick = function() input.resetTilt() end},
            ui.formField{label = 'Readings per second', ui.segmentedControl{id = 'rate', items = kRates, selected = '60', onChange = function(event)
                self.rate = tonumber(event.value)
                self:startSensors()
            end}},
            ui.label{text = 'What runs', font = 'caption', color = 'textMuted'},
            ui.label{id = 'sensors', text = 'Starting the sensors.'},
        },
    })
    self:startSensors()
end

-- Starts each sensor of the device on its own, so one that the device lacks leaves the others running, and the sensors of gamepads apart from them, so a TV whose remote has sensors runs them, and stops what the previous start ran.
function Motion:startSensors()
    for _, result in pairs(self.results) do
        if result.request then
            result.request:stop()
        end
    end
    if self.gamepads then
        self.gamepads:stop()
        self.gamepads = nil
    end
    self:spawn(function()
        self.gamepads = input.startMotion({sensors = kSensors, rate = self.rate, device = false, gamepads = true, owner = self}):await()
    end)

    self.results = {}
    for _, sensor in ipairs(kSensors) do
        local result = {state = 'starting'}
        self.results[sensor] = result
        self:spawn(function()
            local request, failure = input.startMotion({sensors = {sensor}, rate = self.rate, owner = self}):await()
            if self.results[sensor] ~= result then
                return
            end
            result.request = request
            result.state = failure and failure.code or 'running'
            result.reason = failure and failure.message
            if failure then
                self:log('The %s did not start with "%s". %s', sensor, failure.code, failure.message)
            else
                self:log('The %s runs at %d readings per second.', sensor, self.rate)
            end
            self:describeSensors()
        end)
    end
    self:describeSensors()
end

function Motion:describeSensors()
    local lines = {}
    for _, sensor in ipairs(kSensors) do
        local result = self.results[sensor]
        local name = sensor:sub(1, 1):upper() .. sensor:sub(2)
        lines[#lines + 1] = result.reason and string.format('%s: %s', name, result.reason) or string.format('%s: %s.', name, result.state == 'running' and 'runs' or 'starting')
    end
    self:set('sensors', {text = table.concat(lines, '\n')})
end

-- The ball rolls by the action, which the tilt and the sticks both feed, and bounces off the walls of the tray.
function Motion:update(dt)
    Motion.super.update(self, dt)
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

    local tiltX, tiltY = input.tilt()
    local reporting = input.motionSensors()
    self:status(#reporting > 0 and string.format('The tilt reads %.2f, %.2f from %s.', tiltX, tiltY, table.concat(reporting, ', ')) or 'No sensor of the device delivered readings, so the keys and the sticks roll the ball.')
end

-- Draws one line of readings with its title.
function Motion:drawReading(title, text, x, y, color)
    Test.caption(title, x, y, {size = 20})
    graphics2d.drawText(nil, text, x, y + 26, {size = 24, color = color or Test.ink, layer = 2})
end

function Motion:drawReadings(x, y)
    local ax, ay, az = input.acceleration()
    local rx, ry, rz = input.rotationRate()
    local qx, qy, qz, qw = input.attitude()
    local tiltX, tiltY = input.tilt()
    self:drawReading('Acceleration with gravity, meters per second squared', string.format('%6.2f  %6.2f  %6.2f', ax, ay, az), x, y)
    self:drawReading('Rotation rate, radians per second', string.format('%6.2f  %6.2f  %6.2f', rx, ry, rz), x, y + 70)
    self:drawReading('Attitude quaternion', string.format('%5.2f  %5.2f  %5.2f  %5.2f', qx, qy, qz, qw), x, y + 140)
    self:drawReading('Tilt of the device', string.format('%5.2f  %5.2f', tiltX, tiltY), x, y + 210, Test.green)

    -- The first gamepad with sensors shows its tilt and the fingers on its touchpad.
    for index = 1, 8 do
        if #input.gamepadSensors(index) > 0 then
            local padX, padY = input.gamepadTilt(index)
            self:drawReading(string.format('Tilt of gamepad %d, %s', index, table.concat(input.gamepadSensors(index), ' and ')), string.format('%5.2f  %5.2f', padX, padY), x, y + 280, Test.warm)
            local fingers = input.gamepadTouches(index)
            local touched = {}
            for _, finger in ipairs(fingers) do
                touched[#touched + 1] = string.format('%.2f, %.2f', finger.x, finger.y)
            end
            self:drawReading('Fingers on its touchpads', #touched > 0 and table.concat(touched, '   ') or 'None', x, y + 350, Test.warm)
            return
        end
    end
    self:drawReading('Gamepads', 'No gamepad with motion sensors is connected.', x, y + 280, Test.muted)
end

function Motion:draw(area)
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

return Motion
