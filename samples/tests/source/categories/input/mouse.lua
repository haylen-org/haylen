-- Mouse: the three buttons held, pressed and released, the wheel and the trackpad with the scroll speed, the movement of each frame, the position in design units and framebuffer pixels, a zone per cursor shape, a hidden cursor and a captured mouse that moves a crosshair by its movement alone.
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')
local window = require('haylen.window')

local Journal = require('harness.journal')
local Test = require('harness.test')
local prompts = require('categories.input.prompts')

local Mouse = haylen.class('Mouse', Test)

Mouse.code = [[
local x, y = input.mousePosition()
local dx, dy = input.mouseDelta()
local scrollX, scrollY = input.mouseScroll()
if input.mousePressed('left') and not input.pointerCaptured() then fire(x, y) end
window.setCursor('pointingHand')
window.setMouseLocked(true)  -- Hides the cursor and reports only the movement.]]

local kButtons = {'left', 'middle', 'right'}
local kCursors = {'default', 'arrow', 'iBeam', 'crosshair', 'pointingHand', 'resizeHorizontal', 'resizeVertical', 'resizeDiagonalDown', 'resizeDiagonalUp', 'resizeAll', 'notAllowed'}
local kCursorNames = {default = 'Default', arrow = 'Arrow', iBeam = 'Text beam', crosshair = 'Crosshair', pointingHand = 'Pointing hand', resizeHorizontal = 'Resize sideways', resizeVertical = 'Resize up and down', resizeDiagonalDown = 'Resize down diagonal', resizeDiagonalUp = 'Resize up diagonal', resizeAll = 'Move', notAllowed = 'Not allowed'}
local kMouse = {220, 330}
local kFlash = 0.3

function Mouse:enter()
    self.flashes = {}
    for _, button in ipairs(kButtons) do
        self.flashes[button] = {pressed = 0, released = 0}
    end
    self.scrollX, self.scrollY = 0, 0
    self.deltaX, self.deltaY = 0, 0
    self.trail = collections.newRingBuffer(40)
    self.journal = Journal(40)
    self.cursor = 'default'
    self.scrollSpeed = input.scrollSpeed()
    self:frame({
        code = Mouse.code,
        hint = 'Hover a zone to change the cursor, click, scroll and move the mouse. On a touch screen, turn on the finger as the mouse to drive the buttons with the first finger down.',
        focus = 'visible',
        controls = {
            ui.toggle{id = 'visible', text = 'Show the cursor', checked = true, onChange = function(event) window.setCursorVisible(event.checked) end},
            ui.toggle{id = 'touchAsMouse', text = 'A finger acts as the mouse', checked = input.touchAsMouse(), onChange = function(event) input.setTouchAsMouse(event.checked) end},
            ui.toggle{id = 'locked', text = 'Capture the mouse', onChange = function(event) self:capture(event.checked) end},
            ui.label{text = 'A captured mouse stays hidden over this toggle and reports only its movement, which moves the white crosshair. A click releases it.', color = 'textMuted', font = 'caption'},
            ui.button{id = 'reset', text = 'Reset the wheel totals', onClick = function() self.scrollX, self.scrollY = 0, 0 end},
            ui.label{text = 'Scroll speed', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'scrollSpeed', value = self.scrollSpeed, min = 0.25, max = 3, step = 0.25, showValue = true, onChange = function(event) input.setScrollSpeed(event.value) end},
        },
    })
end

function Mouse:exit()
    Mouse.super.exit(self)
    input.setTouchAsMouse(false)
    input.setScrollSpeed(self.scrollSpeed)
    window.setCursor('default')
    window.setCursorVisible(true)
    window.setMouseLocked(false)
end

function Mouse:capture(locked)
    window.setMouseLocked(locked)
    self.locked = locked
    self.gui:set('locked', {checked = locked})
    if locked then
        self.aimX, self.aimY = self.area.width * 0.5, self.area.height * 0.5
    end
end

