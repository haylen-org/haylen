-- Stick focus: a compass of five buttons whose focus the left stick of a gamepad moves up, down, left and right, a push up with a little sideways lean moving it up, and the south button pressing the focused one. The focus goes back to the middle once every stick rests, so each push starts from there, and a line tells the last move. The virtual controller connects again in the gamepad it had, and every other gamepad, such as the one that a simulator connects, stays still while the virtual one acts. On iPhones and iPads the plugin "native-demo" draws the stick and the south button of a virtual controller of GameController, which the UI tests drive on a simulator, and every other platform takes the gamepads that connect.
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local Test = require('harness.test')
local demo = require('native-demo')

local StickFocus = haylen.class('StickFocus', Test)

StickFocus.code = [[
local focus = ui.focused()  -- {gui, id} of the control with the focus.
local x, y = input.gamepadStick('left', pad)
local shown = demo.showVirtualController():await()  -- iPhones and iPads.]]

StickFocus.names = {north = 'North', west = 'West', middle = 'Middle', east = 'East', south = 'South'}
StickFocus.pads = 8
-- The buttons that tell whether another gamepad acts.
StickFocus.buttons = {'south', 'east', 'west', 'north', 'dpadUp', 'dpadDown', 'dpadLeft', 'dpadRight', 'start', 'back'}

function StickFocus:enter()
    self.moves = 0
    self.from = 'middle'
    self.shown = 'middle'
    self.connections = 0
    self.still = {}
    local function button(id)
        return ui.button{id = id, text = StickFocus.names[id], width = 220, onClick = function()
            self:set('pressed', {text = string.format('The south button pressed %s.', StickFocus.names[id])})
        end}
    end
    self:frame{
        hint = 'Push the left stick of a gamepad up, down, left or right. The focus moves one step, and comes back to the middle once the stick rests. On iPhones and iPads the button below the lines connects the virtual controller again.',
        focus = 'middle',
        content = {
            -- The compass and the lines sit side by side, so the whole test fits the region of a phone without a scroll, between the stick and the button that the virtual controller draws at its sides.
            ui.row{gap = 48, justify = 'center', alignItems = 'center', align = 'stretch',
                -- The scope keeps the focus inside the compass, so a stick held past the repeat of the focus stops at the edge of the compass.
                ui.column{gap = 16, alignItems = 'center', focusScope = true, onCancel = function() self:cancel() end,
                    button('north'),
                    ui.row{gap = 16, button('west'), button('middle'), button('east')},
                    button('south'),
                },
                ui.column{gap = 12, width = 560,
                    ui.label{id = 'focus', text = 'The focus rests on Middle.'},
                    ui.label{id = 'move', text = 'The stick has not moved the focus yet.'},
                    ui.label{id = 'pressed', text = 'The south button has pressed nothing yet.', color = 'textMuted'},
                    ui.label{id = 'controller', text = 'The virtual controller is not connected.', color = 'textMuted'},
                    ui.label{id = 'slot', text = 'The virtual controller has taken no gamepad yet.', color = 'textMuted'},
                    ui.label{id = 'others', text = 'No other gamepad is connected.', color = 'textMuted'},
                    ui.button{id = 'reconnect', text = 'Connect the controller again', onClick = function() self:spawn(function() self:reconnect() end) end},
                },
            },
        },
    }
    self:spawn(function()
        self:showController()
    end)
end

function StickFocus:exit()
    if self.connected then
        demo.hideVirtualController()
    end
    StickFocus.super.exit(self)
end

-- Connects the virtual controller where the platform has one, and tells why it does not elsewhere.
function StickFocus:showController()
    if not demo.available() then
        self:set('controller', {text = 'The native part of "native-demo" does not run here, so only the gamepads that connect move the focus.'})
        return
    end
    local shown, err = demo.showVirtualController():await()
    if shown then
        self.connected = true
        self.name = shown.name
        self:set('controller', {text = string.format('The virtual controller "%s" shows its stick and its south button.', shown.name)})
    else
        self:set('controller', {text = err.message .. ' The gamepads that connect move the focus.'})
    end
end

-- Takes the virtual controller away and connects it again, and gives the focus back to the middle, which the button took.
function StickFocus:reconnect()
    if not self.connected then
        return
    end
    demo.hideVirtualController():await()
    self.connected = false
    self.slot = nil
    self:showController()
    self.gui:command('middle', 'focus')
end

-- Whether a gamepad presses a button or leans a stick.
function StickFocus.acting(pad)
    local x, y = input.gamepadStick('left', pad)
    if x ~= 0 or y ~= 0 then
        return true
    end
    for _, button in ipairs(StickFocus.buttons) do
        if input.gamepadDown(button, pad) then
            return true
        end
    end
    return false
end

-- Tells whether each gamepad other than the virtual controller stayed still since the test opened.
function StickFocus:watchOthers()
    local lines = {}
    for pad = 1, StickFocus.pads do
        if input.gamepadConnected(pad) and input.gamepadName(pad) ~= self.name then
            if self.still[pad] == nil then
                self.still[pad] = true
            end
            self.still[pad] = self.still[pad] and not StickFocus.acting(pad)
            local state = self.still[pad] and 'stays still' or 'moved'
            lines[#lines + 1] = string.format('Gamepad %d "%s" %s.', pad, input.gamepadName(pad), state)
        end
    end
    local text = #lines > 0 and table.concat(lines, ' ') or 'No other gamepad is connected.'
    if text ~= self.othersText then
        self.othersText = text
        self:set('others', {text = text})
    end
end

-- Counts each connection of the virtual controller with the gamepad it takes, which the engine keeps while the controller stays connected.
function StickFocus:watchSlot()
    local slot = nil
    for pad = 1, StickFocus.pads do
        if self.name and input.gamepadConnected(pad) and input.gamepadName(pad) == self.name then
            slot = pad
        end
    end
    if slot and slot ~= self.slot then
        self.connections = self.connections + 1
        self:set('slot', {text = string.format('Connection %d of the virtual controller took gamepad %d.', self.connections, slot)})
    end
    self.slot = slot
end

-- Whether every stick and directional pad of the gamepads rests.
function StickFocus.resting()
    for pad = 1, StickFocus.pads do
        local x, y = input.gamepadStick('left', pad)
        local pressed = input.gamepadDown('dpadUp', pad) or input.gamepadDown('dpadDown', pad) or input.gamepadDown('dpadLeft', pad) or input.gamepadDown('dpadRight', pad)
        if x ~= 0 or y ~= 0 or pressed then
            return false
        end
    end
    return true
end

function StickFocus:update(dt)
    StickFocus.super.update(self, dt)
    self:watchSlot()
    self:watchOthers()
    local focus = ui.focused()
    local id = focus and focus.gui == self.gui and StickFocus.names[focus.id] and focus.id
    if not id then
        return
    end

    if id ~= self.shown then
        self.shown = id
        self:set('focus', {text = string.format('The focus rests on %s.', StickFocus.names[id])})
    end

    if id ~= self.from then
        self.moves = self.moves + 1
        self:set('move', {text = string.format('Move %d took the focus from %s to %s.', self.moves, StickFocus.names[self.from], StickFocus.names[id])})
        self.from = id
    end

    -- Each push starts from the middle, and every button around it ends its direction, so a push moves the focus one step however long it lasts.
    if id ~= 'middle' and StickFocus.resting() then
        self.gui:command('middle', 'focus')
        self.from = 'middle'
    end
end

return StickFocus
