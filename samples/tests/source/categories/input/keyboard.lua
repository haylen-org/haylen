-- Keyboard: a keyboard drawn from the key names that lights the keys held and flashes the keys pressed and released, the modifiers, the text typed through `input.text()` and a log of the key events with their repeats.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')
local window = require('haylen.window')

local Journal = require('harness.journal')
local Test = require('harness.test')
local prompts = require('categories.input.prompts')

local Keyboard = haylen.class('Keyboard', Test)

Keyboard.code = [[
if input.keyPressed('space') then hero:jump() end
local running = input.keyDown('w') and input.modifiers().shift
typed = typed .. input.text()  -- The characters typed this frame.
window.setKeyboardVisible(true)]]

-- Each row lists key names with an optional label and width in key units, and every row spans the same width. The arrows stand apart in an inverted T under the last two rows.
local kRows = {
    {{'escape', 'Esc'}, {'digit1', '1'}, {'digit2', '2'}, {'digit3', '3'}, {'digit4', '4'}, {'digit5', '5'}, {'digit6', '6'}, {'digit7', '7'}, {'digit8', '8'}, {'digit9', '9'}, {'digit0', '0'}, {'minus', '-'}, {'equal', '='}, {'backspace', 'Backspace', 2}},
    {{'tab', 'Tab', 1.5}, {'q'}, {'w'}, {'e'}, {'r'}, {'t'}, {'y'}, {'u'}, {'i'}, {'o'}, {'p'}, {'leftBracket', '['}, {'rightBracket', ']'}, {'backslash', '\\', 1.5}},
    {{'capsLock', 'Caps', 1.8}, {'a'}, {'s'}, {'d'}, {'f'}, {'g'}, {'h'}, {'j'}, {'k'}, {'l'}, {'semicolon', ';'}, {'apostrophe', "'"}, {'enter', 'Enter', 2.2}},
    {{'leftShift', 'Shift', 2.3}, {'z'}, {'x'}, {'c'}, {'v'}, {'b'}, {'n'}, {'m'}, {'comma', ','}, {'period', '.'}, {'slash', '/'}, {'rightShift', 'Shift', 2.7}},
    {{'leftControl', 'Ctrl', 1.5}, {'leftSuper', 'Super', 1.3}, {'leftAlt', 'Alt', 1.3}, {'space', 'Space', 6.4}, {'rightAlt', 'Alt', 1.3}, {'rightSuper', 'Super', 1.3}, {'rightControl', 'Ctrl', 1.9}},
}
-- The arrows, with their row, their column after the main block and the angle of their glyph.
local kArrows = {{'up', 4, 1, -math.pi / 2}, {'left', 5, 0, math.pi}, {'down', 5, 1, math.pi / 2}, {'right', 5, 2, 0}}
local kMainUnits = 15
local kUnits = 18.4
local kFlash = 0.35
local kModifiers = {'shift', 'control', 'alt', 'super'}

