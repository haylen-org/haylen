-- A small immediate-mode GUI written in Lua on the drawing, text and input APIs of the engine: buttons drawn with a nine-slice, a check box, a slider that holds the pointer while it drags, a text field that edits through the native text input with its composition, and a list clipped to its box that scrolls with the wheel or a finger. Tab, the arrows, Enter, the gamepad and the remote move the focus and press the focused control.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local utf8 = require('utf8')
local window = require('haylen.window')

local Immediate = haylen.class('Immediate')

Immediate.field = 4294967296
Immediate.colors = {
    panel = '#FF232938', face = '#FF2C3147', hover = '#FF39405C', pressed = '#FF4C7DFF', line = '#FF4A5170',
    ink = '#FFE8EAF2', muted = '#FF7A8099', accent = '#FF8FB0FF', focus = '#FFF2B23A',
}
Immediate.size = 26

function Immediate:init(toStage, toScreen)
    self.toStage, self.toScreen = toStage, toScreen
    self.active, self.focus, self.order, self.focusIndex = nil, nil, {}, 1
    self.typed, self.edits = {}, nil
    self.slice = graphics2d.newNineSlice(assets.texture('interface/themes/parchment.png'), {source = {144, 0, 32, 32}, borders = {10, 10, 10, 10}})
end

-- Reads the pointer, the wheel and the navigation keys once per frame, before any control draws.
function Immediate:begin()
    local touch = input.touches()[1]
    local screenX, screenY
    if touch then
        screenX, screenY = touch.x, touch.y
        self.down = touch.phase ~= 'ended' and touch.phase ~= 'cancelled'
        self.pressed = touch.phase == 'began'
        self.released = not self.down
    else
        screenX, screenY = input.mousePosition()
        self.down, self.pressed, self.released = input.mouseDown('left'), input.mousePressed('left'), input.mouseReleased('left')
    end
    self.x, self.y = self.toStage(screenX, screenY)
    local _, wheel = input.mouseScroll()
    self.wheel = wheel
    self.hovered = nil
    self.cursor = 'default'

    -- The focus walks the controls in the order they drew last frame.
    local count = #self.order
    if count > 0 then
        local step = (input.keyPressed('tab') or input.keyPressed('down') or input.gamepadPressed('dpadDown')) and 1 or (input.keyPressed('up') or input.gamepadPressed('dpadUp')) and -1 or 0
        if step ~= 0 then
            self.focusIndex = (self.focusIndex - 1 + step) % count + 1
            self.focus = self.order[self.focusIndex]
        end
    end
    self.accept = input.keyPressed('enter') or input.keyPressed('space') or input.gamepadPressed('south')
    self.order = {}
end

