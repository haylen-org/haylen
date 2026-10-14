-- Gestures: taps, double taps, long presses, swipes and pinches marked where they happen, a card that two fingers zoom, turn and move and that a double tap resets, a count of each gesture and the thresholds of the recognizer.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local Journal = require('harness.journal')
local Test = require('harness.test')

local Gestures = haylen.class('Gestures', Test)

local kTypes = {'tap', 'doubleTap', 'longPress', 'swipe', 'pinch'}
local kColors = {tap = Test.accent, doubleTap = Test.warm, longPress = Test.violet, swipe = Test.green, pinch = Test.red}
local kLife = 0.9
local kSettings = {
    {key = 'longPressDuration', label = 'Long press seconds', min = 0.2, max = 1.5, step = 0.05},
    {key = 'doubleTapInterval', label = 'Double tap seconds', min = 0.15, max = 0.8, step = 0.05},
    {key = 'swipeMinDistance', label = 'Shortest swipe', min = 30, max = 300, step = 10, decimals = 0},
    {key = 'tapMaxMovement', label = 'Farthest a tap moves', min = 8, max = 80, step = 2, decimals = 0},
}

function Gestures:enter()
    self.defaults = input.gestureSettings()
    self.markers = {}
    self.counts = {}
    for _, name in ipairs(kTypes) do
        self.counts[name] = 0
    end
    self.scale, self.baseScale = 1, 1
    self.angle, self.baseAngle = 0, 0
    self.offsetX, self.offsetY, self.baseX, self.baseY = 0, 0, 0, 0
    self.journal = Journal(30)

    local controls = {}
    for _, setting in ipairs(kSettings) do
        controls[#controls + 1] = ui.formField{label = setting.label, ui.slider{id = setting.key, min = setting.min, max = setting.max, step = setting.step, decimals = setting.decimals, value = self.defaults[setting.key], showValue = true, onChange = function(event)
            input.setGestureSettings({[setting.key] = event.value})
        end}}
    end
    controls[#controls + 1] = ui.toggle{id = 'mouse', text = 'The mouse acts as a finger', checked = self.defaults.mouse, onChange = function(event)
        input.setGestureSettings({mouse = event.checked})
    end}
    controls[#controls + 1] = ui.button{id = 'reset', text = 'Reset the thresholds', onClick = function() self:reset() end}
    self:frame({hint = 'Tap, double tap, hold, swipe or pinch the stage. The left mouse button acts as a finger.', controls = controls, focus = 'longPressDuration'})
end

function Gestures:exit()
    Gestures.super.exit(self)
    input.setGestureSettings(self.defaults)
end

function Gestures:reset()
    input.setGestureSettings(self.defaults)
    for _, setting in ipairs(kSettings) do
        self:set(setting.key, {value = self.defaults[setting.key]})
    end
    self:set('mouse', {checked = self.defaults.mouse})
end

function Gestures:describe(gesture)
    if gesture.type == 'swipe' then
        return string.format('Swipe %+.0f, %+.0f', gesture.dx, gesture.dy)
    elseif gesture.type == 'pinch' then
        return string.format('Pinch scale %.2f, turn %.0f degrees, move %+.0f, %+.0f', gesture.scale, math.deg(gesture.rotation), gesture.dx, gesture.dy)
    end
    return string.format('Gesture "%s" at %.0f, %.0f', gesture.type, gesture.x, gesture.y)
end

function Gestures:update(dt)
    Gestures.super.update(self, dt)
    if not self.area then
        return
    end
    for index = #self.markers, 1, -1 do
        local marker = self.markers[index]
        marker.age = marker.age + dt
        if marker.age > kLife then
            table.remove(self.markers, index)
        end
    end

    -- Gestures over the panel come from its controls, so only gestures on the stage count.
    for _, gesture in ipairs(input.gestures()) do
        local x, y = self:toStage(gesture.x, gesture.y)
        if x >= 0 and y >= 0 and x <= self.area.width and y <= self.area.height then
            self.counts[gesture.type] = self.counts[gesture.type] + 1
            if gesture.type == 'pinch' then
                self.scale = math.max(0.3, math.min(4, self.baseScale * gesture.scale))
                self.angle = self.baseAngle + gesture.rotation
                self.offsetX, self.offsetY = self.baseX + gesture.dx, self.baseY + gesture.dy
            else
                self.markers[#self.markers + 1] = {type = gesture.type, x = x, y = y, dx = gesture.dx, dy = gesture.dy, age = 0}
                self.journal:add(self:describe(gesture), kColors[gesture.type])
            end
            if gesture.type == 'doubleTap' then
                self.scale, self.angle, self.offsetX, self.offsetY = 1, 0, 0, 0
            end
        end
    end
    if #input.touches() < 2 then
        self.baseScale, self.baseAngle, self.baseX, self.baseY = self.scale, self.angle, self.offsetX, self.offsetY
    end

    local counts = {}
    for _, name in ipairs(kTypes) do
        counts[#counts + 1] = '"' .. name .. '" ' .. self.counts[name]
    end
    self:status('Gestures ' .. table.concat(counts, ', ') .. string.format(', card scale %.2f, turn %.0f degrees', self.scale, math.deg(self.angle)))
end

function Gestures:drawMarker(marker)
    local t = marker.age / kLife
    local color = kColors[marker.type]
    if marker.type == 'swipe' then
        local fromX, fromY = marker.x - marker.dx, marker.y - marker.dy
        graphics2d.drawLine(fromX, fromY, marker.x, marker.y, 8, color, {layer = 3})
        graphics2d.drawCircle(marker.x, marker.y, 14, color, {layer = 3})
    elseif marker.type == 'longPress' then
        graphics2d.drawCircle(marker.x, marker.y, 30 + 30 * t, '#80C9A0FF', {layer = 3})
    else
        graphics2d.drawRing(marker.x, marker.y, 20 + 70 * t, 6, color, {layer = 3})
        if marker.type == 'doubleTap' then
            graphics2d.drawRing(marker.x, marker.y, 10 + 40 * t, 4, color, {layer = 3})
        end
    end
    Test.caption('Gesture "' .. marker.type .. '"', marker.x, marker.y + 40, {anchor = {0.5, 0}, color = color})
end

function Gestures:draw(area)
    local cx, cy = area.width * 0.38 + self.offsetX, area.height * 0.5 + self.offsetY
    local half = 150 * self.scale
    graphics2d.drawShape({cx - half, cy - half * 0.66, half * 2, half * 1.32}, {radius = 12, rotation = self.angle, color = Test.surface, borderWidth = 3, borderColor = Test.line})
    Test.caption('Pinch to zoom, turn and move, double tap to reset', cx, cy, {anchor = {0.5, 0.5}, color = Test.ink, size = 17 * math.max(0.6, self.scale)})

    for _, marker in ipairs(self.markers) do
        self:drawMarker(marker)
    end

    local logLeft = area.width * 0.72
    Test.caption('Recognized gestures', logLeft, 20)
    self.journal:draw(logLeft, 56, area.height - 80, 19)
end

return Gestures