-- The mouse stands on the left, the cursor zones in three columns next to it and the log on the right.
function Mouse:resize(area)
    self.zones = {}
    local columns, gap = 3, 12
    local left = 24 + kMouse[1] + 48
    local width = math.min(230, (area.width * 0.62 - left - gap * (columns - 1)) / columns)
    local rows = math.ceil(#kCursors / columns)
    local height = math.min(72, (area.height - 100 - gap * (rows - 1)) / rows)
    for index, name in ipairs(kCursors) do
        local column, row = (index - 1) % columns, (index - 1) // columns
        self.zones[index] = {name = name, rect = {left + column * (width + gap), 72 + row * (height + gap), width, height}}
    end
end

-- Mouse events arrive with the position of each event, while the functions of `haylen.input` read the state of the frame.
function Mouse:event(event)
    if event.type == 'mouseDown' or event.type == 'mouseUp' then
        self.journal:add(string.format('Event "%s" of the %s button at %.0f, %.0f', event.type, event.button, event.x, event.y), event.type == 'mouseDown' and Test.warm or Test.red)
    elseif event.type == 'mouseScroll' then
        self.journal:add(string.format('Event "mouseScroll" by %.1f, %.1f %s', event.scrollX, event.scrollY, event.precise and 'points of a trackpad' or 'steps of a wheel'), Test.green)
    elseif event.type == 'mouseEnter' or event.type == 'mouseLeave' then
        self.journal:add('Event "' .. event.type .. '"', Test.accent)
    end
end

function Mouse:hoveredZone(x, y)
    for _, zone in ipairs(self.zones) do
        local rect = zone.rect
        if x >= rect[1] and y >= rect[2] and x < rect[1] + rect[3] and y < rect[2] + rect[4] then
            return zone.name
        end
    end
end

function Mouse:update(dt)
    Mouse.super.update(self, dt)
    if not self.area then
        return
    end
    for _, button in ipairs(kButtons) do
        local flash = self.flashes[button]
        flash.pressed = input.mousePressed(button) and kFlash or math.max(0, flash.pressed - dt)
        flash.released = input.mouseReleased(button) and kFlash or math.max(0, flash.released - dt)
    end
    local scrollX, scrollY = input.mouseScroll()
    self.scrollX, self.scrollY = self.scrollX + scrollX, self.scrollY + scrollY
    self.deltaX, self.deltaY = input.mouseDelta()

    local mouseX, mouseY = input.mousePosition()
    local x, y = self:toStage(mouseX, mouseY)
    self.pointerX, self.pointerY = x, y
    if self.deltaX ~= 0 or self.deltaY ~= 0 then
        self.trail:push({x, y})
    end
    if self.locked then
        self.aimX = math.max(0, math.min(self.area.width, self.aimX + self.deltaX))
        self.aimY = math.max(0, math.min(self.area.height, self.aimY + self.deltaY))
        if input.mousePressed('left') and not ui.usingPointer() then
            self:capture(false)
        end
    end

    local cursor = (not ui.usingPointer() and self:hoveredZone(x, y)) or 'default'
    if cursor ~= self.cursor then
        self.cursor = cursor
        window.setCursor(cursor)
    end

    local fx, fy = input.mouseFramebufferPosition()
    self:status(string.format('The mouse is at %.0f, %.0f design units and %.0f, %.0f pixels, %s the window, and it moved %.1f, %.1f this frame.%s', mouseX, mouseY, fx, fy, input.mouseInside() and 'inside' or 'outside', self.deltaX, self.deltaY, input.pointerCaptured() and ' The interface holds it.' or ''))
end

-- Draws the mouse seen from above: a rounded body whose two buttons light while held and flash on a press or a release, the wheel between them, which turns with the vertical total and leans with the horizontal one, and the movement of the frame from its middle, ten times longer.
function Mouse:drawMouse(left, top)
    local width, height = kMouse[1], kMouse[2]
    local split = height * 0.42
    local round = width / 2
    graphics2d.drawShape({left, top, width, height}, {radius = {round, round, round * 0.9, round * 0.9}, color = '#FF2E3548', borderWidth = 3, borderColor = prompts.ink})
    local halves = {left = {left + 3, top + 3, width / 2 - 4, split, {round, 0, 0, 0}}, right = {left + width / 2 + 1, top + 3, width / 2 - 4, split, {0, round, 0, 0}}}
    for side, rect in pairs(halves) do
        local flash = self.flashes[side]
        local rim = flash.pressed > 0 and Test.warm or (flash.released > 0 and Test.red or Test.line)
        graphics2d.drawShape({rect[1], rect[2], rect[3], rect[4]}, {radius = rect[5], color = input.mouseDown(side) and Test.accent or '#FF3A4258', borderWidth = 3, borderColor = rim, layer = 1})
        graphics2d.drawText(prompts.font(), side:sub(1, 1):upper() .. side:sub(2), rect[1] + rect[3] / 2, rect[2] + rect[4] * 0.62, {size = 24, anchor = {0.5, 0.5}, outlineWidth = 2, outlineColor = prompts.ink, layer = 3})
    end

    local wheelX, wheelY = left + width / 2, top + split * 0.45
    local middle = self.flashes.middle
    local rim = middle.pressed > 0 and Test.warm or (middle.released > 0 and Test.red or prompts.ink)
    graphics2d.drawShape({wheelX - 14, wheelY - 40, 28, 80}, {radius = 14, color = input.mouseDown('middle') and Test.accent or '#FF15172A', borderWidth = 3, borderColor = rim, layer = 4})
    local lean = math.max(-6, math.min(6, self.scrollX))
    for notch = 0, 3 do
        local y = wheelY - 30 + ((self.scrollY * 8 + notch * 18) % 60)
        graphics2d.drawLine(wheelX - 7 + lean, y, wheelX + 7 + lean, y, 3, Test.warm, {layer = 5})
    end

    local cx, cy = left + width / 2, top + split + (height - split) * 0.45
    graphics2d.drawCircle(cx, cy, 6, Test.muted, {layer = 2})
    graphics2d.drawLine(cx, cy, cx + self.deltaX * 10, cy + self.deltaY * 10, 5, Test.green, {layer = 3})
    local caption = {size = 20, color = Test.muted, maxWidth = width + 20, layer = 2}
    graphics2d.drawText(nil, 'The green line is the movement, ten times longer.', left, top + height + 20, caption)
    local _, captionHeight = graphics2d.measureText(nil, 'The green line is the movement, ten times longer.', caption)
    graphics2d.drawText(nil, string.format('Wheel total %.1f, %.1f', self.scrollX, self.scrollY), left, top + height + 32 + captionHeight, {size = 22, color = Test.ink, layer = 2})
end

function Mouse:draw(area)
    local margin = 24
    graphics2d.drawText(nil, 'The mouse', margin, 30, {size = 24, color = Test.muted, anchor = {0, 0.5}, layer = 2})
    self:drawMouse(margin, 72)
    graphics2d.drawText(nil, 'Hover a card for its cursor', self.zones[1].rect[1], 30, {size = 24, color = Test.muted, anchor = {0, 0.5}, layer = 2})
    for _, zone in ipairs(self.zones) do
        local hovered = zone.name == self.cursor and zone.name ~= 'default'
        local rect = zone.rect
        graphics2d.drawShape(rect, {radius = 14, color = hovered and '#FF34405E' or Test.surface, borderWidth = 2, borderColor = hovered and Test.accent or Test.line})
        graphics2d.drawText(nil, kCursorNames[zone.name], rect[1] + rect[3] / 2, rect[2] + rect[4] / 2, {size = 22, color = hovered and Test.ink or '#FFC4C9DC', anchor = {0.5, 0.5}, maxWidth = rect[3] - 16, wrap = false, minScale = 0.7, layer = 2})
    end

    local last = self.zones[3].rect
    local logLeft = last[1] + last[3] + margin
    local log = {logLeft, margin, area.width - logLeft - margin, area.height - margin * 2}
    graphics2d.drawShape(log, {radius = 18, color = Test.surface, borderWidth = 2, borderColor = Test.line})
    graphics2d.drawText(nil, 'Mouse events', logLeft + 20, margin + 16, {size = 24, color = Test.muted, layer = 2})
    if self.journal.lines.size == 0 then
        graphics2d.drawText(nil, 'Click, scroll or move the mouse over the play area to see its events.', logLeft + log[3] / 2, margin + log[4] / 2, {size = 22, color = Test.muted, anchor = {0.5, 0.5}, maxWidth = log[3] - 48, align = 'center', layer = 2})
    else
        self.journal:draw(logLeft + 4, margin + 56, log[4] - 72, 19, log[3] - 16)
    end

    local points = self.trail:values()
    for index = 2, #points do
        graphics2d.drawLine(points[index - 1][1], points[index - 1][2], points[index][1], points[index][2], 3, '#608FB0FF', {layer = 6})
    end
    if self.pointerX then
        graphics2d.drawRing(self.pointerX, self.pointerY, 14, 3, Test.accent, {layer = 7})
    end
    if self.locked then
        graphics2d.drawLine(self.aimX - 24, self.aimY, self.aimX + 24, self.aimY, 3, Test.ink, {layer = 8})
        graphics2d.drawLine(self.aimX, self.aimY - 24, self.aimX, self.aimY + 24, 3, Test.ink, {layer = 8})
    end
end

return Mouse
