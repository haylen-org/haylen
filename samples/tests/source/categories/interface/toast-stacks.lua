-- Toasts at every position of the safe area, as lists or decks that never cover themselves, with a queue past the limit of a list, the variants, indicators, actions and close buttons of a notice, a notice that follows an upload, the events of every notice and a dialog whose backdrop fades in and out with it.
local haylen = require('haylen')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local ToastStacks = haylen.class('ToastStacks', Test)

ToastStacks.code = [[
ui.toast{id = 'top', position = 'topEnd', arrangement = 'deck', duration = 4, onDismiss = function(event) log.info(event.notice .. ' left, ' .. event.reason .. '.') end}
gui:command('top', 'show', {notice = 'stored', text = 'Wood stored in the camp', tone = 'success', indicator = 'icon'})
gui:command('top', 'show', {text = 'The map was deleted.', tone = 'danger', variant = 'outline', action = 'Undo', closable = true, duration = 0})
gui:command('top', 'update', {notice = 'upload', progress = 0.5})  -- Fills the ring of a notice with the indicator "progress".
gui:command('top', 'hide', {})]]

ToastStacks.positions = {'topStart', 'top', 'topEnd', 'bottomStart', 'bottom', 'bottomEnd'}
ToastStacks.names = {topStart = 'Top start', top = 'Top', topEnd = 'Top end', bottomStart = 'Bottom start', bottom = 'Bottom', bottomEnd = 'Bottom end'}
ToastStacks.tones = {'information', 'success', 'warning', 'danger', 'neutral', 'accent'}
ToastStacks.messages = {'The tide is rising', 'Wood stored in the camp', 'A storm comes at dusk', 'The raft took damage', 'A gull stole a fish', 'Night falls soon'}

function ToastStacks:init(entry)
    ToastStacks.super.init(self, entry)
    self.sent = 0
end

