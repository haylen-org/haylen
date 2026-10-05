-- The nearest control in a direction, neighbours named by id, a focus scope that keeps the focus until it is left, a row that wraps and going back, with the bindings of keyboards, gamepads and TV remotes.
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')
local window = require('haylen.window')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local FocusNavigation = haylen.class('FocusNavigation', Test)

FocusNavigation.help = [==[
[b]How the focus moves[/b]
[ul]
A direction goes to the nearest control that way, preferring controls in line with the focused one.
The properties [color=gold]focusLeft[/color], [color=gold]focusRight[/color], [color=gold]focusUp[/color] and [color=gold]focusDown[/color] name the neighbour instead.
A node with [color=gold]focusScope[/color] keeps the focus until the player leaves it, and hears cancel.
The property [color=gold]focusWrap[/color] wraps a move off one end of a row or a column to its other end.
Cancel reaches the innermost scope, and then the root of the document, which goes back to the test list.
The ring shows once the player navigates, and always on a TV, where there is no pointer.
[/ul]]==]

function FocusNavigation:init(entry)
    FocusNavigation.super.init(self, entry)
    self.last = ''
end

function FocusNavigation:enter()
    self:frame{
        hint = 'Keyboard: arrows move, Tab walks in drawing order, Enter or Space presses. Gamepad: the directional pad or left stick moves and south presses. Apple TV: swipe or click the edges of the remote and click to press. Android TV: the directional pad moves and select presses.',
        focus = 'n5',
        content = {layout.columns{
            ui.column{grow = 3, gap = 24, layout.columns{self:neighbours(), self:explicit()}, layout.columns{self:scope(), self:wrap()}},
            ui.card{grow = 2, ui.richText{text = FocusNavigation.help}},
        }},
    }
end

function FocusNavigation:pressed(event)
    self.last = ' Pressed "' .. event.id .. '".'
end

-- Buttons of different sizes and places, so the nearest control in each direction is easy to follow.
function FocusNavigation:neighbours()
    local press = function(event) self:pressed(event) end
    local function cell(id, width, align)
        return ui.button{id = id, text = id:upper(), width = width, align = align, onClick = press}
    end
    return layout.section('Nearest control', {grow = 1,
        ui.row{gap = 16, cell('n1', 110), cell('n2', 200), cell('n3', 110, 'end')},
        ui.row{gap = 16, cell('n4', 170), cell('n5', 110), cell('n6', 140)},
        ui.row{gap = 16, cell('n7', 110), ui.spacer{width = 150}, cell('n8', 160)},
    })
end

function FocusNavigation:explicit()
    local press = function(event) self:pressed(event) end
    return layout.section('Explicit neighbours', {grow = 1,
        ui.label{text = 'Right from A skips to C, and down from any of them goes to the scope entry.', color = 'textMuted'},
        ui.row{gap = 16,
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
            event.document:command('scoped-1', 'focus')
        end},
        ui.panel{id = 'scope', focusScope = true, gap = 12, onCancel = function(event)
            event.document:command('enter-scope', 'focus')
            self.last = ' Left the scope with cancel.'
        end,
            ui.label{text = 'Moves stay inside this panel. Cancel leaves it.', color = 'textMuted'},
            ui.row{gap = 12, ui.button{id = 'scoped-1', text = 'Inside 1', onClick = press}, ui.button{id = 'scoped-2', text = 'Inside 2', onClick = press}},
            ui.button{id = 'leave-scope', text = 'Leave', variant = 'link', onClick = function(event)
                event.document:command('enter-scope', 'focus')
            end},
        },
    })
end

function FocusNavigation:wrap()
    local cards = {}
    for index = 1, 5 do
        cards[index] = ui.button{id = 'w' .. index, text = index, width = 80, onClick = function(event) self:pressed(event) end}
    end
    return layout.section('Wrapping row', {grow = 1,
        ui.label{text = 'Right from 5 wraps to 1, and left from 1 to 5.', color = 'textMuted'},
        ui.row{gap = 12, focusWrap = 'horizontal', children = cards},
    })
end

-- Shows where the focus is, whether its ring shows and which device the player used last.
function FocusNavigation:update(dt)
    FocusNavigation.super.update(self, dt)
    local _, id = ui.focused()
    local device = window.hasPointerDevice() and input.lastDevice() or 'remote'
    self:status(string.format('Focus on "%s", ring %s, last device "%s".%s', tostring(id), ui.focusRingVisible() and 'shown' or 'hidden', device, self.last))
end

return FocusNavigation
