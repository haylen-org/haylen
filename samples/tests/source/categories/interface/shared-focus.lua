-- A page and a player bar mounted as GUIs of their own that share the focus, so a remote or a gamepad moves between them, a sheet that covers the content of a scroll and a form that keeps its maximum width inside a scroll.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')

local SharedFocus = haylen.class('SharedFocus', Test)

SharedFocus.tracks = {'Morning tide', 'Paper lanterns', 'Slow river', 'Copper sky'}

function SharedFocus:enter()
    self.track = 1
    self.playing = false
    self:frame{
        hint = 'Press down from the form to reach the bar at the bottom right, a GUI of its own that shares the focus with this page. The sheet covers the scrolling form.',
        focus = 'name',
        content = {ui.scroll{grow = 1, height = 0, self:form()}},
        overlay = {self:sheet()},
    }
    self.gui.sharedFocus = true
    self.bar = ui.mount(self:barTree(), {owner = self, layer = 1, sharedFocus = true})
    self:refresh()
end

function SharedFocus:form()
    local rows = {}
    for index = 1, 12 do
        rows[index] = ui.checkbox{id = 'option' .. index, text = 'Option ' .. index .. ' of the form'}
    end
    return ui.column{maxWidth = 560, gap = 12,
        ui.sectionTitle{text = 'A form that keeps its maximum width of 560 units'},
        ui.formField{label = 'Name', ui.textField{id = 'name', value = 'Rui'}},
        ui.button{id = 'open', text = 'Open the sheet', variant = 'primary', onClick = function() self:setSheet(true) end},
        ui.column{gap = 8, children = rows},
    }
end

function SharedFocus:sheet()
    return ui.panel{id = 'sheet', visible = false, anchor = 'stretchBottom', height = 220, focusScope = true, onCancel = function() self:setSheet(false) end,
        ui.column{gap = 12,
            ui.label{text = 'This sheet comes after the scroll in the tree, so it covers the form.', font = 'heading'},
            ui.label{text = 'The focus stays in the sheet until it closes.', color = 'textMuted'},
            ui.button{id = 'close', text = 'Close the sheet', onClick = function() self:setSheet(false) end},
        },
    }
end

function SharedFocus:barTree()
    return ui.panel{anchor = 'bottomRight', margin = 24, padding = 12,
        ui.row{gap = 12,
            ui.button{id = 'previous', text = 'Previous', onClick = function() self:skip(-1) end},
            ui.button{id = 'play', text = 'Play', variant = 'primary', onClick = function() self:toggle() end},
            ui.button{id = 'next', text = 'Next', onClick = function() self:skip(1) end},
            ui.label{id = 'track', text = '', width = 220},
        },
    }
end

function SharedFocus:setSheet(open)
    self.gui:set('sheet', {visible = open})
    self.gui:command(open and 'close' or 'open', 'focus')
end

function SharedFocus:skip(step)
    self.track = (self.track + step - 1) % #SharedFocus.tracks + 1
    self:refresh()
end

function SharedFocus:toggle()
    self.playing = not self.playing
    self:refresh()
end

function SharedFocus:refresh()
    self.bar:set('play', {text = self.playing and 'Pause' or 'Play'})
    self.bar:set('track', {text = (self.playing and 'Playing ' or 'Paused on ') .. SharedFocus.tracks[self.track]})
    self:status(string.format('Track %d of %d, %s', self.track, #SharedFocus.tracks, self.playing and 'playing' or 'paused'))
end

return SharedFocus
