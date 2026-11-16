-- Toasts at every position of the safe area, as lists or decks that never cover themselves, with a queue past the limit of a list, a snackbar whose action undoes a deletion, banners that drop in under the status bar with a picture, a priority that replaces the snackbar that shows, the variants, indicators, actions and close buttons of a message, a message that follows an upload, the events of every message and an alert whose backdrop fades in and out with it.
local haylen = require('haylen')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local Messages = haylen.class('Messages', Test)

Messages.code = [[
ui.showMessage{text = 'Wood stored in the camp', tone = 'success', indicator = 'icon', owner = self}
ui.showMessage{style = 'snackbar', text = 'The map was deleted.', action = 'Undo', onAction = function() restore() end}
ui.showMessage{style = 'banner', title = 'Ana', text = 'The raft is ready.', icon = 'interface/images/avatar-ana.png', onTap = open}
local upload = ui.showMessage{text = 'Uploading', indicator = 'progress', duration = 0}
upload:update{progress = 0.5}  local reason = upload.result:await()  -- Why it left, such as "swiped".]]

Messages.positions = {'topStart', 'top', 'topEnd', 'bottomStart', 'bottom', 'bottomEnd'}
Messages.names = {topStart = 'Top start', top = 'Top', topEnd = 'Top end', bottomStart = 'Bottom start', bottom = 'Bottom', bottomEnd = 'Bottom end'}
Messages.tones = {'information', 'success', 'warning', 'danger', 'neutral', 'accent'}
Messages.texts = {'The tide is rising', 'Wood stored in the camp', 'A storm comes at dusk', 'The raft took damage', 'A gull stole a fish', 'Night falls soon'}

function Messages:init(entry)
    Messages.super.init(self, entry)
    self.sentCount = 0
    self.numbers = setmetatable({}, {__mode = 'k'})
    self.shownMessages = {}
    self.arrangement = {arrangement = 'deck', expandable = true}
end

function Messages:enter()
    local buttons = {}
    for index, position in ipairs(Messages.positions) do
        buttons[index] = ui.button{id = 'show-' .. position, text = Messages.names[position], width = 200, onClick = function() self:notify(position) end}
    end
    self:frame{
        hint = 'Every button shows a message. A list of toasts shows three at most while the others wait, a deck stacks the newest in front, and a snackbar or a banner shows one at a time. A stack waits while the pointer or a finger rests on it, a tap lets a message go and a sideways drag swipes it away.',
        code = Messages.code,
        focus = 'show-top',
        content = {layout.columns{
            ui.column{grow = 1, gap = 24,
                layout.section('Toasts at every position', {
                    ui.segmentedControl{id = 'arrangement', items = {{id = 'list', text = 'List'}, {id = 'deck', text = 'Deck that opens'}, {id = 'depth', text = 'Deck'}}, selected = 'deck', onChange = function(event) self:arrange(event.value) end},
                    ui.panel{height = 220, justify = 'spaceBetween', padding = 16,
                        ui.row{justify = 'spaceBetween', buttons[1], buttons[2], buttons[3]},
                        ui.label{text = 'The screen', align = 'center', color = 'textMuted'},
                        ui.row{justify = 'spaceBetween', buttons[4], buttons[5], buttons[6]},
                    },
                }),
                layout.section('Snackbars and banners', {
                    ui.row{wrap = true, gap = 12,
                        ui.button{id = 'undo', text = 'Delete the map', onClick = function() self:deleteMap() end},
                        ui.button{id = 'banner', text = 'A new message', onClick = function() self:showBanner() end},
                        ui.button{id = 'urgent', text = 'An urgent snackbar', onClick = function() self:show({style = 'snackbar', text = 'The storm is here. Head to the camp.', tone = 'danger', indicator = 'icon', priority = 'high'}) end},
                        ui.button{id = 'snackbars', text = 'Three snackbars', onClick = function() self:queueSnackbars() end},
                    },
                }),
                layout.section('An alert and its backdrop', {grow = 1,
                    ui.button{id = 'ask', text = 'Open the alert', onClick = function()
                        ui.presentAlert{title = 'Set sail now?', message = 'The backdrop fades in and out together with the alert.', owner = self,
                            actions = {{id = 'wait', text = 'Wait', role = 'cancel'}, {id = 'sail', text = 'Sail'}},
                            onDismiss = function(event) self:set('status', {text = 'The alert answered "' .. event.result .. '".'}) end}
                    end},
                }),
            },
            ui.column{grow = 1, gap = 24,
                layout.section('Variants, indicators and actions', {
                    ui.grid{columns = 3, gap = 12,
                        ui.button{id = 'solid', text = 'Solid with an icon', onClick = function() self:show({text = 'Operation completed', tone = 'success', indicator = 'icon', position = 'top'}) end},
                        ui.button{id = 'soft', text = 'Soft with a title', onClick = function() self:show({title = 'Warning', text = 'You are not at the camp. Try again.', tone = 'warning', variant = 'soft', indicator = 'icon', position = 'topEnd'}) end},
                        ui.button{id = 'outline', text = 'Action and close', onClick = function() self:show({text = 'The map was deleted.', tone = 'danger', variant = 'outline', indicator = 'icon', action = 'Undo', closable = true, duration = 0, position = 'bottomEnd'}) end},
                        ui.button{id = 'busy', text = 'Busy', onClick = function() self:show({text = 'Halfway there...', indicator = 'busy'}) end},
                        ui.button{id = 'upload', text = 'Upload with progress', onClick = function() self:upload() end},
                    },
                }),
                layout.section('A queue', {grow = 1,
                    ui.row{wrap = true, gap = 12,
                        ui.button{id = 'burst', text = 'Six at the top', onClick = function() self:burst() end},
                        ui.button{id = 'stay', text = 'One that stays', onClick = function() self:showSticky() end},
                        ui.button{text = 'Let it go', variant = 'link', onClick = function() self:releaseSticky() end},
                        ui.button{id = 'clear', text = 'Dismiss every message', variant = 'link', onClick = function() self:clear() end},
                    },
                }),
            },
        }},
    }
    -- A deck of two toasts shows as the test opens, so the stacks show from the first frame in the room of one toast.
    self:show({text = 'Wood stored in the camp', tone = 'success', indicator = 'icon', duration = 8, position = 'topEnd', arrangement = 'deck'})
    self:show({title = 'Weather', text = 'A storm comes at dusk', tone = 'warning', variant = 'soft', indicator = 'icon', duration = 8, position = 'topEnd', arrangement = 'deck'})
