-- Every presentation of the navigation: a sheet that rests at its detents, a page sheet and a form sheet, a sheet that leaves the app its input, a full screen cover, a dialog with fields, a drawer from the start edge, a popover that points at its button, an alert, an action sheet, a picker that returns what the person picked and a draft that asks before it drops what nobody saved.
local async = require('async')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local Presentations = haylen.class('Presentations', Test)

Presentations.code = [[
ui.present(Queue(), {detents = {'medium', 'large'}})      -- Drag the grabber.
ui.present(Map(), {modal = false, detents = {240, 'large'}})
ui.present(Colors(), {style = 'popover', anchor = {gui = gui, id = 'pen'}})
ui.present(Menu(), {style = 'drawer'})
local color = ui.present(Picker(), {detents = {'fit'}}).result:await()
ui.presentAlert{title = 'Delete?', actions = {{id = 'keep', role = 'cancel'}, {id = 'delete', role = 'destructive'}}}
function Draft:back() return self.changed end  -- Asks first.]]

Presentations.colors = {{id = 'coral', text = 'Coral'}, {id = 'teal', text = 'Teal'}, {id = 'gold', text = 'Gold'}, {id = 'plum', text = 'Plum'}}

-- A screen with a title, a line that says what it shows and a close button, which every presentation of the test shows unless it needs its own.
local Sample = haylen.class('Sample', ui.Screen)

function Sample:init(title, text, rows)
    self.title, self.text, self.rows = title, text, rows or 0
end

function Sample:build()
    local lines = {gap = 8}
    for index = 1, self.rows do
        lines[index] = ui.label{text = 'Line ' .. index .. ' of the content, which scrolls when the presentation is shorter than it.', color = 'textMuted'}
    end
    return ui.column{gap = 12, padding = 24,
        ui.row{gap = 12,
            ui.label{text = self.title, font = 'heading', grow = 1},
            ui.button{id = 'close', text = 'Close', autofocus = true, onClick = function() self.presentation:dismiss() end},
        },
        ui.label{text = self.text},
        self.rows > 0 and ui.scroll{grow = 1, height = 0, ui.column(lines)} or nil,
    }
end

-- The colors of a pen, which hands the picked color back to whoever presented it.
local Picker = haylen.class('Picker', ui.Screen)

function Picker:build()
    local buttons = {gap = 12, wrap = true}
    for index, color in ipairs(Presentations.colors) do
        buttons[index] = ui.button{id = color.id, text = color.text, autofocus = index == 1, onClick = function()
            self.presentation:dismiss({result = {color = color.id}})
        end}
    end
    return ui.column{gap = 12, padding = 24,
        ui.label{text = 'Pick a color', font = 'heading'},
        ui.row(buttons),
    }
end

-- A draft that keeps its place while it holds text nobody saved: Cancel, Escape, the back button, a tap beside it and a drag down ask first.
local Draft = haylen.class('Draft', ui.Screen)

function Draft:init(test)
    self.test = test
    self.text = ''
end

function Draft:build()
    return ui.column{gap = 12, padding = 24,
        ui.row{gap = 12,
            ui.button{id = 'cancel', text = 'Cancel', variant = 'link', onClick = function() ui.goBack() end},
            ui.label{text = 'New note', font = 'heading', grow = 1, textAlign = 'center'},
            ui.button{id = 'save', text = 'Save', variant = 'primary', onClick = function() self.presentation:dismiss({result = self.text}) end},
        },
        ui.textArea{id = 'note', rows = 4, autofocus = true, placeholder = 'Type something, then try to close the sheet', onChange = function(event)
            self.text = event.value
        end},
    }
end

function Draft:back()
    if self.text == '' then
        return false
    end
    ui.presentAlert{title = 'Discard the note?', message = 'What you typed is not saved.', owner = self,
        actions = {{id = 'keep', text = 'Keep editing', role = 'cancel'}, {id = 'discard', text = 'Discard', role = 'destructive'}},
        onDismiss = function(event)
            if event.result == 'discard' then
                self.presentation:dismiss()
                self.test:report('The note was discarded.')
            end
        end}
    return true
end

function Presentations:enter()
    self:frame{
        hint = 'Open each presentation. Drag a sheet by its grabber or its empty parts, tap beside it, or press Escape, the east button or the Menu button of a remote to go back. A narrow window makes the popover and the action sheet rise from the bottom.',
        code = Presentations.code,
        focus = 'queue',
        content = {self:columns()},
    }
end

function Presentations:report(text)
    self:set('status', {text = text})
