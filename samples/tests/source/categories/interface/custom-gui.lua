-- A GUI written in Lua instead of "haylen.ui": the immediate-mode controls of "categories.interface.immediate" drawn on the stage every frame, with the pointer, touch, keys, gamepads, the native text input, clipping, a nine-slice, the cursor, the safe area and the UI scale all read from the engine.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local Immediate = require('categories.interface.immediate')
local Test = require('harness.test')

local CustomGui = haylen.class('CustomGui', Test)

CustomGui.code = [[
local x, y = input.mousePosition()
local over = x >= rect[1] and x < rect[1] + rect[3] and y >= rect[2] and y < rect[2] + rect[4]
graphics2d.drawNineSlice(slice, rect, over and '#FFFFE0C0' or '#FFFFFFFF', {borderScale = 0.6})
if over and input.mouseReleased('left') then sail() end
window.setCursor(over and 'pointingHand' or 'default')
input.editText({text = name, selectionStart = caret, x = 40, y = 330, width = 560, height = 72, returnKey = 'done'})]]

CustomGui.focusNames = {play = 'the button "Set sail"', reset = 'the button "Reset"', sound = 'the check box', volume = 'the slider', name = 'the text field', places = 'the list'}
CustomGui.harbor = {'North pier', 'Fish market', 'Lighthouse', 'Old warehouse', 'Shipyard', 'Customs house', 'Rope maker', 'Sail loft', 'Tavern', 'Watchtower', 'Boat ramp', 'Fuel dock'}

function CustomGui:init(entry)
    CustomGui.super.init(self, entry)
    self.clicks, self.sound, self.volume = 0, true, 0.6
    self.name = {text = '', caret = 0, placeholder = 'Name of the ship'}
    self.places = {scroll = 0}
end

function CustomGui:enter()
    self.controls = Immediate(function(x, y) return self:toStage(x, y) end, function(x, y) return self.camera:worldToScreen(x, y) end)
    self:frame{
        hint = 'Every control on the stage is drawn by Lua each frame. Click or tap them, drag the slider and the list, type a name with the keyboard or an input method, and walk the focus with Tab, the arrows, the gamepad or the remote, pressing with Enter or the south button.',
        code = CustomGui.code,
        play = true,
    }
end

function CustomGui:exit()
    window.setCursor('default')
    input.finishText()
    CustomGui.super.exit(self)
end

function CustomGui:event(event)
    self.controls:event(event)
end

function CustomGui:draw(area)
    local gui = self.controls
    gui:begin()
    -- The panel of the GUI stands in the middle of the stage.
    local left, top = math.max(24, math.floor((area.width - 1132) / 2) + 24), math.max(24, math.floor((area.height - 520) / 2) + 24)
    graphics2d.drawShape({left - 24, top - 24, 1132, 520}, {radius = 20, color = Immediate.colors.panel, borderWidth = 2, borderColor = Immediate.colors.line, layer = -1})
    if gui:button('play', {left, top, 260, 72}, 'Set sail') then
        self.clicks = self.clicks + 1
    end
    if gui:button('reset', {left + 300, top, 260, 72}, 'Reset') then
        self.clicks, self.volume, self.name.text, self.name.caret = 0, 0.6, '', 0
    end
    self.sound = gui:checkbox('sound', {left, top + 110, 400, 56}, 'Sound effects', self.sound)
    self.volume = gui:slider('volume', {left + 10, top + 200, 540, 56}, self.volume, 0, 1)
    gui:textField('name', {left, top + 290, 560, 72}, self.name)
    local ship = self.name.text == '' and 'a ship without a name' or 'the ' .. self.name.text
    local times = self.clicks == 1 and 'once' or self.clicks .. ' times'
    gui:label(string.format('You set sail %s in %s, at %d%% volume.', times, ship, math.floor(self.volume * 100 + 0.5)), left, top + 432, Immediate.colors.muted)

    local picked = gui:list('places', {left + 660, top, 420, 400}, CustomGui.harbor, self.places)
    if picked then
        self.places.selected = picked
    end
    gui:finish()

    -- The safe area and the UI scale come from the engine like everything else the GUI reads.
    local safe = viewport.safeRect()
    local factor = ui.scale()
    self:status(string.format('The safe area spans %d by %d units, the UI scale is %.2f and the focus is on %s.', math.floor(safe.width + 0.5), math.floor(safe.height + 0.5), factor, CustomGui.focusNames[gui.focus] or 'nothing'))
end

return CustomGui
