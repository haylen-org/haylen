-- Gestures: taps, double taps, long presses, swipes and pinches marked where they happen, a picture that two fingers zoom, turn and move and that a double tap resets, a count of each gesture and the thresholds of the recognizer.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local Journal = require('harness.journal')
local Test = require('harness.test')

local Gestures = haylen.class('Gestures', Test)

Gestures.code = [[
input.setGestureSettings({longPressDuration = 0.5, swipeMinDistance = 120, mouse = true})
for _, gesture in ipairs(input.gestures()) do
    if gesture.type == 'pinch' then
        picture.scale, picture.angle = baseScale * gesture.scale, baseAngle + gesture.rotation
    end
end]]

local kTypes = {'tap', 'doubleTap', 'longPress', 'swipe', 'pinch'}
local kColors = {tap = Test.accent, doubleTap = Test.warm, longPress = Test.violet, swipe = Test.green, pinch = Test.red}
local kLife = 0.9
Gestures.names = {tap = 'Tap', doubleTap = 'Double tap', longPress = 'Long press', swipe = 'Swipe', pinch = 'Pinch'}
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
    self.picture = assets.texture('interface/images/landscape.png', {filter = 'linear'})

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
    self:frame({code = Gestures.code, hint = 'Tap, double tap, hold, swipe or pinch the play area. The left mouse button acts as a finger.', controls = controls, focus = 'longPressDuration'})
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
        return string.format('Swipe by %+.0f, %+.0f', gesture.dx, gesture.dy)
    elseif gesture.type == 'pinch' then
        return string.format('Pinch to a scale of %.2f, turned %.0f degrees and moved %+.0f, %+.0f', gesture.scale, math.deg(gesture.rotation), gesture.dx, gesture.dy)
    end
    return string.format('%s at %.0f, %.0f', Gestures.names[gesture.type], gesture.x, gesture.y)
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
        counts[#counts + 1] = Gestures.names[name] .. ' ' .. self.counts[name]
    end
    self:status(table.concat(counts, ', ') .. string.format('. The picture is at %.2f times its size, turned %.0f degrees.', self.scale, math.deg(self.angle)))
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
    graphics2d.drawText(nil, Gestures.names[marker.type], marker.x, marker.y + 40, {size = 24, color = color, anchor = {0.5, 0}, outlineWidth = 3, outlineColor = '#FF15172A', layer = 4})
end

-- Draws the picture the pinch moves as a photo with a white border, turned and scaled around its middle, with its shadow under it.
function Gestures:draw(area)
    local logWidth = math.max(320, area.width * 0.28)
    local room = area.width - logWidth - 48
    local cx, cy = 24 + room / 2 + self.offsetX, area.height * 0.46 + self.offsetY
    local width = math.min(room * 0.6, area.height * 0.9) * self.scale
    local height = width * self.picture.height / self.picture.width
    local frame = {cx - width / 2 - 12, cy - height / 2 - 12, width + 24, height + 24}
    graphics2d.pushClip({0, 0, room + 36, area.height})
    graphics2d.drawShape({frame[1] + 10, frame[2] + 14, frame[3], frame[4]}, {radius = 18, rotation = self.angle, color = '#66000000', softness = 18})
    graphics2d.drawShape(frame, {radius = 18, rotation = self.angle, color = '#FFF2F3FF', layer = 1})
    graphics2d.drawShape({cx - width / 2, cy - height / 2, width, height}, {radius = 10, rotation = self.angle, texture = self.picture, layer = 2})
    graphics2d.popClip()
    graphics2d.drawText(nil, 'Pinch the picture to zoom, turn and move it, and double tap to reset it.', 24 + room / 2, area.height - 36, {size = 24, color = Test.ink, anchor = {0.5, 0.5}, maxWidth = room - 24, layer = 6})

    for _, marker in ipairs(self.markers) do
        self:drawMarker(marker)
    end

    local logLeft = area.width - logWidth - 24
    local log = {logLeft, 24, logWidth, area.height - 48}
    graphics2d.drawShape(log, {radius = 18, color = Test.surface, borderWidth = 2, borderColor = Test.line})
    graphics2d.drawText(nil, 'Recognized gestures', logLeft + 20, 40, {size = 24, color = Test.muted, layer = 2})
    self.journal:draw(logLeft + 4, 80, log[4] - 72, 20, logWidth - 16)
end

return Gestures