end

function Presentations:present(screen, options)
    options.owner = self
    options.onDetentChange = function(event) self:report('The sheet rests at detent ' .. event.detent .. '.') end
    return ui.present(screen, options)
end

function Presentations:columns()
    return layout.columns{
        ui.column{grow = 1, gap = 24,
            layout.section('Sheets', {
                ui.button{id = 'queue', text = 'Sheet at half or nearly all of the screen', align = 'stretch', onClick = function()
                    self:present(Sample('Up next', 'Drag the grabber up for the whole queue, or down to close it.', 30), {detents = {'medium', 'large'}})
                end},
                ui.button{id = 'page', text = 'Page sheet', align = 'stretch', onClick = function()
                    self:present(Sample('Ride details', 'On a wide window a page sheet rests at its detents along the bottom.', 20), {size = 'page', detents = {'medium', 'large'}})
                end},
                ui.button{id = 'form', text = 'Form sheet', align = 'stretch', onClick = function()
                    self:present(Sample('Sign in', 'On a wide window a form sheet floats as a card in the middle.'), {size = 'form'})
                end},
                ui.button{id = 'beside', text = 'Sheet beside the app', align = 'stretch', onClick = function()
                    self:present(Sample('Map layers', 'This sheet is not modal, so the buttons of the test still answer.'), {modal = false, detents = {240, 'large'}})
                end},
            }),
            layout.section('Covers and dialogs', {
                ui.button{id = 'cover', text = 'Full screen cover', align = 'stretch', onClick = function()
                    self:present(Sample('Camera', 'A cover takes the whole screen and is not dragged away.'), {style = 'fullScreen'})
                end},
                ui.button{id = 'dialog', text = 'Dialog with fields', align = 'stretch', onClick = function()
                    self:present({build = function(screen)
                        return ui.dialog{title = 'Rename the island', buttons = {{id = 'cancel', text = 'Cancel'}, {id = 'rename', text = 'Rename', variant = 'primary'}},
                            onAnswer = function(event) screen.presentation:dismiss({result = event.button}) end,
                            ui.textField{id = 'name', value = 'Tiny Island', autofocus = true},
                        }
                    end}, {style = 'dialog'})
                end},
                ui.button{id = 'drawer', text = 'Drawer from the start edge', align = 'stretch', onClick = function()
                    self:present(Sample('Places', 'A drawer slides in from the start edge, the right one in a right-to-left interface.'), {style = 'drawer'})
                end},
            }),
        },
        ui.column{grow = 1, gap = 24,
            layout.section('Choices of one control', {
                ui.button{id = 'pen', text = 'Popover of the pen', onClick = function()
                    self:present(Sample('Pen', 'A popover points at its button, and rises as a sheet on a narrow window.'), {style = 'popover', anchor = {gui = self.gui, id = 'pen'}, width = 520})
                end},
                ui.button{id = 'share', text = 'Action sheet', onClick = function()
                    ui.presentAlert{style = 'actionSheet', title = 'Share the island', anchor = {gui = self.gui, id = 'share'}, owner = self,
                        actions = {{id = 'copy', text = 'Copy the link'}, {id = 'message', text = 'Send in a message'}, {id = 'cancel', text = 'Cancel', role = 'cancel'}},
                        onDismiss = function(event) self:report('The action sheet chose "' .. event.result .. '".') end}
                end},
                ui.button{id = 'picker', text = 'Pick a color', onClick = function()
                    async.spawn(function()
                        local picked = self:present(Picker(), {detents = {'fit'}}).result:await()
                        self:report(picked and 'The picker returned "' .. picked.color .. '".' or 'The picker closed without a color.')
                    end)
                end},
            }),
            layout.section('Interruptions', {
                ui.button{id = 'alert', text = 'Delete the island', variant = 'destructive', onClick = function()
                    ui.presentAlert{title = 'Delete the island?', message = 'Everything on it will be gone.', owner = self,
                        actions = {{id = 'keep', text = 'Keep it', role = 'cancel'}, {id = 'delete', text = 'Delete', role = 'destructive'}},
                        onDismiss = function(event) self:report('The alert chose "' .. event.result .. '".') end}
                end},
                ui.button{id = 'draft', text = 'Write a note that asks before it closes', onClick = function()
                    self:present(Draft(self), {detents = {'large'}, grabber = true, onDismiss = function(event)
                        if event.result then
                            self:report('The note "' .. event.result .. '" was saved.')
                        end
                    end})
                end},
            }),
        },
    }
end

return Presentations