-- Hands the frame the text the player typed: native edits of the text field, or characters and Backspace where the platform types through keys.
function Immediate:event(event)
    if event.type == 'textEdited' and event.field == Immediate.field then
        self.edits = event
    elseif event.type == 'character' then
        self.typed[#self.typed + 1] = event.character
    elseif event.type == 'keyDown' and event.key == 'backspace' then
        self.typed[#self.typed + 1] = false
    end
end

function Immediate:finish()
    window.setCursor(self.cursor)
    if self.focus == nil or self.focus ~= self.editing then
        if self.editing then
            input.finishText()
        end
        self.editing = nil
    end
    if self.released then
        self.active = nil
    end
    self.typed = {}
end

-- Registers a control: it joins the focus order, takes the hover and the press, and returns whether the pointer is over it.
function Immediate:control(id, rect)
    self.order[#self.order + 1] = id
    if self.focus == id then
        self.focusIndex = #self.order
    end
    local over = self.x >= rect[1] and self.y >= rect[2] and self.x < rect[1] + rect[3] and self.y < rect[2] + rect[4]
    if over then
        self.hovered = id
        if self.pressed then
            self.active, self.focus = id, id
        end
    end
    return over
end

function Immediate:frame(id, rect, over)
    local colors = Immediate.colors
    graphics2d.drawRect(rect, self.active == id and colors.pressed or over and colors.hover or colors.face)
    graphics2d.drawRectOutline(rect, 2, colors.line, {layer = 1})
    self:ring(id, rect)
end

function Immediate:ring(id, rect)
    if self.focus == id then
        graphics2d.drawRectOutline({rect[1] - 6, rect[2] - 6, rect[3] + 12, rect[4] + 12}, 3, Immediate.colors.focus, {layer = 3})
    end
end

function Immediate:label(text, x, y, color)
    graphics2d.drawText(nil, text, x, y, {size = Immediate.size, color = color or Immediate.colors.ink, layer = 2})
end

function Immediate:button(id, rect, text)
    local over = self:control(id, rect)
    if over then
        self.cursor = 'pointingHand'
    end
    graphics2d.drawNineSlice(self.slice, rect, self.active == id and '#FFB0B0B0' or over and '#FFFFE0C0' or '#FFFFFFFF', nil, 2)
    self:ring(id, rect)
    local width, height = graphics2d.measureText(nil, text, {size = Immediate.size})
    self:label(text, rect[1] + (rect[3] - width) / 2, rect[2] + (rect[4] - height) / 2)
    return (over and self.released and self.active == id) or (self.focus == id and self.accept)
end

function Immediate:checkbox(id, rect, text, checked)
    local box = {rect[1], rect[2], rect[4], rect[4]}
    local over = self:control(id, rect)
    self:frame(id, box, over)
    if checked then
        graphics2d.drawRect({box[1] + 10, box[2] + 10, box[3] - 20, box[4] - 20}, Immediate.colors.accent, {layer = 1})
    end
    local _, height = graphics2d.measureText(nil, text, {size = Immediate.size})
    self:label(text, rect[1] + rect[4] + 16, rect[2] + (rect[4] - height) / 2)
    if (over and self.released and self.active == id) or (self.focus == id and self.accept) then
        return not checked
    end
    return checked
end

-- The slider holds the pointer while it drags, so the knob follows it outside the track until it lets go.
function Immediate:slider(id, rect, value, low, high)
    local over = self:control(id, rect)
    if over or self.active == id then
        self.cursor = 'resizeHorizontal'
    end
    if self.active == id and self.down then
        value = low + math.max(0, math.min(1, (self.x - rect[1]) / rect[3])) * (high - low)
    elseif self.focus == id then
        local step = (input.keyPressed('right') or input.gamepadPressed('dpadRight')) and 1 or (input.keyPressed('left') or input.gamepadPressed('dpadLeft')) and -1 or 0
        value = math.max(low, math.min(high, value + step * (high - low) / 10))
    end
    local share = (value - low) / (high - low)
    local middle = rect[2] + rect[4] / 2
    graphics2d.drawRect({rect[1], middle - 6, rect[3], 12}, Immediate.colors.line)
    graphics2d.drawRect({rect[1], middle - 6, rect[3] * share, 12}, Immediate.colors.accent, {layer = 1})
    graphics2d.drawCircle(rect[1] + rect[3] * share, middle, 18, self.active == id and Immediate.colors.focus or Immediate.colors.ink, {layer = 2})
    self:ring(id, rect)
    return value
end

-- A field keeps its text and caret in `state`, edits through the native text field while it has the focus, and underlines the text an input method still composes.
function Immediate:textField(id, rect, state)
    local over = self:control(id, rect)
    if over then
        self.cursor = 'iBeam'
    end
    self:frame(id, rect, over)
    if self.focus == id then
        if self.edits then
            state.text, state.caret = self.edits.text, self.edits.selectionStart
            state.composing = self.edits.compositionStart and {self.edits.compositionStart, self.edits.compositionEnd} or nil
            self.edits = nil
        end
        for _, typed in ipairs(self.typed) do
            local cut = utf8.offset(state.text, state.caret + 1) or #state.text + 1
            if typed then
                state.text = state.text:sub(1, cut - 1) .. typed .. state.text:sub(cut)
                state.caret = state.caret + utf8.len(typed)
            elseif state.caret > 0 then
                local before = utf8.offset(state.text, state.caret)
                state.text = state.text:sub(1, before - 1) .. state.text:sub(cut)
                state.caret = state.caret - 1
            end
        end
        local left, top = self.toScreen(rect[1], rect[2])
        local right, bottom = self.toScreen(rect[1] + rect[3], rect[2] + rect[4])
        input.editText({text = state.text, selectionStart = state.caret, x = left, y = top, width = right - left, height = bottom - top, returnKey = 'done', autocapitalize = 'words'})
        self.editing = id
    end

    local shown = state.text == '' and self.focus ~= id and state.placeholder or state.text
    local _, height = graphics2d.measureText(nil, 'Ag', {size = Immediate.size})
    local textY = rect[2] + (rect[4] - height) / 2
    graphics2d.pushClip(rect)
    self:label(shown, rect[1] + 16, textY, state.text == '' and Immediate.colors.muted or nil)
    if self.focus == id then
        local before = state.text:sub(1, (utf8.offset(state.text, state.caret + 1) or #state.text + 1) - 1)
        local caretX = rect[1] + 16 + graphics2d.measureText(nil, before, {size = Immediate.size})
        graphics2d.drawRect({caretX, textY, 2, height}, Immediate.colors.accent, {layer = 3})
        if state.composing then
            local start = state.text:sub(1, (utf8.offset(state.text, state.composing[1] + 1) or #state.text + 1) - 1)
            local finish = state.text:sub(1, (utf8.offset(state.text, state.composing[2] + 1) or #state.text + 1) - 1)
            local fromX = rect[1] + 16 + graphics2d.measureText(nil, start, {size = Immediate.size})
            local toX = rect[1] + 16 + graphics2d.measureText(nil, finish, {size = Immediate.size})
            graphics2d.drawRect({fromX, textY + height, toX - fromX, 2}, Immediate.colors.focus, {layer = 3})
        end
    end
    graphics2d.popClip()
end

-- A list draws only inside its box and scrolls with the wheel, a dragging finger or the arrows while it has the focus, and returns the row the player picked.
function Immediate:list(id, rect, rows, state)
    local over = self:control(id, rect)
    local rowHeight = 48
    local most = math.max(0, #rows * rowHeight - rect[4])
    if over and self.wheel ~= 0 then
        state.scroll = state.scroll - self.wheel * rowHeight
    end
    if self.active == id and self.down and state.lastY then
        state.scroll = state.scroll - (self.y - state.lastY)
    end
    state.lastY = self.active == id and self.down and self.y or nil
    state.scroll = math.max(0, math.min(most, state.scroll))
    self:frame(id, rect, false)

    local picked
    graphics2d.pushClip(rect)
    for index, row in ipairs(rows) do
        local top = rect[2] + (index - 1) * rowHeight - state.scroll
        if top + rowHeight >= rect[2] and top <= rect[2] + rect[4] then
            local rowRect = {rect[1] + 6, top + 4, rect[3] - 12, rowHeight - 8}
            local hit = over and self.y >= rowRect[2] and self.y < rowRect[2] + rowRect[4]
            if hit and self.released and self.active == id and math.abs(self.y - (state.pressY or self.y)) < 8 then
                picked = index
            end
            if state.selected == index or hit then
                graphics2d.drawRect(rowRect, state.selected == index and Immediate.colors.pressed or Immediate.colors.hover, {layer = 1})
            end
            self:label(row, rowRect[1] + 12, rowRect[2] + 6)
        end
    end
    graphics2d.popClip()
    if self.pressed and over then
        state.pressY = self.y
    end
    return picked
end

return Immediate
