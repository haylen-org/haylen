-- Modal dialogs with children, toasts at the top and the bottom of the safe area, tooltips, popovers and context menus.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local Overlays = haylen.class('Overlays', Test)

function Overlays:enter()
    self:frame{
        hint = 'Escape or the east button closes a dismissible dialog, a popover or a menu first. Rest the pointer on a control for its tooltip. Right click or long press the save slot, or focus it and press the menu key or the north button, for its context menu.',
        focus = 'delete',
        content = {self:columns()},
    }
end

function Overlays:report(text)
    self:set('status', {text = text})
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
            layout.section('Component "dialog"', {
                ui.button{id = 'delete', text = 'Delete the save', variant = 'destructive', onClick = function(event)
                    event.gui:set('confirm', {open = true})
                end},
                ui.button{text = 'Open a dialog that must be answered', onClick = function(event)
                    event.gui:set('terms', {open = true})
                end},
            }),
            layout.section('Component "toast"', {
                ui.row{gap = 12, children = toneButtons},
                ui.row{gap = 12,
                    ui.button{text = 'At the bottom', onClick = self:toast('bottom', 'Saved at the bottom of the safe area')},
                    ui.button{text = 'Until closed', onClick = self:toast('sticky', 'This toast stays until the button below closes it')},
                    ui.button{text = 'Close it', variant = 'link', onClick = function(event) event.gui:set('sticky', {open = false}) end},
                },
            }),
        },
        ui.column{grow = 1, gap = 24,
            layout.section('Component "tooltip"', {
                ui.row{gap = 16,
                    ui.button{text = 'Hover me', tooltip = 'Tooltips show after the pointer rests for half a second.'},
                    ui.button{icon = 'interface/icons/gear.png', variant = 'icon', tooltip = 'Settings'},
                    ui.icon{image = 'interface/icons/coin.png', size = 56, tooltip = 'Twelve gold coins'},
                    ui.badge{text = 'New', tone = 'accent', tooltip = 'Any node takes a tooltip, badges too.'},
                },
            }),
            layout.section('Component "popover"', {
                ui.popover{text = 'Wind', icon = 'interface/icons/fish.png', contentWidth = 560,
                    ui.column{gap = 12,
                        ui.label{text = 'A popover holds any child, such as these controls.'},
                        ui.slider{value = 0.4, showValue = true, onChange = function(event) self:report(string.format('The wind is %.2f.', event.value)) end},
                        ui.toggle{text = 'Gusts', onChange = function(event) self:report(event.checked and 'Gusts are on.' or 'Gusts are off.') end},
                    },
                },
            }),
            layout.section('Component "contextMenu"', {
                ui.contextMenu{id = 'slot-menu', items = {{id = 'rename', text = 'Rename', image = 'interface/icons/key.png'}, {id = 'copy', text = 'Duplicate', image = 'interface/icons/star.png'}, {id = 'erase', text = 'Erase', image = 'interface/icons/heart.png'}, {id = 'upload', text = 'Upload', enabled = false}},
                    onSelect = function(event) self:report('The context menu picked "' .. event.item .. '".') end,
                    ui.button{id = 'slot', text = 'Save slot 1', icon = 'interface/icons/potion.png', align = 'stretch'},
                },
                ui.button{text = 'Open the menu from code', variant = 'link', onClick = function(event) event.gui:command('slot-menu', 'open') end},
            }),
        },
        ui.dialog{id = 'confirm', title = 'Delete the save?', message = 'The island and everything on it will be gone.',
            buttons = {{id = 'keep', text = 'Keep it'}, {id = 'delete', text = 'Delete', variant = 'destructive'}},
            onAnswer = function(event) self:report('The dialog answered "' .. event.button .. '".') end,
            onDismiss = function() self:report('The dialog was dismissed.') end,
            ui.checkbox{text = 'Also delete the cloud copy'},
        },
        ui.dialog{id = 'terms', title = 'Before you sail', message = 'Escape does not close this dialog, so pick an answer.', dismissible = false,
            buttons = {{id = 'decline', text = 'Decline'}, {id = 'accept', text = 'Accept', variant = 'primary'}},
            onAnswer = function(event) self:report('The terms answered "' .. event.button .. '".') end,
        },
        ui.toast{id = 'notice', duration = 3},
        ui.toast{id = 'bottom', tone = 'success', position = 'bottom'},
        ui.toast{id = 'sticky', tone = 'warning', duration = 0},
    }
end

return Overlays