end

function Messages:report(message, what)
    self:set('status', {text = string.format('Message %d %s.', self.numbers[message], what)})
end

-- Shows a message that reports its events in the status line and ends with the test.
function Messages:show(options)
    options.owner = self
    options.onShow = function(event) self:report(event.message, 'came on screen') end
    options.onTap = function(event) self:report(event.message, 'was tapped') end
    options.onAction = options.onAction or function(event) self:report(event.message, 'ran its action') end
    options.onDismiss = function(event)
        -- The upload stops with its message, which a swipe or the button that dismisses every message may take away.
        if event.message == self.uploading then
            timer.cancel(self.uploadTimer)
        end
        self:report(event.message, 'left, ' .. event.reason)
    end
    local message = ui.showMessage(options)
    self.sentCount = self.sentCount + 1
    self.numbers[message] = self.sentCount
    self.shownMessages[#self.shownMessages + 1] = message
    return message
end

function Messages:notify(position)
    local text = Messages.texts[self.sentCount % #Messages.texts + 1]
    self:show({text = string.format('%d. %s', self.sentCount + 1, text), tone = Messages.tones[self.sentCount % #Messages.tones + 1], position = position, duration = 4, arrangement = self.arrangement.arrangement, expandable = self.arrangement.expandable})
end

function Messages:burst()
    for _ = 1, 6 do
        self:notify('top')
    end
end

-- A deck that opens spreads under the pointer, and the other deck keeps its depth while each tap takes the front toast away. The toasts that come next take the arrangement.
function Messages:arrange(choice)
    self.arrangement = {arrangement = choice == 'list' and 'list' or 'deck', expandable = choice == 'deck'}
end

-- A deletion offers its undo in a snackbar, the way a phone does after the person deletes something.
function Messages:deleteMap()
    self:show({style = 'snackbar', text = 'The map was deleted.', action = 'Undo', onAction = function(event)
        self:report(event.message, 'brought the map back')
    end})
end

-- A banner tells of a new chat message with the picture of whoever wrote, at the top under the status bar.
function Messages:showBanner()
    self:show({style = 'banner', title = 'Ana', text = 'The raft is ready. Meet me at the dock?', icon = 'interface/images/avatar-ana.png'})
end

-- Three snackbars wait their turn, the high one first and the low one last, whatever order they asked in.
function Messages:queueSnackbars()
    self:show({style = 'snackbar', text = 'Low: the fish are dry.', priority = 'low'})
    self:show({style = 'snackbar', text = 'Normal: the fire needs wood.'})
    self:show({style = 'snackbar', text = 'High: a raider is near.', priority = 'high', tone = 'danger'})
end

function Messages:showSticky()
    if self.sticky and not self.sticky.dismissed then
        return
    end
    self.sticky = self:show({text = 'This message stays until it is let go', tone = 'warning', duration = 0, position = 'bottomStart'})
end

function Messages:releaseSticky()
    if self.sticky then
        self.sticky:dismiss()
    end
end

-- One message follows an upload that fills its ring in ten steps and then turns into a success that leaves after a while.
function Messages:upload()
    if self.uploadTimer then
        timer.cancel(self.uploadTimer)
    end
    local message = self:show({text = 'Uploading the save', variant = 'outline', indicator = 'progress', progress = 0, duration = 0, position = 'bottomStart'})
    local step = 0
    self.uploading = message
    self.uploadTimer = timer.every(0.3, function()
        step = step + 1
        if step < 10 then
            message:update{progress = step / 10}
        else
            message:update{text = 'Save uploaded', tone = 'success', variant = 'solid', indicator = 'icon', duration = 3}
        end
    end, {owner = self, count = 10})
end

function Messages:clear()
    for _, message in ipairs(self.shownMessages) do
        message:dismiss()
    end
    self.shownMessages = {}
end

return Messages