function ToastStacks:enter()
    local buttons = {}
    for index, position in ipairs(ToastStacks.positions) do
        buttons[index] = ui.button{id = 'show-' .. position, text = ToastStacks.names[position], width = 200, onClick = function() self:notify(position) end}
    end
    local toasts = {}
    for index, position in ipairs(ToastStacks.positions) do
        toasts[index] = ui.toast{id = position, position = position, duration = 4,
            onNoticeShow = function(event) self:report(event, 'came on screen') end,
            onClick = function(event) self:report(event, 'was pressed') end,
            onAction = function(event) self:report(event, 'ran its action') end,
            onDismiss = function(event)
                -- The upload stops with its notice, which a swipe or the button that hides every notice may take away.
                if event.notice == self.uploading then
                    timer.cancel(self.uploadTimer)
                end
                self:report(event, 'left, ' .. event.reason)
            end}
    end
    self:frame{
        hint = 'Every button adds a notice at its place on the screen. A list shows three at most while the others wait, and a deck stacks the newest in front. A stack waits while the pointer or a finger rests on it, a press closes a notice and a sideways drag swipes it away.',
        code = ToastStacks.code,
        focus = 'show-top',
        content = {layout.columns{
            ui.column{grow = 1, gap = 24,
                layout.section('Command "show" at every position', {
                    ui.segmentedControl{id = 'arrangement', items = {{id = 'list', text = 'List'}, {id = 'deck', text = 'Deck that opens'}, {id = 'depth', text = 'Deck'}}, selected = 'list', onChange = function(event) self:arrange(event.value) end},
                    ui.panel{height = 240, justify = 'spaceBetween', padding = 16,
                        ui.row{justify = 'spaceBetween', buttons[1], buttons[2], buttons[3]},
                        ui.label{text = 'The screen', align = 'center', color = 'textMuted'},
                        ui.row{justify = 'spaceBetween', buttons[4], buttons[5], buttons[6]},
                    },
                }),
                layout.section('Variants, indicators and actions', {
                    ui.grid{columns = 3, gap = 12,
                        ui.button{id = 'solid', text = 'Solid with an icon', onClick = function() self:show('top', {text = 'Operation completed', tone = 'success', indicator = 'icon'}) end},
                        ui.button{id = 'soft', text = 'Soft with a title', onClick = function() self:show('topEnd', {title = 'Warning', text = 'You are not at the camp. Try again.', tone = 'warning', variant = 'soft', indicator = 'icon'}) end},
                        ui.button{id = 'outline', text = 'Action and close', onClick = function() self:show('bottomEnd', {text = 'The map was deleted.', tone = 'danger', variant = 'outline', indicator = 'icon', action = 'Undo', closable = true, duration = 0}) end},
                        ui.button{id = 'busy', text = 'Busy', onClick = function() self:show('bottom', {text = 'Halfway there...', indicator = 'busy'}) end},
                        ui.button{id = 'upload', text = 'Upload with progress', onClick = function() self:upload() end},
                    },
                }),
                layout.section('A queue', {
                    ui.row{wrap = true, gap = 12,
                        ui.button{id = 'burst', text = 'Six at the top', onClick = function() self:burst() end},
                        ui.button{text = 'One that stays', onClick = function(event) event.gui:set('sticky', {open = true}) end},
                        ui.button{text = 'Let it go', variant = 'link', onClick = function(event) event.gui:set('sticky', {open = false}) end},
                        ui.button{id = 'clear', text = 'Hide every notice', variant = 'link', onClick = function() self:clear() end},
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
    -- A deck of two notices shows as the test opens, so the stacks show from the first frame.
    self:show('topEnd', {text = 'Wood stored in the camp', tone = 'success', indicator = 'icon', duration = 8})
    self:show('topEnd', {title = 'Weather', text = 'A storm comes at dusk', tone = 'warning', variant = 'soft', indicator = 'icon', duration = 8})
end

function ToastStacks:report(event, what)
    self:set('status', {text = string.format('Notice %s at the %s %s.', event.notice:sub(2), (ToastStacks.names[event.id] or event.id):lower(), what)})
end

function ToastStacks:show(position, notice)
    self.sent = self.sent + 1
    notice.notice = 'n' .. self.sent
    self.gui:command(position, 'show', notice)
end

function ToastStacks:notify(position)
    local message = ToastStacks.messages[self.sent % #ToastStacks.messages + 1]
    self:show(position, {text = string.format('%d. %s', self.sent + 1, message), tone = ToastStacks.tones[self.sent % #ToastStacks.tones + 1]})
end

function ToastStacks:burst()
    for _ = 1, 6 do
        self:notify('top')
    end
end

-- A deck that opens spreads under the pointer, and the other deck keeps its depth while each press takes the front notice away.
function ToastStacks:arrange(choice)
    for _, position in ipairs(ToastStacks.positions) do
        self:set(position, {arrangement = choice == 'list' and 'list' or 'deck', expandable = choice == 'deck'})
    end
end

-- One notice follows an upload that fills its ring in ten steps and then turns into a success that leaves after a while.
function ToastStacks:upload()
    if self.uploadTimer then
        timer.cancel(self.uploadTimer)
    end
    self:show('bottomStart', {text = 'Uploading the save', variant = 'outline', indicator = 'progress', progress = 0, duration = 0})
    local id = 'n' .. self.sent
    local step = 0
    self.uploading = id
    self.uploadTimer = timer.every(0.3, function()
        step = step + 1
        if step < 10 then
            self.gui:command('bottomStart', 'update', {notice = id, progress = step / 10})
        else
            self.gui:command('bottomStart', 'update', {notice = id, text = 'Save uploaded', tone = 'success', variant = 'solid', indicator = 'icon', duration = 3})
        end
    end, {owner = self, count = 10})
end

function ToastStacks:clear()
    for _, position in ipairs(ToastStacks.positions) do
        self.gui:command(position, 'hide', {})
    end
end

return ToastStacks
