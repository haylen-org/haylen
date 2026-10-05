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
    local left, top = 40, 40
    graphics2d.drawRect({left - 20, top - 20, 620, 560}, Immediate.colors.panel, {layer = -1})
    if gui:button('play', {left, top, 260, 72}, 'Set sail') then
        self.clicks = self.clicks + 1
    end
    if gui:button('reset', {left + 300, top, 260, 72}, 'Reset') then
        self.clicks, self.volume, self.name.text, self.name.caret = 0, 0.6, '', 0
    end
    self.sound = gui:checkbox('sound', {left, top + 110, 400, 56}, 'Sound effects', self.sound)
    self.volume = gui:slider('volume', {left + 10, top + 200, 540, 56}, self.volume, 0, 1)
    gui:textField('name', {left, top + 290, 560, 72}, self.name)
    gui:label(string.format('Sailed %d times, volume %d%%, ship "%s".', self.clicks, math.floor(self.volume * 100 + 0.5), self.name.text), left, top + 400, Immediate.colors.muted)

    local picked = gui:list('places', {left + 660, top, 420, 400}, CustomGui.harbor, self.places)
    if picked then
        self.places.selected = picked
    end
    gui:finish()

    -- The safe area and the UI scale come from the engine like everything else the GUI reads.
    local safe = viewport.safeRect()
    local factor, scale = ui.scale()
    local text = string.format('Safe area %d, %d, %d by %d. UI scale %.2f by %.2f. Focus on "%s".', safe.x, safe.y, safe.width, safe.height, factor, scale, tostring(gui.focus))
    Test.caption(text, left, area.height - 60, {size = 22})
end

return CustomGui
