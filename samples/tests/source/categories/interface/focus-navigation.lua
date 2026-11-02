-- The nearest control in a direction, neighbours named by id, a focus scope that keeps the focus until it is left, a row that wraps and draws its focus its own way, and going back, with the bindings of keyboards, gamepads and TV remotes.
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')
local window = require('haylen.window')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local FocusNavigation = haylen.class('FocusNavigation', Test)

FocusNavigation.code = [[
ui.button{id = 'a', text = 'A', focusRight = 'c', focusDown = 'enter-scope'}
ui.panel{focusScope = true, onCancel = function(event) event.gui:command('enter-scope', 'focus') end, insideButtons}
ui.row{focusWrap = 'horizontal', focusRing = false, children = cards}
ui.button{id = 'w1', text = '1', onFocus = function(event) event.gui:set('w1', {variant = 'primary'}) end}
local focus = ui.focused()  -- The GUI and the id of the focused node.]]

FocusNavigation.help = [==[
[ul]
A direction goes to the nearest control that way, preferring controls in line with the focused one.
[code]focusLeft[/code], [code]focusRight[/code], [code]focusUp[/code] and [code]focusDown[/code] name a neighbour instead.
[code]focusScope[/code] keeps the focus inside a node until the player cancels.
[code]focusWrap[/code] wraps a move off one end of a row to its other end.
[code]focusRing = false[/code] lets a node draw its own focus from its focus and blur events.
The ring shows once the player navigates, and always on a TV.
[/ul]]==]

FocusNavigation.devices = {keyboardMouse = 'the keyboard and mouse', gamepad = 'a gamepad', touch = 'a touch screen'}

function FocusNavigation:init(entry)
    FocusNavigation.super.init(self, entry)
    self.last = ''
end

function FocusNavigation:enter()
    self:frame{
        hint = 'Keyboard: arrows move, Tab walks in drawing order, Enter or Space presses. Gamepad: the directional pad or left stick moves and south presses. Apple TV: swipe or click the edges of the remote and click to press. Android TV: the directional pad moves and select presses.',
        code = FocusNavigation.code,
        focus = 'n5',
        content = {layout.columns{
            ui.column{grow = 3, gap = 24, layout.columns{grow = 0, self:neighbours(), self:explicit()}, layout.columns{grow = 0, self:scope(), self:wrap()}},
            layout.section('How the focus moves', {grow = 2, ui.richText{text = FocusNavigation.help}}),
        }},
    }
end

function FocusNavigation:pressed(event)
    self.last = ' You pressed ' .. event.id:upper() .. '.'
end

-- A grid of buttons, where each direction moves to the next button that way.
function FocusNavigation:neighbours()
    local cells = {}
    for index = 1, 8 do
        local id = 'n' .. index
        cells[index] = ui.button{id = id, text = id:upper(), align = 'stretch', onClick = function(event) self:pressed(event) end}
    end
    return layout.section('Nearest control', {grow = 1, ui.grid{columns = 4, gap = 16, children = cells}})
end

function FocusNavigation:explicit()
    local press = function(event) self:pressed(event) end
    return layout.section('Explicit neighbours', {grow = 1,
        ui.label{text = 'Right from A skips to C, and down from any of them goes to the scope entry.', color = 'textMuted'},
        ui.row{wrap = true, gap = 16,
            ui.button{id = 'a', text = 'A', width = 120, focusRight = 'c', focusDown = 'enter-scope', onClick = press},
            ui.button{id = 'b', text = 'B', width = 120, focusDown = 'enter-scope', onClick = press},
            ui.button{id = 'c', text = 'C', width = 120, focusLeft = 'a', focusDown = 'enter-scope', onClick = press},
        },
    })
end

-- A panel that keeps the focus. Cancel inside it returns the focus to the button that entered it.
function FocusNavigation:scope()
    local press = function(event) self:pressed(event) end
    return layout.section('Focus scope', {grow = 1,
        ui.button{id = 'enter-scope', text = 'Enter the scope', onClick = function(event)
            event.gui:command('scoped-1', 'focus')
        end},
        ui.panel{id = 'scope', focusScope = true, gap = 12, onCancel = function(event)
            event.gui:command('enter-scope', 'focus')
            self.last = ' Cancel left the scope.'
        end,
            ui.label{text = 'Moves stay inside this panel. Cancel leaves it.', color = 'textMuted'},
            ui.row{wrap = true, gap = 12,
                ui.button{id = 'scoped-1', text = 'Inside 1', onClick = press},
                ui.button{id = 'scoped-2', text = 'Inside 2', onClick = press},
                ui.button{id = 'leave-scope', text = 'Leave', variant = 'link', onClick = function(event)
                    event.gui:command('enter-scope', 'focus')
                end},
            },
        },
    })
end

-- The cards of the row leave the ring out and show their focus by turning primary and growing, as a menu of a TV may.
function FocusNavigation:wrap()
    local cards = {}
    for index = 1, 5 do
        local id = 'w' .. index
        cards[index] = ui.button{id = id, text = index, width = 80, height = 64, onClick = function(event) self:pressed(event) end,
            onFocus = function(event) event.gui:set(id, {variant = 'primary', width = 96, height = 76}) end,
            onBlur = function(event) event.gui:set(id, {variant = 'default', width = 80, height = 64}) end,
        }
    end
    return layout.section('Wrapping row with its own focus', {grow = 1,
        ui.label{text = 'Right from 5 wraps to 1. The cards hide the ring and grow when they take the focus.', color = 'textMuted'},
        ui.row{gap = 12, focusWrap = 'horizontal', focusRing = false, children = cards},
    })
end

-- Shows where the focus is, whether its ring shows and which device the player used last.
function FocusNavigation:update(dt)
    FocusNavigation.super.update(self, dt)
    local focus = ui.focused()
    local device = window.hasPointerDevice() and FocusNavigation.devices[input.lastDevice()] or 'a remote'
    local place = focus and focus.id and focus.id:upper() or 'nothing'
    self:status(string.format('The focus is on %s, its ring %s, and the last device is %s.%s', place, ui.focusRingVisible() and 'shows' or 'shows once you navigate', device or 'unknown', self.last))
end

return FocusNavigation
