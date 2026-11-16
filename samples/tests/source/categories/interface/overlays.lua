-- Alerts, a dialog with children, toasts at the top and the bottom of the safe area, tooltips, a popover that points at its button and context menus.
local haylen = require('haylen')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local Overlays = haylen.class('Overlays', Test)

Overlays.code = [[
ui.presentAlert{title = 'Delete the save?', actions = {{id = 'keep', text = 'Keep it', role = 'cancel'}, {id = 'delete', text = 'Delete', role = 'destructive'}}}
ui.present(Confirm, {style = 'dialog'})  -- Confirm:build() returns ui.dialog{...}.
gui:set('notice', {open = true, tone = 'success', text = 'Saved.'})  -- The toast is ui.toast{id = 'notice', duration = 3}.
ui.button{text = 'Hover me', tooltip = 'Shows after the pointer rests.'}
ui.contextMenu{items = {{id = 'rename', text = 'Rename'}, {id = 'erase', text = 'Erase'}}, onSelect = function(event) act(event.item) end, slotButton}
ui.present(Wind, {style = 'popover', anchor = {gui = gui, id = 'wind'}})]]

-- A dialog with a check box, presented over the dimmed screen.
local Confirm = haylen.class('Confirm', ui.Screen)

function Confirm:init(test)
    self.test = test
end

function Confirm:build()
    return ui.dialog{title = 'Delete the save?', message = 'The island and everything on it will be gone.',
        buttons = {{id = 'keep', text = 'Keep it'}, {id = 'delete', text = 'Delete', variant = 'destructive'}},
        onAnswer = function(event)
            self.test:report('The dialog answered "' .. event.button .. '".')
            self.presentation:dismiss({result = event.button})
        end,
        ui.checkbox{text = 'Also delete the cloud copy'},
    }
end

-- The controls of the wind in a popover that points at its button.
local Wind = haylen.class('Wind', ui.Screen)

function Wind:init(test)
    self.test = test
end

function Wind:build()
    return ui.column{gap = 12, padding = 24,
        ui.label{text = 'A popover holds any screen, such as these controls.'},
        ui.slider{value = 0.4, showValue = true, autofocus = true, onChange = function(event) self.test:report(string.format('The wind is %.2f.', event.value)) end},
        ui.toggle{text = 'Gusts', onChange = function(event) self.test:report(event.checked and 'Gusts are on.' or 'Gusts are off.') end},
    }
end

function Overlays:enter()
    self:frame{
        hint = 'Escape or the east button closes a menu, a popover or a dismissible dialog first. Rest the pointer on a control for its tooltip. Right click or long press the save slot, or focus it and press the menu key or the north button, for its context menu.',
        code = Overlays.code,
        focus = 'delete',
        content = {self:columns()},
    }
    -- The menu of the save slot opens as the test starts, so an overlay shows from the first frame.
    timer.after(0.5, function() self.gui:command('slot-menu', 'open') end, {owner = self})
end

function Overlays:report(text)
    self:set('status', {text = text})
end

-- Presents an alert and reports the action the person chose once it left.
function Overlays:ask(alert)
    alert.owner = self
    alert.onDismiss = function(event) self:report('The alert answered "' .. tostring(event.result) .. '".') end
    ui.presentAlert(alert)
end

function Overlays:toast(id, text)
    return function(event)
        event.gui:set(id, {open = true, text = text})
    end
end