function Keyboard:enter()
    self.keys = {}
    for row, keys in ipairs(kRows) do
        local offset = 0
        for _, key in ipairs(keys) do
            local width = key[3] or 1
            self.keys[#self.keys + 1] = {name = key[1], label = key[2] or key[1]:upper(), row = row, offset = offset, width = width, pressed = 0, released = 0}
            offset = offset + width
        end
    end
    for _, arrow in ipairs(kArrows) do
        self.keys[#self.keys + 1] = {name = arrow[1], angle = arrow[4], row = arrow[2], offset = kMainUnits + 0.4 + arrow[3], width = 1, pressed = 0, released = 0}
    end
    self.text = ''
    self.presses = 0
    self.journal = Journal(60)
    self:frame({
        code = Keyboard.code,
        hint = 'Type anything: held keys light up, presses flash yellow and releases flash red.',
        play = true,
        controls = {
            ui.label{text = 'Every key reaches the test while the play area has the focus, and Escape still goes back.', color = 'textMuted', font = 'caption'},
            ui.toggle{id = 'onScreen', text = 'On-screen keyboard', onChange = function(event) window.setKeyboardVisible(event.checked) end},
            ui.button{id = 'clear', text = 'Clear the text and the log', onClick = function()
                self.text = ''
                self.journal:clear()
            end},
        },
    })
end

function Keyboard:exit()
    Keyboard.super.exit(self)
    window.setKeyboardVisible(false)
end

function Keyboard:resize(area)
    self.unit = math.min((area.width * 0.7 - 48) / kUnits, (area.height * 0.62) / 5, 64)
end

-- Key events arrive here as soon as the platform sends them, with the repeats of a held key.
function Keyboard:event(event)
    if event.type == 'keyDown' or event.type == 'keyUp' then
        local repeated = event['repeat'] and ' (repeat)' or ''
        self.journal:add('Event "' .. event.type .. '" of "' .. event.key .. '"' .. repeated, event.type == 'keyDown' and Test.warm or Test.red)
    elseif event.type == 'character' then
        self.journal:add('Character "' .. event.character .. '"', Test.green)
    end
end

function Keyboard:update(dt)
    Keyboard.super.update(self, dt)
    for _, key in ipairs(self.keys) do
        key.down = input.keyDown(key.name)
        key.pressed = input.keyPressed(key.name) and kFlash or math.max(0, key.pressed - dt)
        key.released = input.keyReleased(key.name) and kFlash or math.max(0, key.released - dt)
    end
    if input.anyKeyPressed() then
        self.presses = self.presses + 1
    end

    self.text = self.text .. input.text()
    if input.keyPressed('backspace') and #self.text > 0 then
        self.text = self.text:sub(1, utf8.offset(self.text, -1) - 1)
    end

    local modifiers = input.modifiers()
    local held = {}
    for _, name in ipairs(kModifiers) do
        if modifiers[name] then
            held[#held + 1] = name:sub(1, 1):upper() .. name:sub(2)
        end
    end
    self:status(string.format('%d presses, %s held, and %d characters typed.', self.presses, #held > 0 and table.concat(held, ' and ') or 'no modifier', utf8.len(self.text) or 0))
end

-- Draws a key cap: a darker side under a light top that sinks while the key is held, with a rim that flashes on a press or a release.
function Keyboard:drawKey(key, left, top)
    local unit = self.unit
    local x, y, width, height = left + key.offset * unit + 3, top + (key.row - 1) * unit + 3, key.width * unit - 6, unit - 6
    local sink = key.down and height * 0.08 or 0
    local radius = unit * 0.16
    local rim = key.pressed > 0 and Test.warm or (key.released > 0 and Test.red or prompts.ink)
    graphics2d.drawShape({x, y + height * 0.1, width, height * 0.9}, {radius = radius, color = key.down and '#FF2A5BC0' or '#FF5D6596', layer = 1})
    graphics2d.drawShape({x, y + sink, width, height * 0.86}, {radius = radius, color = key.down and Test.accent or '#FFE6E8F5', borderWidth = (key.pressed > 0 or key.released > 0) and 3 or 2, borderColor = rim, layer = 2})
    local middleX, middleY = x + width / 2, y + sink + height * 0.43
    if key.angle then
        local size = unit * 0.2
        local points = {}
        for index, corner in ipairs({{1.2, 0}, {-0.8, -1}, {-0.8, 1}}) do
            local cos, sin = math.cos(key.angle), math.sin(key.angle)
            points[index] = {middleX + (corner[1] * cos - corner[2] * sin) * size, middleY + (corner[1] * sin + corner[2] * cos) * size}
        end
        graphics2d.drawPolygon(points, prompts.ink, {layer = 3})
        return
    end
    graphics2d.drawText(prompts.font(), key.label, middleX, middleY, {size = unit * (#key.label > 1 and 0.3 or 0.4), color = prompts.ink, anchor = {0.5, 0.5}, maxWidth = width - 8, wrap = false, minScale = 0.6, layer = 3})
end

-- Draws a chip per modifier that lights in its color while held.
function Keyboard:drawModifiers(left, top)
    local modifiers = input.modifiers()
    local font = prompts.font()
    local x = left
    for _, name in ipairs(kModifiers) do
        local text = name:sub(1, 1):upper() .. name:sub(2)
        local width = graphics2d.measureText(font, text, {size = 24}) + 40
        local held = modifiers[name]
        graphics2d.drawShape({x, top, width, 44}, {radius = 22, color = held and Test.green or Test.surface, borderWidth = 2, borderColor = held and Test.green or Test.line, layer = 1})
        graphics2d.drawText(font, text, x + width / 2, top + 22, {size = 24, color = held and prompts.ink or Test.ink, anchor = {0.5, 0.5}, layer = 2})
        x = x + width + 12
    end
end

function Keyboard:draw(area)
    local margin = 24
    local left, top = margin, margin
    for _, key in ipairs(self.keys) do
        self:drawKey(key, left, top)
    end
    local below = top + 5 * self.unit + 20
    graphics2d.drawText(nil, 'Modifiers', left, below + 22, {size = 24, color = Test.muted, anchor = {0, 0.5}, layer = 2})
    self:drawModifiers(left + 130, below)

    local width = kUnits * self.unit
    local box = {left, below + 64, width, area.height - below - 64 - margin}
    graphics2d.drawShape(box, {radius = 18, color = Test.surface, borderWidth = 2, borderColor = Test.line})
    graphics2d.drawText(nil, 'Text from "input.text()"', box[1] + 20, box[2] + 16, {size = 24, color = Test.muted, layer = 2})
    local caret = math.floor(haylen.elapsed() * 2) % 2 == 0 and '|' or ''
    graphics2d.drawText(nil, self.text .. caret, box[1] + 20, box[2] + 52, {size = 30, color = Test.ink, maxWidth = box[3] - 40, layer = 2})

    local logLeft = left + width + 24
    local log = {logLeft, top, area.width - logLeft - margin, area.height - top - margin}
    graphics2d.drawShape(log, {radius = 18, color = Test.surface, borderWidth = 2, borderColor = Test.line})
    graphics2d.drawText(nil, 'Key events', logLeft + 20, top + 16, {size = 24, color = Test.muted, layer = 2})
    self.journal:draw(logLeft + 4, top + 56, log[4] - 72, 20, log[3] - 16)
end

return Keyboard
