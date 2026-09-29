-- Gestures: taps, double taps, long presses, swipes and pinches marked where they happen, a card that pinches to zoom and resets on a double tap, a count of each gesture and the thresholds of the recognizer.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local Journal = require('journal')
local sample = require('sample')

local Gestures = haylen.class('Gestures', sample.Test)

local kTypes = {'tap', 'double_tap', 'long_press', 'swipe', 'pinch'}
local kColors = {tap = sample.accent, double_tap = sample.warm, long_press = sample.violet, swipe = sample.green, pinch = sample.red}
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
        return string.format('swipe %+.0f, %+.0f', gesture.dx, gesture.dy)
    elseif gesture.type == 'pinch' then
        return string.format('pinch scale %.2f', gesture.scale)
    end
    return string.format('%s at %.0f, %.0f', gesture.type, gesture.x, gesture.y)
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
            else
                self.markers[#self.markers + 1] = {type = gesture.type, x = x, y = y, dx = gesture.dx, dy = gesture.dy, age = 0}
                self.journal:add(self:describe(gesture), kColors[gesture.type])
            end
            if gesture.type == 'double_tap' then
                self.scale = 1
            end
        end
    end
    if #input.touches() < 2 then
        self.baseScale = self.scale
    end

    local counts = {}
    for _, name in ipairs(kTypes) do
        counts[#counts + 1] = name .. ' ' .. self.counts[name]
    end
    self:status(table.concat(counts, '   ') .. string.format('   card scale %.2f', self.scale))
end

function Gestures:drawMarker(marker)
    local t = marker.age / kLife
    local color = kColors[marker.type]
    if marker.type == 'swipe' then
        local fromX, fromY = marker.x - marker.dx, marker.y - marker.dy
        graphics2d.drawLine(fromX, fromY, marker.x, marker.y, 8, color, {layer = 3})
        graphics2d.drawCircle(marker.x, marker.y, 14, color, {layer = 3})
    elseif marker.type == 'long_press' then
        graphics2d.drawCircle(marker.x, marker.y, 30 + 30 * t, '#80C9A0FF', {layer = 3})
    else
        graphics2d.drawRing(marker.x, marker.y, 20 + 70 * t, 6, color, {layer = 3})
        if marker.type == 'double_tap' then
            graphics2d.drawRing(marker.x, marker.y, 10 + 40 * t, 4, color, {layer = 3})
        end
    end
    sample.caption(marker.type, marker.x, marker.y + 40, {anchor = {0.5, 0}, color = color})
end

function Gestures:draw(area)
    local cx, cy = area.width * 0.38, area.height * 0.5
    local half = 150 * self.scale
    graphics2d.drawRect({cx - half, cy - half * 0.66, half * 2, half * 1.32}, sample.surface)
    graphics2d.drawRectOutline({cx - half, cy - half * 0.66, half * 2, half * 1.32}, 3, sample.line, {layer = 1})
    sample.caption('Pinch to zoom, double tap to reset', cx, cy, {anchor = {0.5, 0.5}, color = sample.ink, size = 17 * math.max(0.6, self.scale)})

    for _, marker in ipairs(self.markers) do
        self:drawMarker(marker)
    end

    local logLeft = area.width * 0.72
    sample.caption('Recognized gestures', logLeft, 20)
    self.journal:draw(logLeft, 56, area.height - 80, 19)
end

return Gestures
