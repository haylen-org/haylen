-- Keyboard: a keyboard drawn from the key names that lights the keys held and flashes the keys pressed and released, the modifiers, the text typed through `input.text()` and a log of the key events with their repeats.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')
local window = require('haylen.window')

local Journal = require('journal')
local sample = require('sample')

local Keyboard = haylen.class('Keyboard', sample.Test)

-- Each row lists key names with an optional label and width in key units.
local kRows = {
    {{'escape', 'Esc'}, {'digit1', '1'}, {'digit2', '2'}, {'digit3', '3'}, {'digit4', '4'}, {'digit5', '5'}, {'digit6', '6'}, {'digit7', '7'}, {'digit8', '8'}, {'digit9', '9'}, {'digit0', '0'}, {'minus', '-'}, {'equal', '='}, {'backspace', 'Back', 1.6}},
    {{'tab', 'Tab', 1.4}, {'q'}, {'w'}, {'e'}, {'r'}, {'t'}, {'y'}, {'u'}, {'i'}, {'o'}, {'p'}, {'leftBracket', '['}, {'rightBracket', ']'}, {'backslash', '\\', 1.2}},
    {{'capsLock', 'Caps', 1.7}, {'a'}, {'s'}, {'d'}, {'f'}, {'g'}, {'h'}, {'j'}, {'k'}, {'l'}, {'semicolon', ';'}, {'apostrophe', "'"}, {'enter', 'Enter', 1.9}},
    {{'leftShift', 'Shift', 2.2}, {'z'}, {'x'}, {'c'}, {'v'}, {'b'}, {'n'}, {'m'}, {'comma', ','}, {'period', '.'}, {'slash', '/'}, {'rightShift', 'Shift', 2.4}},
    {{'leftControl', 'Ctrl', 1.4}, {'leftSuper', 'Super', 1.4}, {'leftAlt', 'Alt', 1.4}, {'space', 'Space', 5.2}, {'rightAlt', 'Alt', 1.4}, {'left', '<'}, {'up', '^'}, {'down', 'v'}, {'right', '>'}},
}
local kUnits = 15.6
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
    self.text = ''
    self.presses = 0
    self.journal = Journal(60)
    self:frame({
        hint = 'Type anything: held keys light up, presses flash yellow and releases flash red.',
        navigation = not window.hasPointerDevice(),
        focus = 'onScreen',
        controls = {
            ui.label{text = 'The keys belong to the test, so the panel answers the pointer and touch only.', color = 'textMuted', font = 'caption'},
            ui.toggle{id = 'onScreen', text = 'On-screen keyboard', onChange = function(event) window.setKeyboardVisible(event.checked) end},
            ui.button{id = 'clear', text = 'Clear the text and the log', onClick = function()
                self.text = ''
                self.journal:clear()
            end},
            ui.sectionTitle{text = 'Reading keys'},
            ui.label{font = 'monospace', text = "input.keyDown('w')\ninput.keyPressed('space')\ninput.keyReleased('space')\ninput.anyKeyPressed()\ninput.modifiers().shift\ninput.text()"},
        },
    })
end

function Keyboard:exit()
    window.setKeyboardVisible(false)
end

function Keyboard:resize(area)
    self.unit = math.min(area.width * 0.64 / kUnits, 64)
end

-- Key events arrive here as soon as the platform sends them, with the repeats of a held key.
function Keyboard:event(event)
    if event.type == 'keyDown' or event.type == 'keyUp' then
        local repeated = event['repeat'] and ' (repeat)' or ''
        self.journal:add('Event "' .. event.type .. '" for "' .. event.key .. '"' .. repeated, event.type == 'keyDown' and sample.warm or sample.red)
    elseif event.type == 'character' then
        self.journal:add('Character "' .. event.character .. '"', sample.green)
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
            held[#held + 1] = name
        end
    end
    self:status(string.format('Presses %d   modifiers %s   text %d characters', self.presses, #held > 0 and table.concat(held, '+') or 'none', utf8.len(self.text) or 0))
end

function Keyboard:drawKeys(left, top)
    local unit = self.unit
    for _, key in ipairs(self.keys) do
        local rect = {left + key.offset * unit + 3, top + (key.row - 1) * unit + 3, key.width * unit - 6, unit - 6}
        local fill = key.down and sample.accent or sample.surface
        graphics2d.drawRect(rect, fill)
        if key.pressed > 0 then
            graphics2d.drawRectOutline(rect, 4, sample.warm, {layer = 1})
        elseif key.released > 0 then
            graphics2d.drawRectOutline(rect, 4, sample.red, {layer = 1})
        end
        sample.caption(key.label, rect[1] + rect[3] / 2, rect[2] + rect[4] / 2, {size = unit * 0.34, color = key.down and '#FF101418' or sample.ink, anchor = {0.5, 0.5}})
    end
end

function Keyboard:drawModifiers(left, top)
    local modifiers = input.modifiers()
    for index, name in ipairs(kModifiers) do
        local x = left + (index - 1) * 150
        graphics2d.drawRect({x, top, 136, 44}, modifiers[name] and sample.green or sample.surface)
        sample.caption(name:sub(1, 1):upper() .. name:sub(2), x + 68, top + 22, {color = modifiers[name] and '#FF101418' or sample.muted, anchor = {0.5, 0.5}})
    end
end

function Keyboard:draw(area)
    local left, top = 24, 24
    self:drawKeys(left, top)
    local below = top + #kRows * self.unit + 20
    self:drawModifiers(left, below)

    local box = {left, below + 64, kUnits * self.unit, area.height - below - 88}
    graphics2d.drawRect(box, sample.surface)
    sample.caption('Text from "input.text()"', box[1] + 12, box[2] + 10)
    local caret = math.floor(haylen.elapsed() * 2) % 2 == 0 and '|' or ''
    graphics2d.drawText(nil, self.text .. caret, box[1] + 12, box[2] + 44, {size = 30, color = sample.ink, maxWidth = box[3] - 24, layer = 2})

    local logLeft = left + kUnits * self.unit + 32
    sample.caption('Key events', logLeft, top)
    self.journal:draw(logLeft, top + 36, area.height - top - 60, 20)
end

return Keyboard
