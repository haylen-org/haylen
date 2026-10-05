-- Mouse: the three buttons held, pressed and released, the wheel, the movement of each frame, the position in design units and framebuffer pixels, a zone per cursor shape, a hidden cursor and a captured mouse that moves a crosshair by its movement alone.
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')
local window = require('haylen.window')

local Journal = require('harness.journal')
local Test = require('harness.test')

local Mouse = haylen.class('Mouse', Test)

local kButtons = {'left', 'middle', 'right'}
local kCursors = {'default', 'arrow', 'iBeam', 'crosshair', 'pointingHand', 'resizeHorizontal', 'resizeVertical', 'resizeDiagonalDown', 'resizeDiagonalUp', 'resizeAll', 'notAllowed'}
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
    self:frame({
        hint = 'Hover a zone to change the cursor, click, scroll and move the mouse.',
        focus = 'visible',
        controls = {
            ui.toggle{id = 'visible', text = 'Show the cursor', checked = true, onChange = function(event) window.setCursorVisible(event.checked) end},
            ui.toggle{id = 'locked', text = 'Capture the mouse', onChange = function(event) self:capture(event.checked) end},
            ui.label{text = 'A captured mouse stays hidden over this toggle and reports only its movement, which moves the white crosshair. A click releases it.', color = 'textMuted', font = 'caption'},
            ui.button{id = 'reset', text = 'Reset the wheel totals', onClick = function() self.scrollX, self.scrollY = 0, 0 end},
            ui.sectionTitle{text = 'Reading the mouse'},
            ui.label{font = 'monospace', text = "input.mouseDown('left')\ninput.mousePosition()\ninput.mouseDelta()\ninput.mouseScroll()\ninput.pointerCaptured()\nwindow.setCursor('iBeam')\nwindow.setMouseLocked(true)"},
        },
    })
end

function Mouse:exit()
    Mouse.super.exit(self)
    window.setCursor('default')
    window.setCursorVisible(true)
    window.setMouseLocked(false)
end

function Mouse:capture(locked)
    window.setMouseLocked(locked)
    self.locked = locked
    self.document:set('locked', {checked = locked})
    if locked then
        self.aimX, self.aimY = self.area.width * 0.5, self.area.height * 0.5
    end
end

function Mouse:resize(area)
    self.zones = {}
    local columns, width, height = 3, 224, 64
    local left = 310
    for index, name in ipairs(kCursors) do
        local column, row = (index - 1) % columns, (index - 1) // columns
        self.zones[index] = {name = name, rect = {left + column * (width + 12), 60 + row * (height + 12), width, height}}
    end
end

-- Mouse events arrive with the position of each event, while the functions of `haylen.input` read the state of the frame.
function Mouse:event(event)
    if event.type == 'mouseDown' or event.type == 'mouseUp' then
        self.journal:add(string.format('Event "%s" of button "%s" at %.0f, %.0f', event.type, event.button, event.x, event.y), event.type == 'mouseDown' and Test.warm or Test.red)
    elseif event.type == 'mouseScroll' then
        self.journal:add(string.format('Event "mouseScroll" %.1f, %.1f', event.scrollX, event.scrollY), Test.green)
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
    self:status(string.format('Design units %.0f, %.0f, pixels %.0f, %.0f, delta %.1f, %.1f, %s, %s, cursor "%s"', mouseX, mouseY, fx, fy, self.deltaX, self.deltaY, input.mouseInside() and 'inside' or 'outside', input.pointerCaptured() and 'captured by the interface' or 'free', self.cursor))
end

function Mouse:drawMouse(left, top)
    local width, height = 240, 360
    graphics2d.drawRect({left, top, width, height}, Test.surface)
    local halves = {left = {left, top, width * 0.42, 150}, middle = {left + width * 0.42, top, width * 0.16, 150}, right = {left + width * 0.58, top, width * 0.42, 150}}
    for _, button in ipairs(kButtons) do
        local rect = halves[button]
        local flash = self.flashes[button]
        graphics2d.drawRect({rect[1] + 3, rect[2] + 3, rect[3] - 6, rect[4] - 6}, input.mouseDown(button) and Test.accent or '#FF2E3548', {layer = 1})
        if flash.pressed > 0 then
            graphics2d.drawRectOutline(rect, 4, Test.warm, {layer = 2})
        elseif flash.released > 0 then
            graphics2d.drawRectOutline(rect, 4, Test.red, {layer = 2})
        end
        Test.caption(button:sub(1, 1):upper() .. button:sub(2), rect[1] + rect[3] / 2, rect[2] + rect[4] + 14, {anchor = {0.5, 0}, size = 18})
    end

    -- The wheel turns with the vertical total and its notch slides with the horizontal total.
    local wheelX, wheelY = left + width / 2, top + 75
    local notch = (self.scrollY * 8) % 60 - 30
    graphics2d.drawLine(wheelX - 8 + math.max(-8, math.min(8, self.scrollX)), wheelY + notch, wheelX + 8 + math.max(-8, math.min(8, self.scrollX)), wheelY + notch, 4, Test.warm, {layer = 3})
    Test.caption(string.format('Wheel total %.1f, %.1f', self.scrollX, self.scrollY), left, top + height + 20)

    -- The movement of this frame, drawn ten times longer from the middle of the body.
    local cx, cy = left + width / 2, top + 270
    graphics2d.drawCircle(cx, cy, 6, Test.muted, {layer = 1})
    graphics2d.drawLine(cx, cy, cx + self.deltaX * 10, cy + self.deltaY * 10, 4, Test.green, {layer = 2})
    Test.caption('Delta, ten times longer', cx, cy + 40, {anchor = {0.5, 0}, size = 18})
end

function Mouse:draw(area)
    self:drawMouse(24, 60)
    Test.caption('Buttons, wheel and movement', 24, 20)
    Test.caption('Hover a zone for its cursor', self.zones[1].rect[1], 20)
    for _, zone in ipairs(self.zones) do
        local hovered = zone.name == self.cursor and zone.name ~= 'default'
        graphics2d.drawRect(zone.rect, hovered and '#FF34405E' or Test.surface)
        Test.caption('Cursor\n"' .. zone.name .. '"', zone.rect[1] + 12, zone.rect[2] + zone.rect[4] / 2, {anchor = {0, 0.5}, size = 18, color = hovered and Test.ink or Test.muted})
    end

    local logLeft = self.zones[3].rect[1] + self.zones[3].rect[3] + 32
    Test.caption('Mouse events', logLeft, 20)
    self.journal:draw(logLeft, 60, area.height - 80, 18)

    local points = self.trail:values()
    for index = 2, #points do
        graphics2d.drawLine(points[index - 1][1], points[index - 1][2], points[index][1], points[index][2], 3, '#608FB0FF', {layer = 4})
    end
    if self.pointerX then
        graphics2d.drawRing(self.pointerX, self.pointerY, 14, 3, Test.accent, {layer = 5})
    end
    if self.locked then
        graphics2d.drawLine(self.aimX - 24, self.aimY, self.aimX + 24, self.aimY, 3, Test.ink, {layer = 6})
        graphics2d.drawLine(self.aimX, self.aimY - 24, self.aimX, self.aimY + 24, 3, Test.ink, {layer = 6})
    end
end

return Mouse
