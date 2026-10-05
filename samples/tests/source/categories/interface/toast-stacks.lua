-- Toasts at every position of the safe area, stacked so they never cover each other, with a queue past the limit of a stack and a dialog whose backdrop fades in and out with it.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local ToastStacks = haylen.class('ToastStacks', Test)

ToastStacks.positions = {'topStart', 'top', 'topEnd', 'bottomStart', 'bottom', 'bottomEnd'}
ToastStacks.names = {topStart = 'Top start', top = 'Top', topEnd = 'Top end', bottomStart = 'Bottom start', bottom = 'Bottom', bottomEnd = 'Bottom end'}
ToastStacks.tones = {'information', 'success', 'warning', 'danger'}
ToastStacks.messages = {'The tide is rising', 'Wood stored in the camp', 'A storm comes at dusk', 'The raft took damage', 'A gull stole a fish', 'Night falls soon'}

function ToastStacks:init(entry)
    ToastStacks.super.init(self, entry)
    self.sent = 0
end

function ToastStacks:enter()
    local buttons = {}
    for index, position in ipairs(ToastStacks.positions) do
        buttons[index] = ui.button{id = 'show-' .. position, text = ToastStacks.names[position], grow = 1, onClick = function() self:notify(position) end}
    end
    local toasts = {}
    for index, position in ipairs(ToastStacks.positions) do
        toasts[index] = ui.toast{id = position, position = position, duration = 4}
    end
    self:frame{
        hint = 'Every button adds a notice at its position, and notices at one position stack in the order they came, three at most, while the next ones wait. The burst adds six notices at the top at once.',
        focus = 'show-top',
        content = {layout.columns{
            ui.column{grow = 1, gap = 24,
                layout.section('Command "show" at every position', {ui.grid{columns = 3, gap = 12, children = buttons}}),
                layout.section('A queue', {
                    ui.row{gap = 12,
                        ui.button{id = 'burst', text = 'Six at the top', onClick = function() self:burst() end},
                        ui.button{text = 'One that stays', onClick = function(event) event.gui:set('sticky', {open = true}) end},
                        ui.button{text = 'Let it go', variant = 'link', onClick = function(event) event.gui:set('sticky', {open = false}) end},
                    },
                }),
                layout.section('A dialog and its backdrop', {
                    ui.button{id = 'ask', text = 'Open the dialog', onClick = function(event) event.gui:set('question', {open = true}) end},
                }),
            },
            ui.dialog{id = 'question', title = 'Set sail now?', message = 'The backdrop fades in and out together with the dialog.', buttons = {{id = 'wait', text = 'Wait'}, {id = 'sail', text = 'Sail', variant = 'primary'}},
                onAnswer = function(event) self:set('status', {text = 'The dialog answered "' .. event.button .. '".'}) end},
            ui.toast{id = 'sticky', text = 'This notice stays until it is let go', tone = 'warning', duration = 0},
            table.unpack(toasts),
        }},
    }
end

function ToastStacks:notify(position)
    self.sent = self.sent + 1
    local message = ToastStacks.messages[(self.sent - 1) % #ToastStacks.messages + 1]
    self.gui:command(position, 'show', {text = string.format('%d. %s', self.sent, message), tone = ToastStacks.tones[(self.sent - 1) % #ToastStacks.tones + 1]})
    self:set('status', {text = string.format('Sent %d notices.', self.sent)})
end

function ToastStacks:burst()
    for _ = 1, 6 do
        self:notify('top')
    end
end

return ToastStacks