function Overlays:columns()
    local toneButtons = {}
    for index, tone in ipairs({'information', 'success', 'warning', 'danger'}) do
        toneButtons[index] = ui.button{text = tone:sub(1, 1):upper() .. tone:sub(2), onClick = function(event)
            event.gui:set('notice', {open = true, tone = tone, text = 'A toast in the "' .. tone .. '" tone'})
        end}
    end
    return layout.columns{
        ui.column{grow = 1, gap = 24,
            layout.section('Alerts and dialogs', {
                ui.button{id = 'delete', text = 'Delete the save', variant = 'destructive', onClick = function()
                    self:ask{title = 'Delete the save?', message = 'The island and everything on it will be gone.', actions = {{id = 'keep', text = 'Keep it', role = 'cancel'}, {id = 'delete', text = 'Delete', role = 'destructive'}}}
                end},
                ui.button{text = 'Open an alert that must be answered', onClick = function()
                    self:ask{title = 'Before you sail', message = 'Escape does not close this alert, so pick an answer.', actions = {{id = 'decline', text = 'Decline'}, {id = 'accept', text = 'Accept'}}}
                end},
                ui.button{text = 'Open a dialog with a check box', onClick = function()
                    ui.present(Confirm(self), {style = 'dialog', owner = self})
                end},
            }),
            layout.section('Component "toast"', {grow = 1,
                ui.row{wrap = true, gap = 12, children = toneButtons},
                ui.row{wrap = true, gap = 12,
                    ui.button{text = 'Toast at the bottom', onClick = self:toast('bottom', 'Saved at the bottom of the safe area')},
                    ui.button{text = 'Toast until closed', onClick = self:toast('sticky', 'This toast stays until the link closes it')},
                    ui.button{text = 'Close it', variant = 'link', onClick = function(event) event.gui:set('sticky', {open = false}) end},
                },
            }),
        },
        -- The slot keeps the room under it free, so its menu opens inside the card.
        layout.section('Component "contextMenu"', {grow = 1, align = 'stretch',
            ui.label{text = 'A right click, a long press or the menu key opens the menu of the slot.', color = 'textMuted'},
            ui.button{text = 'Open the menu from code', variant = 'link', onClick = function(event) event.gui:command('slot-menu', 'open') end},
            ui.column{height = 300,
                ui.contextMenu{id = 'slot-menu', items = {{id = 'rename', text = 'Rename', image = 'interface/icons/pencil.svg', iconColor = false}, {id = 'copy', text = 'Duplicate', image = 'interface/icons/copy.svg', iconColor = false}, {id = 'erase', text = 'Erase', image = 'interface/icons/trash.svg', iconColor = false}, {id = 'upload', text = 'Upload', enabled = false}},
                    onSelect = function(event) self:report('The context menu picked "' .. event.item .. '".') end,
                    ui.button{id = 'slot', text = 'Save slot 1', icon = 'interface/icons/save.svg'},
                },
            },
        }),
        ui.column{grow = 1, gap = 24,
            layout.section('Component "tooltip"', {
                ui.row{wrap = true, gap = 16, alignItems = 'center',
                    ui.button{text = 'Hover me', tooltip = 'Tooltips show after the pointer rests for half a second.'},
                    ui.button{icon = 'interface/themes/fantasy/icons/gear.png', variant = 'icon', tooltip = 'Settings'},
                    ui.button{icon = 'interface/themes/fantasy/icons/coin.png', variant = 'icon', tooltip = 'Twelve gold coins'},
                    ui.badge{text = 'New', tone = 'accent', tooltip = 'Any node takes a tooltip, badges too.'},
                },
                ui.label{text = 'Each of these four has a tooltip.', font = 'caption', color = 'textMuted'},
            }),
            layout.section('Popover', {grow = 1,
                ui.button{id = 'wind', text = 'Wind', icon = 'interface/icons/wind.svg', onClick = function()
                    ui.present(Wind(self), {style = 'popover', anchor = {gui = self.gui, id = 'wind'}, width = 560, owner = self})
                end},
                ui.label{text = 'On a narrow screen the popover rises as a sheet.', font = 'caption', color = 'textMuted'},
            }),
        },
        ui.toast{id = 'notice', duration = 3},
        ui.toast{id = 'bottom', tone = 'success', position = 'bottom'},
        ui.toast{id = 'sticky', tone = 'warning', duration = 0},
    }
end

return Overlays
